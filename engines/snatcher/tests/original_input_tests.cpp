/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../original_input.h"
#include <cstdio>
#include <stdexcept>
namespace {unsigned checks=0;void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}}
int main(){using namespace Snatcher;try{std::string error;
 for(unsigned flag=0;flag<2;++flag)for(unsigned x=0;x<256;++x)for(unsigned y=0;y<256;++y){
  LegacyInputHeader original;original.deviceFlag=flag!=0;original.anchorX=uint8_t(x);original.anchorY=uint8_t(y);NativeArchive write;
  check(archiveLegacyInputHeader(write,original,10)&&write.finished()&&write.bytes==std::vector<uint8_t>({uint8_t(flag),uint8_t(x),uint8_t(y)}),"opaque archive preserves every byte combination without interpretation");
  NativeArchive read(write.bytes);LegacyInputHeader restored;check(archiveLegacyInputHeader(read,restored,10)&&read.finished()&&restored.deviceFlag==original.deviceFlag&&restored.anchorX==x&&restored.anchorY==y,"opaque v10 bytes roundtrip");
  StartupSettings settings;settings.compatibility=restored;settings.controlMapping=(x+y)%6;check(originalInputResumeSupported(settings,error),"hardware flags never gate valid native resume");
  GameSaveRecord source;captureOriginalInputSettings(settings,source);StartupSettings again=originalInputSettings(source);check(again.compatibility.deviceFlag==original.deviceFlag&&again.compatibility.anchorX==x&&again.compatibility.anchorY==y&&again.controlMapping==settings.controlMapping,"original header and native archive share opaque owner");
 }
 for(unsigned version=1;version<10;++version){LegacyInputHeader original;NativeArchive write;check(archiveLegacyInputHeader(write,original,version)&&write.bytes.empty(),"legacy neutral owner keeps exact old layout");original.deviceFlag=true;original.anchorX=99;NativeArchive read(std::vector<uint8_t>{});check(archiveLegacyInputHeader(read,original,version)&&read.finished()&&neutralLegacyInputHeader(original),"pre20 archives migrate source-guarded neutral provenance");NativeArchive rejected;original.anchorY=1;check(!archiveLegacyInputHeader(rejected,original,version),"legacy export cannot discard opaque bytes");}
 for(std::vector<uint8_t> bytes: {std::vector<uint8_t>{},std::vector<uint8_t>{1},std::vector<uint8_t>{1,2},std::vector<uint8_t>{2,3,4}}){LegacyInputHeader value;value.anchorX=77;NativeArchive bad(bytes);check(!archiveLegacyInputHeader(bad,value,10)&&value.anchorX==77,"bad/truncated opaque fields reject transactionally");}
 LegacyInputHeader value;NativeArchive unknown;check(!archiveLegacyInputHeader(unknown,value,11),"future opaque schema rejected");StartupSettings bad;bad.controlMapping=6;check(!originalInputResumeSupported(bad,error),"mapping validity remains a gameplay invariant");
 std::printf("original input compatibility: %u checks,0 failures\n",checks);return 0;}catch(const std::exception& e){std::fprintf(stderr,"original input compatibility failure: %s\n",e.what());return 1;}}
