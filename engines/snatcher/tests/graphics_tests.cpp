/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../graphics.h"
#include <cstdio>

namespace {
int checks = 0;
int failures = 0;
void check(bool value, const char *name) {
    ++checks;
    if (!value) {
        ++failures;
        std::fprintf(stderr, "FAIL graphics: %s\n", name);
    }
}
bool decode(const std::vector<uint8_t> &input, size_t expected,
            std::vector<uint8_t> &output, size_t &used, std::string &error,
            size_t cap = 65536) {
    return Snatcher::decodeGraphics(input.empty() ? 0 : &input[0], input.size(),
                                    expected, cap, output, used, error);
}
}

int runGraphicsTests() {
    checks = failures = 0;
    std::vector<uint8_t> out;
    size_t used = 99;
    std::string error;
    const uint8_t literals[] = {8, 'A', 'B', 'C', 0x1f};
    check(decode(std::vector<uint8_t>(literals,literals+5),3,out,used,error), "literal stream");
    check(out == std::vector<uint8_t>(literals+1,literals+4) && used == 5, "literal output and consumption");
    const uint8_t shortRef[] = {6,'A',0x91,0x1f};
    check(decode(std::vector<uint8_t>(shortRef,shortRef+4),4,out,used,error), "overlapping short reference");
    check(out == std::vector<uint8_t>(4,'A'), "growing history");
    const uint8_t longRef[] = {6,'B',0,1,0x1f};
    check(decode(std::vector<uint8_t>(longRef,longRef+5),4,out,used,error), "long reference");
    check(out == std::vector<uint8_t>(4,'B'), "long output");
    const uint8_t run[] = {3,0xc0,1,2,3,4,5,6,7,8,0x1f};
    check(decode(std::vector<uint8_t>(run,run+11),8,out,used,error), "literal run");
    check(out.size()==8 && out[0]==1 && out[7]==8 && used==11, "run payload");
    const uint8_t empty[] = {1,0x1f,0xaa};
    check(decode(std::vector<uint8_t>(empty,empty+3),0,out,used,error), "empty declared stream");
    check(out.empty() && used==2, "trailing source not consumed");
    const uint8_t reload[] = {0,1,2,3,4,5,6,7,8,1,0x1f};
    check(decode(std::vector<uint8_t>(reload,reload+11),8,out,used,error), "flag reload");
    check(out.size()==8 && out[7]==8, "flag reload output");
    check(!decode(std::vector<uint8_t>(literals,literals+5),3,out,used,error,2), "caller output bound");
    check(out.empty() && used==0 && !error.empty(), "failure cleanup");
    check(!decode(std::vector<uint8_t>(literals,literals+5),2,out,used,error), "excess output rejected");
    check(!decode(std::vector<uint8_t>(literals,literals+5),4,out,used,error), "short output rejected");
    for (size_t n=0;n<sizeof(literals);++n)
        check(!decode(std::vector<uint8_t>(literals,literals+n),3,out,used,error), "every literal truncation");
    const uint8_t zeroRef[] = {1,0x80};
    check(!decode(std::vector<uint8_t>(zeroRef,zeroRef+2),2,out,used,error), "zero distance");
    const uint8_t pastRef[] = {1,0x91};
    check(!decode(std::vector<uint8_t>(pastRef,pastRef+2),3,out,used,error), "reference before history");
    const uint8_t truncatedLong[] = {1,0};
    check(!decode(std::vector<uint8_t>(truncatedLong,truncatedLong+2),3,out,used,error), "missing long distance");
    const uint8_t truncatedRun[] = {1,0xc0,1};
    check(!decode(std::vector<uint8_t>(truncatedRun,truncatedRun+3),8,out,used,error), "truncated run");
    check(!Snatcher::decodeGraphics(0,4,0,0,out,used,error), "null input");
    // A long-distance token independently tests its high distance bits.
    std::vector<uint8_t> distant;
    for (size_t block=0;block<32;++block) {
        distant.push_back(0);
        for (size_t i=0;i<8;++i) distant.push_back(static_cast<uint8_t>(block*8+i));
    }
    distant.push_back(3);distant.push_back(0x20);distant.push_back(0);distant.push_back(0x1f);
    check(decode(distant,259,out,used,error), "long-distance high byte");
    check(out.size()==259 && out[256]==0 && out[257]==1 && out[258]==2, "long-distance source");
    const uint8_t geometry[] = {0,2,0,0,0xff,0xf8,0x0c,0,0x12,0x34,0xff,0xf0};
    std::vector<Snatcher::FramePiece> pieces;
    check(Snatcher::readFrameGeometry(geometry,sizeof(geometry),0,0,64,pieces,error), "frame geometry");
    check(pieces.size()==1 && pieces[0].x==-16 && pieces[0].y==-8 &&
          pieces[0].width==32 && pieces[0].height==8 && pieces[0].attributeWord==0x1234,
          "signed frame coordinates and size bits");
    for (size_t n=0;n<sizeof(geometry);++n)
        check(!Snatcher::readFrameGeometry(geometry,n,0,0,64,pieces,error), "all frame truncations");
    check(!Snatcher::readFrameGeometry(geometry,sizeof(geometry),0,128,64,pieces,error), "frame control record rejected");
    check(!Snatcher::readFrameGeometry(geometry,sizeof(geometry),0,0,0,pieces,error), "caller piece bound");
    check(pieces.empty() && !error.empty(), "frame failure cleanup");
    check(!Snatcher::readFrameGeometry(geometry,sizeof(geometry),static_cast<size_t>(-1),0,64,pieces,error), "frame table offset overflow");
    std::vector<uint8_t> large(4+65*8,0);large[1]=2;large[3]=64;
    check(Snatcher::readFrameGeometry(&large[0],large.size(),0,0,65,pieces,error)&&pieces.size()==65,"dynamic geometry above legacy arbitrary cap");
    check(!Snatcher::readFrameGeometry(&large[0],large.size()-1,0,0,65,pieces,error)&&pieces.empty(),"dynamic geometry still checks complete resource extent");
    const uint8_t backwards[] = {0,0,0,1,0,0,0,2,0,3,0xff,0xf6};
    check(Snatcher::readFrameGeometry(backwards,sizeof(backwards),10,0,64,pieces,error) &&
          pieces.size()==1 && pieces[0].x==3 && pieces[0].y==1,
          "signed relative frame table entry");
    std::printf("graphics: %d checks, %d failures\n", checks, failures);
    return failures;
}

#ifdef SNATCHER_GRAPHICS_TEST_MAIN
#include <fstream>
#include <sstream>
#include <iterator>

int main(int argc, char **argv) {
    if (argc == 1)
        return runGraphicsTests() ? 1 : 0;
    if (argc != 4 || std::string(argv[1]) != "--batch") {
        std::fprintf(stderr, "Usage: graphics-tests [--batch MANIFEST PRIVATE_OUTDIR]\n");
        return 2;
    }
    std::ifstream manifest(argv[2]);
    if (!manifest) return 2;
    std::string line;
    unsigned count = 0;
    while (std::getline(manifest,line)) {
        if (!line.empty() && line[line.size()-1]=='\r') line.resize(line.size()-1);
        const size_t first = line.find('\t'), second = first == std::string::npos ? first : line.find('\t',first+1);
        if (first==std::string::npos || second==std::string::npos) return 2;
        const std::string input = line.substr(0,first), expectedText = line.substr(first+1,second-first-1), name = line.substr(second+1);
        if (name.empty() || name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-")!=std::string::npos || name=="." || name=="..") return 2;
        std::istringstream number(expectedText);
        size_t expected;
        char trailing;
        if (!(number>>expected) || (number>>trailing) || expected>65535) return 2;
        std::ifstream source(input.c_str(),std::ios::binary);
        if (!source) return 2;
        source.seekg(0,std::ios::end);
        const std::streamoff length=source.tellg();
        if (length<0 || length>16*1024*1024) return 2;
        source.seekg(0,std::ios::beg);
        std::vector<uint8_t> data(static_cast<size_t>(length));
        if (length && !source.read(reinterpret_cast<char *>(&data[0]),length)) return 2;
        std::vector<uint8_t> output;
        size_t consumed;
        std::string error;
        if (!Snatcher::decodeGraphics(data.empty()?0:&data[0],data.size(),expected,65535,output,consumed,error)) {
            std::fprintf(stderr,"FAIL graphics corpus %s: %s\n",name.c_str(),error.c_str());
            return 1;
        }
        const std::string target=std::string(argv[3])+"/"+name;
        std::ofstream destination(target.c_str(),std::ios::binary|std::ios::trunc);
        if (!destination) return 2;
        if (!output.empty()) destination.write(reinterpret_cast<const char *>(&output[0]),output.size());
        destination.close();
        if (!destination) return 2;
        std::printf("%s\t%zu\t%zu\n",name.c_str(),output.size(),consumed);
        ++count;
    }
    std::printf("graphics corpus: %u blocks\n",count);
    return manifest.bad() ? 2 : 0;
}
#endif
