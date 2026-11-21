/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../runtime.h"
#include "../command_input.h"
#include <fstream>
#include <iterator>
#include <cstdio>
std::vector<uint8_t> readBytes(const std::string& path){std::ifstream file(path.c_str(),std::ios::binary);return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());}
int main(int argc,char**argv){using namespace Snatcher;bool nativeInput=argc==11&&std::string(argv[10])=="--input-confirm";if(argc!=10&&!nativeInput)return 2;std::string error;std::vector<Scenario> scenarios(39);
 for(unsigned i=0;i<39;++i){char name[32];std::snprintf(name,sizeof(name),"/SP%02u.BIN",i);std::vector<uint8_t> bytes=readBytes(std::string(argv[1])+name);if(!parseScenario(bytes.data(),bytes.size(),scenarios[i],error))return 3;}
 std::vector<uint8_t> before=readBytes(argv[4]),after=readBytes(argv[5]);std::ifstream references(argv[6]);std::vector<uint16_t> expected;unsigned word;while(references>>word)expected.push_back(uint16_t(word));EventScheduler events;SemanticTrace trace;trace.enable(true);NarrativeRuntime runtime(scenarios,events,trace);
 if(before.size()!=352||after.size()!=352||expected.empty()||!runtime.start(Cursor(uint16_t(std::stoul(argv[2])),uint32_t(std::stoul(argv[3]))),before,error))return 4;
 RuntimeStatus status=runtime.run();if(status!=kRuntimeChoices||runtime.control().availableChoices()!=expected){std::fprintf(stderr,"menu mismatch: %s\n",runtime.diagnostic().c_str());return 5;}
 if(nativeInput){
  if(std::stoul(argv[7])!=1)return 6;CommandInput input;
  if(!input.open(runtime.control().availableChoices(),!runtime.control().selectedPath().empty(),0,error)||!input.setButton(kCommandConfirm,true,error))return 6;
  CommandResponse response=input.update(0);
  if(response.kind!=kCommandConfirmed||response.reference!=expected[0]||!runtime.chooseReference(response.reference,error))return 6;
  for(unsigned counter=1;counter<100;++counter){if(!input.setButton(kCommandConfirm,true,error)||input.update(uint8_t(counter)).kind!=kCommandNoResponse)return 6;}
 }else if(!runtime.choose(unsigned(std::stoul(argv[7])),error))return 6;
 status=runtime.run();if(status!=kRuntimeWaiting||runtime.packed()!=after){std::fprintf(stderr,"command/state mismatch: %s\n",runtime.diagnostic().c_str());return 7;}
 bool dialogue=false;for(const RuntimeRequest& request:runtime.requests())if(request.kind==kRequestDialogue&&request.first==std::stoul(argv[8])&&request.second==std::stoul(argv[9]))dialogue=true;
 if(!dialogue||runtime.requests().size()!=1)return 8;std::printf("{\"status\":\"PASS\",\"choices\":%zu,\"selected_reference\":%u,\"packed_bytes_exact\":352,\"dialogue_style\":%s,\"dialogue_reference\":%s}\n",expected.size(),unsigned(expected[std::stoul(argv[7])-1]),argv[8],argv[9]);return 0;
}
