/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../fm_publication.h"
#include "../fm_audio.h"
#include <cstdio>
#include <cstdlib>
using namespace Snatcher;
static unsigned checks=0;
static void check(bool value,const char* label){++checks;if(!value){std::fprintf(stderr,"FM publication failure: %s\n",label);std::exit(1);}}
int main(){std::string error;bool admitted;uint8_t command=255;
 for(unsigned id=0;id<256;++id){FmPublicationQueue queue;check(queue.post(uint8_t(id),7,admitted,error),"finite byte request");bool valid=id&&!(id>=123&&id<241);check(admitted==valid&&queue.size()==unsigned(valid),"source zero/reserved admission");check(queue.take(command)==valid,"one service consumes only admitted request");if(valid)check(command==id&&!queue.size()&&!queue.priority(),"exact identity and priority retirement");}
 FmPublicationQueue queue;check(queue.post(50,3,admitted,error)&&admitted&&queue.priority()==3,"first pending effect barrier");check(queue.post(51,2,admitted,error)&&!admitted&&queue.size()==1&&queue.priority()==3,"lower pending priority rejected");check(queue.post(79,0,admitted,error)&&admitted&&queue.priority()==3,"ordinary request preserves pending priority");check(queue.post(52,3,admitted,error)&&admitted&&queue.size()==3,"equal priority admitted and replaces barrier owner");
 check(queue.take(command)&&command==50&&queue.priority()==3,"old priority entry cannot clear later owner");check(queue.take(command)&&command==79&&queue.priority()==3,"music entry cannot clear priority owner");auto restored=queue;check(restored.take(command)&&command==52&&!restored.priority()&&queue.priority()==3,"copied semantic owner independent");check(queue.take(command)&&command==52&&!queue.priority(),"exact latest owner retires barrier");check(queue.post(51,1,admitted,error)&&admitted,"lower request admitted after barrier consumed");queue.clear();check(!queue.size()&&!queue.priority()&&!queue.take(command),"reset retires pending requests and priority");
 for(unsigned i=0;i<31;++i)check(queue.post(79,0,admitted,error)&&admitted,"finite usable ring domain");check(!queue.post(50,9,admitted,error)&&!admitted&&queue.size()==31&&!queue.priority(),"overrun preserves all old requests and priority");for(unsigned i=0;i<31;++i)check(queue.take(command)&&command==79,"overflow never aliases full queue to empty");
 for(unsigned version=1;version<=8;++version){check(fmHistoryFramesAllowed(version,0)&&fmHistoryEventsAllowed(version,0),"supported history policy");}check(fmHistoryFrameLimit(5)==fmHistoryFrameLimit(4)&&fmHistoryEventLimit(5)==fmHistoryEventLimit(4),"new publication policy keeps bounded resource domains");check(!fmHistoryFramesAllowed(9,0)&&!fmHistoryEventsAllowed(9,0),"future policy rejected");
 std::printf("FM publication: %u checks,0 failures\n",checks);return 0;}
