/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../scene_state.h"
#include "../text_entry.h"
#include "../native_visual_jobs.h"
#include <cstdio>
#include <stdexcept>
#include <fstream>
#include <iterator>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <new>
#include <cstddef>
namespace {
size_t liveHeapBytes=0;
struct alignas(std::max_align_t) AllocationHeader {size_t size;};
}
void* operator new(size_t n){auto* p=static_cast<AllocationHeader*>(std::malloc(sizeof(AllocationHeader)+(n?n:1)));if(!p)throw std::bad_alloc();p->size=n;liveHeapBytes+=n;return p+1;}
void operator delete(void* p) noexcept {if(!p)return;auto* h=static_cast<AllocationHeader*>(p)-1;liveHeapBytes-=h->size;std::free(h);}
void operator delete(void* p,size_t) noexcept {::operator delete(p);}

using namespace Snatcher;
namespace {
unsigned checks;
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
void nativeJobContracts(){
 for(bool service:{false,true})for(bool replacement:{false,true})check(nativeStartupInputReady(service,replacement)==(!service&&!replacement),"startup mouse handoff uses original coordinator admission requirements");

 std::string error;SceneRenderer renderer;SceneLayout layout;layout.width=24;layout.height=8;layout.columns=layout.rows=32;layout.planeEnabled[1]=false;
 check(renderer.configure(layout,error),"native job layout");renderer.establishBlankAssets();std::vector<uint16_t> palette(64,0);palette[1]=0xe;palette[2]=0xe0;check(renderer.setPalette(palette,0,error),"native job colors");
 GraphicsTransfer initial;initial.destination=0;initial.decoded.assign(8192,0x11);check(renderer.applyTransfer(initial,error)&&renderer.fillPlaneCells(0,1,0,1,1,1,error)&&renderer.fillPlaneCells(0,2,0,1,1,255,error),"native old body and retained tail");
 auto render=[&](){SceneImage out;check(renderer.render({},kSceneReferenceRGB565,out,error),"native job canonical image");return out;};
 const auto before=render();SceneGraphicsQueue sources;NativeVisualRequest request;request.clearFont=true;SceneGraphicsCopy copy;copy.channel=4;copy.destination=0;copy.count=32;copy.bytes.assign(32,0x22);request.copies.push_back(copy);
 NativeVisualCommit job(request);check(job.state()==NativeVisualCommit::kRequested&&!job.continuationAllowed(),"request cannot continue");
 check(!job.commit(renderer,error)&&render().rgb==before.rgb,"uncompleted job cannot publish");
 check(job.admit(sources,error)&&job.state()==NativeVisualCommit::kDataReady&&!job.continuationAllowed(),"admission resolves native ready data");
 check(job.complete(renderer,error)&&job.state()==NativeVisualCommit::kCompleted&&render().rgb==before.rgb&&!job.continuationAllowed(),"atomic native completion remains unpublished");
 check(job.commit(renderer,error)&&job.continuationAllowed(),"only committed presentation permits continuation");auto after=render();check(after.indexed[0]==2&&after.indexed[8]==0&&after.indexed[16]==1,"clear precedes copy and preserves terminal font tile");
 check(!job.cancel(error)&&job.continuationAllowed(),"cannot cancel committed work");
 const auto committed=after.rgb;NativeVisualRequest bad=request;copy.destination=1;bad.copies.push_back(copy);NativeVisualCommit failed(bad);
 check(failed.admit(sources,error)&&!failed.complete(renderer,error)&&failed.state()==NativeVisualCommit::kFailed&&!failed.continuationAllowed()&&render().rgb==committed,"bad successor cannot expose a clear or earlier copy");
 check(!failed.commit(renderer,error)&&render().rgb==committed,"failure preserves existing presentation");
 NativeVisualCommit retry(request);check(retry.admit(sources,error)&&retry.complete(renderer,error)&&retry.commit(renderer,error)&&render().rgb==committed,"corrected request retries without partial history");
 bad=request;bad.copies[0].channel=5;NativeVisualCommit notReady(bad);check(!notReady.admit(sources,error)&&notReady.state()==NativeVisualCommit::kFailed&&render().rgb==committed,"invalid native data owner fails before operation");
 NativeVisualCommit cancelled(request);check(cancelled.cancel(error)&&cancelled.state()==NativeVisualCommit::kCancelled&&!cancelled.admit(sources,error)&&!cancelled.continuationAllowed(),"cancelled request cannot be admitted or continued");
}
void word(std::vector<uint8_t>& b,unsigned p,unsigned v){b[p]=uint8_t(v>>8);b[p+1]=uint8_t(v);}
struct Source:SceneStateSource {
 std::vector<uint8_t> bytes;
 Source():bytes(64,0){bytes[1]=2;bytes[2]=0x80;bytes[3]=8;bytes[5]=2;bytes[6]=0x80;bytes[7]=32;word(bytes,32,2);bytes[34]=0x80;}
 bool loadSceneResource(uint16_t id,std::vector<uint8_t>& out,std::string&)override{if(id!=22)return false;out=bytes;return true;}
};
SceneRuntime fresh(Source& source){SceneRuntime scene;std::string error;SceneLayout layout;layout.width=256;layout.height=224;layout.columns=32;layout.rows=64;layout.planeEnabled[1]=false;
 check(scene.establishBootSubmissionHistory(error)&&scene.configure(layout,error),"cold compositor owner");scene.establishBlankAssets();scene.establishFreshModuleDialogueContext();scene.establishFreshAnimationClock();scene.establishSceneFlags(0);
 std::vector<uint16_t> colors(128,0);colors[1]=0xe;colors[2]=0xe0;
 check(scene.setPalette(colors,0,error)&&scene.setTransferChannel(0,error)&&scene.install(22,source.bytes,0,error),"source-bound stable scene");return scene;
}
GraphicsTransfer canvas(uint8_t value){GraphicsTransfer t;t.recordOffset=t.sourceOffset=t.encodedEnd=0;t.destination=t.delay=0;t.decoded.assign(8192,value);return t;}
SceneImage image(SceneRuntime& scene){std::string error;SceneImage out;check(scene.render(kSceneRGB888,out,error),"render complete canonical canvas");return out;}
void captionViewPublication(){
 Source source;SceneRuntime scene=fresh(source);std::string error;
 check(scene.beginFrame(false,error),"initial Main boundary");
 GraphicsTransfer portrait=canvas(0x11);portrait.destination=32;portrait.decoded.resize(8160);
 check(scene.applyTransfer(portrait,error)&&scene.fillPlaneCells(0,0,50,32,8,1,error),"old caption name cells overlap portrait pattern allocation");
 check(scene.requestVisualCommand(0xfe,error),"closed caption before incoming text");const auto closed=image(scene);check(std::all_of(closed.indexed.begin(),closed.indexed.end(),[](uint8_t n){return n==0;}),"closed view hides reused patterns");
 SceneRuntime eager=scene;check(eager.requestVisualCommand(0x0f,error)&&image(eager).rgb!=closed.rgb,"old eager Sub view deterministically exposes stale portrait pixels");
 check(scene.postDialogueView(0x0f,error)&&scene.fontPublicationPending()&&scene.visualBusy(),"Sub caption request awaits Main publication");
 check(scene.applyDialogueTransfers({canvas(0)},error)&&image(scene).rgb==closed.rgb,"posted canvas/view does not expose stale name-map pixels");
 check(!scene.postDialogueView(0x99,error)&&scene.fontPublicationPending(),"invalid view retains accepted request");
 check(scene.beginFrame(true,error)&&image(scene).rgb==closed.rgb&&scene.fontPublicationPending(),"pause preserves unpublished caption request");
 NativeSaveSection pending;check(!SceneStateCodec::encode(scene,pending,error),"unpublished caption view is outside stable save boundary");
 check(scene.beginFrame(false,error)&&!scene.fontPublicationPending()&&image(scene).rgb==closed.rgb,"Main accepts open view together with clean canvas");
 check(scene.postDialogueView(0x0f,error)&&scene.requestVisualCommand(0xfe,error)&&!scene.fontPublicationPending(),"later primary close cancels pending open");
}
// Exercise the second yellow-flash fix without gameplay, a window or SDL.
void captionPublicationLifetime(){
 Source source;std::string error;const size_t before=liveHeapBytes;
 {
 SceneRuntime scene=fresh(source);check(scene.beginFrame(false,error),"caption lifetime initial boundary");
 auto cycle=[&](){
  check(scene.requestVisualCommand(0xfe,error),"caption lifetime close");
  check(scene.postDialogueView(0x0f,error),"caption lifetime post");
  check(scene.applyDialogueTransfers({canvas(0)},error),"caption lifetime canvas");
  check(scene.beginFrame(true,error)&&scene.fontPublicationPending(),"caption lifetime pause retains request");
  check(scene.beginFrame(false,error)&&!scene.fontPublicationPending()&&!scene.visualBusy()&&!scene.pendingGraphicsTasks(),"caption lifetime accepts and drains once");
  {auto pixels=image(scene);check(pixels.width==256&&pixels.height==224,"caption retains display geometry");}
  NativeSaveSection first,second;
  check(SceneStateCodec::encode(scene,first,error),"caption lifetime settled state");
  check(scene.beginFrame(false,error)&&SceneStateCodec::encode(scene,second,error)&&first.bytes==second.bytes,"idle boundary cannot replay completed publication");
 };
 cycle();const size_t settled=liveHeapBytes;
 for(unsigned i=0;i<16;++i){cycle();check(liveHeapBytes==settled,"caption cycles cannot retain heap allocations");}
 std::printf("caption lifetime: 16 cycles, retained growth %zu bytes\n",liveHeapBytes-settled);
 }
 check(liveHeapBytes==before,"caption scene destruction releases its heap owners");
}
void presentationReplacement(){for(unsigned plane=0;plane<2;++plane){Source source;SceneRuntime scene=fresh(source);std::string error;SceneLayout layout;layout.width=256;layout.height=224;layout.columns=64;layout.rows=64;layout.planeEnabled[0]=plane==0;layout.planeEnabled[1]=plane==1;
 std::vector<uint16_t> colors(128,0);colors[1]=0xe;check(scene.configure(layout,error)&&scene.establishFreshPalette(error)&&scene.setPalette(colors,0,error),"direct replacement palette and plane owner");GraphicsTransfer tile=canvas(0x11);tile.destination=32;tile.decoded.resize(32);check(scene.applyTransfer(tile,error)&&scene.fillPlaneCells(plane,0,0,64,64,1,error),"inherited map selects nonblank independent pattern");const auto before=image(scene);check(before.indexed[0]==1,"positive inherited-map control");
 NativeSaveSection prior;check(SceneStateCodec::encode(scene,prior,error),"stable literal scene predates the new handoff");
 SceneRuntime queued=scene,suppressed=scene; suppressed.establishSceneFlags(2);
 check(suppressed.preparePresentationReplacement(error)&&!suppressed.blankingPending()&&image(suppressed).rgb==before.rgb,"source bit1 suppresses the entire deferred blank request");
 NativeSaveSection saved;SceneRuntime restored;check(SceneStateCodec::encode(suppressed,saved,error)&&SceneStateCodec::decode(saved,source,restored,error)&&image(restored).rgb==before.rgb,"suppressed stable replacement retains supported literal map persistence");
 SceneGraphicsTask cancelled;cancelled.flags=1;cancelled.transfers.push_back(tile);cancelled.stagingOffsets.push_back(0);cancelled.copyLengths.push_back(32);cancelled.stagingSources.push_back(std::vector<uint8_t>(32,0x22));
 check(scene.queueGraphicsTasks({cancelled},error)&&scene.pendingGraphicsTasks()==1,"source replacement begins with an unconsumed graphics program");
 check(scene.preparePresentationReplacement(error)&&!scene.resourceChangePending()&&scene.blankingPending()&&!scene.pendingGraphicsTasks()&&image(scene).rgb==before.rgb,"F5E8 cancels queued programs, retains maps now and requests their later clear");
 check(!SceneStateCodec::encode(scene,saved,error),"unposted resource handoff is not a stable scene save");
 bool admitted=true;check(scene.advanceCoordinatorClock(error,&admitted)&&!admitted,"requested blank blocks coordinator admission");
 SceneRuntime module=scene;module.resetForModule();check(module.blankingPending(),"module reconstruction preserves the actual pending clear owner");
 check(scene.commitTransfers(error)&&scene.blankingPending()&&image(scene).rgb==before.rgb,"Sub posts the request without changing displayed names");
 check(scene.commitTransfers(error)&&scene.blankingPending()&&image(scene).rgb==before.rgb,"repeated Sub calls cannot acknowledge an unconsumed Main clear");
 check(!SceneStateCodec::encode(scene,saved,error),"posted clear retains the transient handoff save restriction");
 check(scene.beginFrame(true,error)&&scene.blankingPending()&&image(scene).rgb==before.rgb,"pause preserves the unpublished request and frame");
 check(scene.beginFrame(false,error)&&scene.blankingPending()&&image(scene).indexed[0]==0,"Main clears the first4KB before Sub releases the handoff");
 check(!SceneStateCodec::encode(scene,saved,error),"Main completion alone does not invent a completed Sub visit");
 check(scene.commitTransfers(error)&&!scene.blankingPending()&&scene.advanceCoordinatorClock(error,&admitted)&&admitted,"following Sub acknowledgement releases the next coordinator visit");
 check(SceneStateCodec::encode(scene,saved,error)&&SceneStateCodec::decode(saved,source,restored,error)&&image(restored).rgb==image(scene).rgb&&!restored.blankingPending(),"stable completed handoff restores without new transient history");
 layout.sourceY[plane]=256;check(scene.configure(layout,error)&&image(scene).rgb==before.rgb,"second4KB of the actual plane allocation survives the deferred clear");
 check(SceneStateCodec::decode(prior,source,restored,error)&&!restored.blankingPending()&&image(restored).rgb==before.rgb,"prior literal archive does not acquire an invented deferred writer");
 check(queued.prepareResourceChange(error)&&queued.resourceChangePending()&&image(queued).rgb!=before.rgb,"queue0F retains its independently owned frontend handshake");
 }}
void dependentCopies(){for(unsigned policy=0;policy<4;++policy){Source source;auto& b=source.bytes;b.resize(6000,0);b[34]=9;b[35]=2;b[37]=uint8_t(policy);b[38]=1;b[39]=18;
 b[41]=2;b[42]=0x80;b[43]=128;word(b,46,0);b[49]=2;b[50]=0x80|uint8_t(5000>>8);b[51]=uint8_t(5000);word(b,54,4096);word(b,56,0xffff);b[58]=0x80;
 if(policy>=2){for(unsigned i=0;i<18;++i)b[36+i]=b[40+i];b[34]=1;b[35]=18;b[54]=9;b[55]=2;b[56]=0;b[57]=uint8_t(policy);}
 auto literals=[&](unsigned start,unsigned count,uint8_t value){word(b,start,count);unsigned at=start+2;for(unsigned group=0;group<count/8;++group){b[at++]=0;for(unsigned n=0;n<8;++n)b[at++]=value;}b[at++]=1;b[at]=0x1f;};literals(128,4096,0x11);literals(5000,32,0x22);
 SceneRuntime scene=fresh(source);std::string error;unsigned visits=0;while(scene.transferBusy()){check(++visits<=5&&scene.commitTransfers(error),"dependent worker makes bounded progress");if(scene.transferBusy()){check(scene.pendingGraphicsCopies()==1,"dependent worker queues one original copy");NativeSaveSection unsupported;check(!SceneStateCodec::encode(scene,unsupported,error),"inflight dependent task retains existing save boundary");check(scene.beginFrame(false,error)&&scene.pendingGraphicsCopies()==0,"Main consumes dependent record before successor");}}
 check(visits==(policy?4u:3u),"chunk policy controls two-kilobyte copies and a separate terminator visit");
 SceneGraphicsTask reuse;reuse.flags=0x81;reuse.transfers.push_back(canvas(0));reuse.transfers[0].destination=32;reuse.stagingOffsets.push_back(0);reuse.copyLengths.push_back(32);reuse.stagingSources.push_back({});
 check(scene.queueGraphicsTasks({reuse},error)&&scene.updateObjects(false,error)&&scene.beginFrame(false,error)&&scene.fillPlaneCells(0,0,0,32,64,1,error),"dependent data remains in cache0 for task reuse under every chunk policy");const auto frame=image(scene);check(frame.indexed[0]==2,"cache0 inherits actual last dependent decode");
 }}
void fontAliases(const char* path){std::ifstream in(path,std::ios::binary);std::vector<uint8_t> sub((std::istreambuf_iterator<char>(in)),{});Source source;SceneRuntime scene=fresh(source);DialogueWindow window;std::string error;const uint8_t body[]={'A','B',255};
 check(window.loadAssets(sub.data(),sub.size(),error)&&window.begin(body,sizeof(body),0,0,freshModuleDialogueLayout(),error)&&scene.prepareDialogue(window,error)&&window.patterns().back()==0x77,"fresh canvas retains its explicit initialization before first binding");
 check(scene.stageDialogue(window,error),"bind font and names to shared staging allocations");
 SceneGraphicsTask raw;raw.flags=0x32;raw.transfers.push_back(canvas(0));raw.transfers[0].decoded.assign(32,0x22);raw.transfers[0].destination=0x1fe0;raw.stagingOffsets.push_back(8160);raw.copyLengths.push_back(32);raw.stagingSources.push_back({});
 check(scene.queueGraphicsTasks({raw},error)&&scene.updateObjects(false,error)&&scene.prepareDialogue(window,error)&&window.patterns().back()==0x22,"later channel3 worker is visible to font producer");
 raw.flags=0x22;raw.transfers[0].decoded={0x80,0x2a};raw.transfers[0].destination=0xee7e;raw.stagingOffsets[0]=0x11fe;raw.copyLengths[0]=2;
 check(scene.queueGraphicsTasks({raw},error)&&scene.updateObjects(false,error)&&scene.prepareDialogue(window,error),"names share the cache2 window after a later task");std::vector<GraphicsTransfer> out;check(window.transfers(out,error)&&out[1].decoded[510]==0x80&&out[1].decoded[511]==0x2a,"font names retain the actual shared writer");
 check(window.tick(false,error)&&scene.stageDialogue(window,error)&&window.patterns().back()==0x22,"glyph work preserves unrelated shared cache bytes");
 while(window.takeViewAction()!=kDialogueNoViewAction){}std::vector<uint8_t> saved;DialogueWindow restored;check(SceneStateCodec::encodeDialogue(window,saved,error)&&SceneStateCodec::decodeDialogue(saved,sub,restored,error)&&scene.prepareDialogue(restored,error)&&restored.patterns()==window.patterns(),"SDZ retains bound shared canvas continuation");
 saved[3]='X';saved.resize(saved.size()-29);check(SceneStateCodec::decodeDialogue(saved,sub,restored,error)&&scene.prepareDialogue(restored,error)&&restored.patterns()==window.patterns(),"legacy SDX keeps its literal canvas without inventing old cache binding");check(scene.stageDialogue(restored,error)&&scene.prepareDialogue(restored,error)&&restored.patterns()==window.patterns(),"real legacy producer acquires the shared owner");
 SceneRuntime partial=fresh(source);DialogueWindow text;check(text.loadAssets(sub.data(),sub.size(),error)&&text.begin(body,sizeof(body),0,0,freshModuleDialogueLayout(),error),"independent row publication fixture");
 check(partial.applyTransfer(canvas(0x11),error),"displayed font initially contains prior allocation");const auto prior=image(partial);
 check(partial.stageDialogue(text,error)&&image(partial).rgb==prior.rgb,"new canvas producer cannot clear displayed font immediately");NativeSaveSection clearSave;SceneRuntime clearRestored;check(SceneStateCodec::encode(partial,clearSave,error)&&SceneStateCodec::decode(clearSave,source,clearRestored,error),"pending Main font clear is supported source-bound graphics state");
 check(partial.beginFrame(false,error)&&clearRestored.beginFrame(false,error)&&image(partial).rgb==image(clearRestored).rgb&&image(partial).rgb!=prior.rgb,"Main font clear and terminal fill survive restoration at their real publication boundary");
 raw.flags=2;raw.transfers[0].decoded.assign(32,0x22);raw.transfers[0].destination=4096;raw.stagingOffsets[0]=0;raw.copyLengths[0]=32;
 check(partial.queueGraphicsTasks({raw},error)&&partial.updateObjects(false,error)&&partial.beginFrame(false,error)&&partial.fillPlaneCells(0,0,0,32,64,128,error),"unrelated task owns a displayed tile outside the next glyph row");const auto untouched=image(partial);
 for(unsigned i=0;i<2;++i){check(partial.prepareDialogue(text,error)&&text.tick(false,error),"real glyph consumer advances");if(i==1){std::vector<uint8_t> dirty;DialogueWindow resumed;SceneRuntime continued=partial;check(SceneStateCodec::encodeDialogue(text,dirty,error)&&SceneStateCodec::decodeDialogue(dirty,sub,resumed,error)&&continued.stageDialogue(resumed,error)&&continued.beginFrame(false,error)&&image(continued).rgb==untouched.rgb,"unpublished dirty glyph rows survive dialogue restoration");}check(partial.stageDialogue(text,error)&&partial.beginFrame(false,error)&&image(partial).rgb==untouched.rgb,"glyph producer must not repost untouched canvas rows over another displayed owner");}
 check(text.revealedGlyphs()==1,"positive control rasterized a real glyph while retaining the unrelated displayed tile");
 for(unsigned variant=0;variant<3;++variant){SceneRuntime prompt=fresh(source);DialogueWindow dialog;DialogueLayout context=freshModuleDialogueLayout();context.variant=uint8_t(variant);check(dialog.loadAssets(sub.data(),sub.size(),error)&&dialog.begin(body,sizeof(body),0,3,context,error),"source variant prompt owner");for(unsigned i=0;!dialog.waiting()&&i<200;++i){check(prompt.prepareDialogue(dialog,error)&&dialog.tick(false,error)&&prompt.stageDialogue(dialog,error)&&prompt.beginFrame(false,error)&&prompt.updateObjects(false,error),"establish actual waiting state through normal dialogue producer");}check(dialog.waiting(),"named dialogue reaches waiting state without injection");std::vector<GraphicsTransfer> base;check(dialog.transfers(base,error),"capture base name canvas before prompt");check(dialog.tickPrompt(error),"prompt coordinator advances original timeline");std::vector<uint8_t> dirty;DialogueWindow resumed;check(SceneStateCodec::encodeDialogue(dialog,dirty,error)&&SceneStateCodec::decodeDialogue(dirty,sub,resumed,error),"unconsumed prompt task outbox survives dialogue restoration");check(prompt.stageDialogue(resumed,error)&&prompt.pendingGraphicsTasks()==1,"prompt posts one two-record raw task before graphics scan");check(prompt.updateObjects(false,error)&&!prompt.pendingGraphicsTasks(),"raw prompt task publishes both source records in one graphics visit");NativeSaveSection copies;SceneRuntime loaded;check(SceneStateCodec::encode(prompt,copies,error)&&SceneStateCodec::decode(copies,source,loaded,error)&&prompt.beginFrame(false,error)&&loaded.beginFrame(false,error)&&image(prompt).rgb==image(loaded).rgb,"typed cache1 prompt copies survive Main publication restoration");check(prompt.prepareDialogue(resumed,error),"font continuation reopens its base name cache");std::vector<GraphicsTransfer> after;check(resumed.transfers(after,error)&&after[1].decoded==base[1].decoded,"prompt task changes displayed name cells without rewriting cache2 base names");}

 SceneRuntime common=fresh(source);check(common.establishFreshPalette(error),"common packet has actual fresh application palette ownership");std::vector<uint16_t> colors(128);const uint16_t spectrum[]={0,0x00e,0x0e0,0xe00,0xeee,0x0ee,0xe0e,0xee0};for(unsigned i=0;i<128;++i)colors[i]=spectrum[i&7];check(common.setPalette(colors,0,error),"common asset positive-control colors");
 raw.flags=0x12;raw.transfers[0].destination=0x2000;raw.transfers[0].decoded.assign(7680,0x33);raw.stagingOffsets[0]=512;raw.copyLengths[0]=7680;
 check(common.queueGraphicsTasks({raw},error)&&common.updateObjects(false,error)&&common.beginFrame(false,error),"prior application owns the cache1 tail before common initialization");
 NativeSaveSection initialCommon,afterBad;check(SceneStateCodec::encode(common,initialCommon,error),"snapshot before invalid common source packet");auto altered=sub;altered[0xecb6-0xd400]=0x42;check(!common.queueScenarioGraphicsInitialization(altered.data(),altered.size(),error)&&!common.queueScenarioGraphicsInitialization(sub.data(),32,error)&&SceneStateCodec::encode(common,afterBad,error)&&initialCommon.bytes==afterBad.bytes,"invalid common source metadata rejects transactionally before application clear");
 check(common.queueScenarioGraphicsInitialization(sub.data(),sub.size(),error)&&common.pendingGraphicsTasks()==2,"authored common packet queues atlas and cursor in distinct cache owners");check(!common.queueScenarioGraphicsInitialization(sub.data(),sub.size(),error)&&common.pendingGraphicsTasks()==2,"initialization boundary rejects replacement of unconsumed jobs");NativeSaveSection notReady;check(!SceneStateCodec::encode(common,notReady,error),"pending common programs retain existing save restriction");
 check(common.updateObjects(false,error)&&!common.pendingGraphicsTasks()&&common.pendingGraphicsCopies()==2,"parallel atlas and ordinary cursor complete through one normal task scan");NativeSaveSection completed;SceneRuntime restoredCommon;check(SceneStateCodec::encode(common,completed,error)&&SceneStateCodec::decode(completed,source,restoredCommon,error)&&common.beginFrame(false,error)&&restoredCommon.beginFrame(false,error),"completed common source references restore before Main publication");
 DialogueWindow menu;std::vector<uint8_t> labels(0x3810,255);labels[0x3800]='A';std::vector<PlacedPiece> pieces;check(menu.loadAssets(sub.data(),sub.size(),error)&&menu.beginMenu(labels.data(),labels.size(),{0},0,freshModuleDialogueLayout(),error)&&menu.menuPieces(pieces,error)&&common.setMenuCursorPieces(pieces,error),"common cursor uses original menu geometry");
 for(unsigned i=0;i<3;++i)check(common.beginFrame(false,error)&&common.updateObjects(false,error)&&common.commitEnvironmentFrame(uint16_t(i),error),"cursor uses existing submission and display boundaries");const auto visible=image(common);GraphicsTransfer erase=canvas(0);erase.destination=0xde00;erase.decoded.assign(512,0);check(common.applyTransfer(erase,error)&&image(common).rgb!=visible.rgb,"positive control removes actual cursor pixels without changing its staging owner");
 SceneGraphicsTask reuse;reuse.flags=0x91;reuse.transfers.push_back(erase);reuse.stagingOffsets.push_back(0);reuse.copyLengths.push_back(512);reuse.stagingSources.push_back({});check(common.queueGraphicsTasks({reuse},error)&&common.updateObjects(false,error)&&common.beginFrame(false,error)&&image(common).rgb==visible.rgb,"common initializer established cache1 cursor bytes rather than only reposting visible pixels");
 SceneRuntime expected=common;erase.decoded.assign(512,0x33);check(expected.applyTransfer(erase,error)&&image(expected).rgb!=visible.rgb,"tail-history positive control changes cursor shape and color");reuse.stagingOffsets[0]=512;check(common.queueGraphicsTasks({reuse},error)&&common.updateObjects(false,error)&&common.beginFrame(false,error)&&image(common).rgb==image(expected).rgb,"common cursor prefix retains prior cache1 tail bytes exactly");


 SceneRuntime taskScene=fresh(source);std::vector<uint16_t> taskColors(128,0);taskColors[1]=0xe;taskColors[2]=0xe0;for(unsigned i=64;i<128;++i)taskColors[i]=0x222;check(taskScene.setPalette(taskColors,0,error),"distinct primary and secondary task palette ownership");DialogueWindow task;check(task.loadAssets(sub.data(),sub.size(),error)&&task.beginTask(body,sizeof(body),0,3,freshModuleDialogueLayout(),error),"authored dialogue task begins without executing its font producer early");size_t nameGlyphs=0;unsigned firstWaiting=0;
 for(unsigned frame=0;frame<14;++frame){check(taskScene.beginFrame(false,error)&&taskScene.prepareDialogue(task,error)&&task.coordinateTask(error),"dialogue task coordinator precedes graphics worker");for(DialogueViewAction action=task.takeViewAction();action!=kDialogueNoViewAction;action=task.takeViewAction()){if(action==kDialogueCloseView)check(taskScene.requestVisualCommand(0xfe,error),"task closes source view");else check(taskScene.requestVisualCommand(action==kDialogueSpecialView?0x15:task.defaultViewCommand(),error),"task opens its authored view");}check(taskScene.stageDialogue(task,error)&&taskScene.updateObjects(false,error)&&taskScene.prepareDialogue(task,error)&&task.tickGlyphs(false,error)&&taskScene.stageDialogue(task,error),"dialogue consumer follows graphics and stages its completed rows");
  if(frame<4)check(!task.revealedGlyphs(),"entry reset, publication policy, atlas and canvas transactions precede glyphs");if(frame==0){for(unsigned i=0;i<128;++i)check(taskScene.computedPaletteColors()[i]==(i>=80?0:i>=64?0x222:((i==1)?0xe:(i==2)?0xe0:0)),"caption reset owns secondary colors80through127 only");}if(frame==4){nameGlyphs=task.revealedGlyphs();check(nameGlyphs>0,"speaker program consumes its zero-delay glyphs in its own worker");}if(frame==5)check(task.revealedGlyphs()==nameGlyphs,"body delay control is consumed in the same worker as body coordination");if(frame==6)check(task.revealedGlyphs()==nameGlyphs+1,"first body glyph follows its real delay rather than another queue-only worker");if(task.waiting()&&!firstWaiting)firstWaiting=frame;
  NativeSaveSection sceneState;std::vector<uint8_t> dialogueState;SceneRuntime restoredScene;DialogueWindow restoredTask;check(SceneStateCodec::encode(taskScene,sceneState,error)&&SceneStateCodec::decode(sceneState,source,restoredScene,error)&&SceneStateCodec::encodeDialogue(task,dialogueState,error)&&SceneStateCodec::decodeDialogue(dialogueState,sub,restoredTask,error),"each supported entry/reveal/waiting task state has production codec continuation");taskScene=restoredScene;task=restoredTask;
 }
 check(firstWaiting==9&&task.waiting(),"source last-line consumer and ready transaction precede prompt input");check(task.acknowledge(error)&&task.waiting(),"acknowledgement is consumed by the next source coordinator");check(task.coordinateTask(error)&&!task.waiting()&&!task.finished()&&task.takeViewAction()==kDialogueCloseView,"input transaction closes view before finishing task");check(task.coordinateTask(error)&&task.finished()&&!task.active(),"following source finish transaction completes dialogue");

 std::vector<uint8_t> taskArchive;DialogueWindow rejectedTask;
 check(SceneStateCodec::encodeDialogue(task,taskArchive,error)&&taskArchive.size()>6,"task trailer has a bounded versioned encoding");
 std::vector<uint8_t> badTask=taskArchive;badTask[badTask.size()-23]=255;check(!SceneStateCodec::decodeDialogue(badTask,sub,rejectedTask,error),"unknown dialogue task phase is rejected");
 badTask=taskArchive;badTask[badTask.size()-24]=0;check(!SceneStateCodec::decodeDialogue(badTask,sub,rejectedTask,error),"legacy producer cannot acquire a task continuation implicitly");
 badTask=taskArchive;badTask[badTask.size()-22]=1;check(!SceneStateCodec::decodeDialogue(badTask,sub,rejectedTask,error),"acknowledgement outside the input phase is rejected");

 // Exercise the source page boundary independently of video-worker eligibility.
 // F6 ends a page without ending the authored text; it must retain its cursor
 // through close, canvas reset, reopen and speaker replay.
 const uint8_t pages[]={'A',0xf6,'B',0xff};DialogueWindow paged;
 check(paged.loadAssets(sub.data(),sub.size(),error)&&paged.beginTask(pages,sizeof(pages),0,3,freshModuleDialogueLayout(),error),"two-page original text task");
 auto visitPage=[&](){check(paged.coordinateTask(error)&&paged.tickGlyphs(false,error),"one logical source dialogue worker");};
 for(unsigned guard=0;!paged.waiting()&&guard<40;++guard)visitPage();
 check(paged.waiting()&&!paged.textEnded()&&paged.cursor()==2,"pause marker preserves the next page source cursor");
 while(paged.takeViewAction()!=kDialogueNoViewAction){}
 check(paged.acknowledge(error)&&paged.coordinateTask(error)&&paged.takeViewAction()==kDialogueCloseView,"page input posts close without erasing its source cursor");
 for(unsigned phase=0;phase<2;++phase){std::vector<uint8_t> saved;DialogueWindow resumed;check(SceneStateCodec::encodeDialogue(paged,saved,error)&&SceneStateCodec::decodeDialogue(saved,sub,resumed,error),"close/reset page continuation restores without an invented history");paged=resumed;check(paged.coordinateTask(error)&&paged.cursor()==2,"reset and reopen retain the unread page");if(phase==0)check(paged.takeViewAction()==kDialogueNoViewAction,"canvas reset precedes reopen");else check(paged.takeViewAction()==kDialogueDefaultView,"reopen requests the default source window before speaker replay");}
 for(unsigned guard=0;!paged.waiting()&&guard<40;++guard)visitPage();
 check(paged.waiting()&&paged.textEnded()&&paged.cursor()==sizeof(pages),"second page consumes its own ending marker");
 check(paged.acknowledge(error)&&paged.coordinateTask(error)&&paged.coordinateTask(error)&&paged.finished(),"last-page acknowledgement reaches the separate finish transaction");

 {std::vector<uint8_t> menuText(0x3810,0xff);menuText[0x3800]='A';menuText[0x3802]='B';menuText[0x3804]='C';std::vector<uint16_t> labels={0,2,4};DialogueWindow menu,expectedMenu;
 check(menu.loadAssets(sub.data(),sub.size(),error)&&expectedMenu.loadAssets(sub.data(),sub.size(),error)&&expectedMenu.beginMenu(menuText.data(),menuText.size(),labels,1,freshModuleDialogueLayout(),error)&&menu.beginMenuTask(menuText.data(),menuText.size(),labels,1,freshModuleDialogueLayout(),true,error),"menu task retains bounded original label identities");
 SceneRuntime menuScene=fresh(source);unsigned menuWorkers=0;size_t priorGlyphs=0;
 auto visitMenu=[&](){check(menuScene.beginFrame(false,error)&&menuScene.prepareDialogue(menu,error)&&menu.coordinateMenu(error),"menu coordinator submits at most one label program");for(auto action=menu.takeViewAction();action!=kDialogueNoViewAction;action=menu.takeViewAction())check(action==kDialogueMenuView&&menuScene.requestMenuView(error),"menu opening follows completed label programs");check(menuScene.stageDialogue(menu,error)&&menuScene.updateObjects(false,error)&&menuScene.prepareDialogue(menu,error)&&menu.tickGlyphs(false,error)&&menuScene.stageDialogue(menu,error),"menu font consumer uses normal shared staging");NativeSaveSection state;SceneRuntime resumedScene;std::vector<uint8_t> text;DialogueWindow resumedMenu;if(menuScene.visualControllerBusy())check(!SceneStateCodec::encode(menuScene,state,error),"menu transition retains its existing unsupported scene-save boundary");else{check(SceneStateCodec::encode(menuScene,state,error)&&SceneStateCodec::decode(state,source,resumedScene,error),"supported menu publication state restores its shared caches");menuScene=resumedScene;}check(SceneStateCodec::encodeDialogue(menu,text,error)&&SceneStateCodec::decodeDialogue(text,sub,resumedMenu,error),"menu painter and unconsumed source programs have bounded codec continuation");menu=resumedMenu;++menuWorkers;};
 for(unsigned guard=0;menu.menuPainting()&&guard<24;++guard){visitMenu();check(menu.revealedGlyphs()-priorGlyphs<=1,"separate single-glyph labels do not collapse into one worker");priorGlyphs=menu.revealedGlyphs();}
 check(menu.waiting()&&!menu.menuPainting()&&menuWorkers>=10&&menu.patterns()==expectedMenu.patterns(),"completed progressive menu equals the established original-font producer");
 check(menu.beginMenuTask(menuText.data(),menuText.size(),labels,2,freshModuleDialogueLayout(),false,error)&&expectedMenu.beginMenu(menuText.data(),menuText.size(),labels,2,freshModuleDialogueLayout(),error),"selection replacement retains its established page canvas");
 for(unsigned guard=0;menu.menuPainting()&&guard<12;++guard)visitMenu();
 check(menu.waiting()&&menu.revealedGlyphs()==2&&menu.patterns()==expectedMenu.patterns(),"selection replacement paints only the previous and new labels");
 menuText.assign(0x3820,0xff);labels.clear();for(unsigned i=0;i<8;++i){labels.push_back(uint16_t(i*2));menuText[0x3800+i*2]=uint8_t('A'+i);}
 for(unsigned variant=0;variant<3;++variant)for(unsigned selected:{0u,5u,7u}){DialogueLayout pageLayout=freshModuleDialogueLayout();pageLayout.variant=uint8_t(variant);check(menu.beginMenuTask(menuText.data(),menuText.size(),labels,selected,pageLayout,true,error)&&expectedMenu.beginMenu(menuText.data(),menuText.size(),labels,selected,pageLayout,error),"paged menu uses its source variant and selected identity");for(unsigned guard=0;menu.menuPainting()&&guard<24;++guard)visitMenu();check(menu.waiting()&&menu.patterns()==expectedMenu.patterns(),"progressive source page retains exact arrows, labels and selected colors");}
 }

 for(unsigned mode=2;mode<=3;++mode){TextEntry entry;TextEntryEffects effects;std::vector<TextEntryChoice> choices(9);for(unsigned i=0;i<choices.size();++i){choices[i].reference=uint16_t(i);choices[i].label={uint8_t((mode==2?'0':'A')+i)};}TextEntrySettings settings;settings.limit=3;SceneRuntime entryScene=fresh(source);
 check(entry.loadAssets(sub.data(),sub.size(),error)&&entry.begin(choices,settings,effects,error)&&entry.beginGraphics(false,error),"original numeric or alphabetic font producer starts independently of controller input");unsigned counter=0;
 auto visitEntry=[&](){check(entryScene.beginFrame(false,error)&&entryScene.prepareTextEntry(entry,error)&&entry.coordinateGraphics(entryScene.visualControllerBusy(),effects,error)&&entryScene.stageTextEntry(entry,error)&&entryScene.updateObjects(false,error)&&entryScene.prepareTextEntry(entry,error)&&entry.consumeGraphics(uint8_t(counter++),error)&&entryScene.stageTextEntry(entry,error),"entry font programs share normal source staging and Main publication");};
 for(unsigned guard=0;entry.graphicsBusy()&&guard<80;++guard)visitEntry();check(!entry.graphicsBusy()&&counter>=9,"entry setup consumes separate font and source view transactions");std::vector<GraphicsTransfer> target;check(entry.transfers(target,error)&&entry.fontCanvas()==target[0].decoded,"progressive initial entry canvas equals its established original-font content");
 SceneGraphicsTask overwrite;overwrite.flags=0x32;overwrite.transfers.push_back(canvas(0));overwrite.transfers[0].decoded.assign(1024,0x22);overwrite.transfers[0].destination=3072;overwrite.stagingOffsets.push_back(3072);overwrite.copyLengths.push_back(1024);overwrite.stagingSources.push_back({});check(entryScene.queueGraphicsTasks({overwrite},error)&&entryScene.updateObjects(false,error)&&entryScene.beginFrame(false,error),"independent source writer establishes inherited bytes beyond the entry buffer clear");
 effects=TextEntryEffects();effects.bodyRedraw=true;check(entry.queueGraphics(effects,error),"empty original buffer still owns its actual clear and font programs");visitEntry();check(entryScene.pendingGraphicsCopies()==0,"source buffer clear changes shared bytes without an invented immediate display copy");
 for(unsigned i=0;i<1024;++i)check(entry.fontCanvas()[3072+i]==(i<800?0:0x22),"entry clears exactly source bytes3072through3871 and retains neighboring history");
 visitEntry();DialogueLayout sharedMode;check(entryScene.dialogueLayout(sharedMode,error)&&sharedMode.fixedWidth,"actual fixed-width command establishes the shared source font mode");NativeSaveSection modeSave;SceneRuntime modeLoaded;check(SceneStateCodec::encode(entryScene,modeSave,error)&&SceneStateCodec::decode(modeSave,source,modeLoaded,error)&&modeLoaded.dialogueLayout(sharedMode,error)&&sharedMode.fixedWidth,"SNSS retains an explicitly produced font mode");NativeSaveSection legacyMode=modeSave;legacyMode.bytes[3]='Q';legacyMode.bytes.resize(legacyMode.bytes.size()-8);check(SceneStateCodec::decode(legacyMode,source,modeLoaded,error)&&modeLoaded.dialogueLayout(sharedMode,error)&&!sharedMode.fixedWidth,"legacy SNSQ retains its inherited layout policy without guessing a font-mode writer");NativeSaveSection invalidMode=modeSave;invalidMode.bytes[invalidMode.bytes.size()-8]=0;check(!SceneStateCodec::decode(invalidMode,source,modeLoaded,error),"unknown shared font mode cannot carry an asserted value");
 for(unsigned guard=0;entry.graphicsBusy()&&guard<12;++guard)visitEntry();check(!entry.graphicsBusy()&&entry.fontCanvas()[4095]==0x22,"later body and cursor font programs retain the other writer's last tile");
 check(entryScene.dialogueLayout(sharedMode,error)&&!sharedMode.fixedWidth,"fixed-width restore survives entry completion for the next window");
 // A click arriving on the source underline deadline must survive its font job.
 check(entry.graphicsReady(),"established entry owns deferred pointer activation");
 bool blinkDeferred=false;for(unsigned guard=0;guard<256&&!blinkDeferred;++guard){TextEntry probe=entry;check(probe.step(32,0,uint8_t(counter),effects,error),"probe real blink deadline");if(effects.inputDeferred){entry=probe;blinkDeferred=true;}else check(entry.step(0,0,uint8_t(counter),effects,error),"advance neutral source input");++counter;}
 check(blinkDeferred&&entry.queueGraphics(effects,error),"blink defers activation into its redraw");
 unsigned redrawVisits=0;while(entry.graphicsBusy()&&redrawVisits++<40)visitEntry();
 check(!entry.graphicsBusy()&&redrawVisits>1,"underline redraw spans multiple producer/consumer visits");
 check(entry.step(32,0,uint8_t(counter++),effects,error)&&!effects.inputDeferred&&entry.buffer()[0]!=0&&entry.queueGraphics(effects,error),"one retained edge edits only after redraw completes");
 for(unsigned guard=0;entry.graphicsBusy()&&guard<80;++guard)visitEntry();
 check(entry.step(128,0,uint8_t(counter++),effects,error)&&entry.awaitingKey(),"submit observes source key barrier");
 check(entry.resumeKey(true,effects,error)&&effects.completed&&effects.cursorHidden,"completion keeps cursor hidden instead of restoring it");
 check(entry.beginCloseGraphics(effects.view,error)&&entry.graphicsBusy(),"entry cannot hand result off before modal/common cleanup");
 unsigned closeVisits=0;while(entry.graphicsBusy()&&closeVisits++<40)visitEntry();
 check(!entry.graphicsBusy()&&closeVisits>=4,"modal close waits before queued16 canvas reset");
 std::vector<uint8_t> cleanPatterns,cleanNames;initializeOriginalFontCanvas(cleanPatterns,cleanNames);
 check(entry.fontCanvas()==cleanPatterns,"queued16 clears all old body and underline bytes, retaining original sentinel");
 check(entryScene.beginFrame(false,error)&&!entryScene.fontPublicationPending(),"Main publishes clean canvas before result continuation");
 check(entryScene.prepareDialogue(window,error)&&window.transfers(target,error)&&target[1].decoded==cleanNames,"next caption sees all restored shared name cells");


 }

 {DialogueWindow producer;const uint8_t modeText[]={0xf7,1,'A','B',0xff};check(producer.loadAssets(sub.data(),sub.size(),error)&&producer.begin(modeText,sizeof(modeText),0,0,freshModuleDialogueLayout(),error)&&producer.tick(false,error)&&producer.tick(false,error),"literal font continuation reaches a real unconsumed mode write");std::vector<uint8_t> modern;DialogueWindow resumed;check(SceneStateCodec::encodeDialogue(producer,modern,error)&&SceneStateCodec::decodeDialogue(modern,sub,resumed,error),"SND3 preserves pending font-mode publication");SceneRuntime modes=fresh(source);check(modes.stageDialogue(resumed,error),"restored explicit mode writer binds its shared owner");DialogueLayout layout;check(modes.dialogueLayout(layout,error)&&layout.fixedWidth,"pending mode publication is not lost across restoration");std::vector<uint8_t> legacy=modern;legacy[3]='2';legacy.resize(legacy.size()-2);DialogueWindow old;check(SceneStateCodec::decodeDialogue(legacy,sub,old,error),"SND2 remains a literal legacy continuation");SceneRuntime inherited=fresh(source);check(inherited.prepareDialogue(old,error)&&inherited.stageDialogue(old,error)&&inherited.prepareDialogue(old,error)&&old.tick(false,error),"legacy continuation is not overwritten by a new shared-mode assumption");std::vector<uint8_t> expected=producer.patterns();OriginalFont font;FontGlyph glyph;check(font.load(sub.data(),sub.size(),error)&&font.glyph('B',glyph,error)&&rasterOriginalFontGlyph(glyph,'B',33,0,1,true,expected,error)&&old.patterns()==expected,"legacy fixed-width glyph retains its recorded placement until a real mode producer runs");}


 }
int blankCold(bool importing){
 Source source;std::string error;SceneRuntime scene;NativeSaveSection saved(kSaveScene);
 auto emit=[](const char* tag,const std::vector<uint8_t>& bytes){std::printf("%s",tag);for(uint8_t value:bytes)std::printf("%02x",unsigned(value));std::putchar('\n');};
 if(importing){std::string hex;check(bool(std::getline(std::cin,hex))&&!hex.empty()&&!(hex.size()&1)&&hex.size()<=32*1024*1024,"bounded cold archive on stdin");auto nibble=[](char c){if(c>='0'&&c<='9')return unsigned(c-'0');if(c>='a'&&c<='f')return unsigned(c-'a'+10);throw std::runtime_error("invalid cold archive hex");};for(size_t at=0;at<hex.size();at+=2)saved.bytes.push_back(uint8_t((nibble(hex[at])<<4)|nibble(hex[at+1])));check(SceneStateCodec::decode(saved,source,scene,error),error.c_str());}
 else{scene=fresh(source);std::vector<uint16_t> colors(128,0);colors[1]=0xe;check(scene.establishFreshPalette(error)&&scene.setPalette(colors,0,error),"cold handoff has an actual fresh palette policy owner");SceneLayout layout=scene.presentationLayout();layout.columns=layout.rows=64;
  GraphicsTransfer tile=canvas(0x11);tile.destination=32;tile.decoded.resize(32);
  check(scene.configure(layout,error)&&scene.applyTransfer(tile,error)&&scene.fillPlaneCells(0,0,0,64,64,1,error),"cold handoff independently authored map allocation");scene.establishSceneFlags(2);
  check(scene.preparePresentationReplacement(error)&&!scene.blankingPending()&&SceneStateCodec::encode(scene,saved,error),"suppressed stable handoff exports its actual literal image");
  emit("STATE ",saved.bytes);
 }
 check(!scene.blankingPending()&&image(scene).indexed[0]==1,"cold import does not invent a pending resource writer");
 for(unsigned step=0;step<8;++step){
  check(scene.beginFrame(false,error,step),"cold future Main publication");
  if(step==0)check(scene.preparePresentationReplacement(error)&&scene.install(22,source.bytes,0,error),"future actual replacement requests a new handoff");
  bool admitted=false;check(scene.advanceCoordinatorClock(error,&admitted)&&admitted==(step>=2),"cold future source coordinator admission");
  if(step==3)check(scene.queueAbsolutePlanePosition(0,0,256,error),"future source scroll command selects the retained allocation tail");
  check(scene.updateObjects(false,error),"cold future Sub objects/scroll");
  if(scene.graphicsServiceBusy())check(scene.commitTransfers(error),"cold future graphics service");
  check(scene.commitEnvironmentFrame(uint16_t(step),error),"cold future completed sprite owner");
  const SceneImage frame=image(scene);check(frame.indexed[0]==(step==0||step>=4?1:0),"cold future blank and retained-tail publication order");
  char tag[32];std::snprintf(tag,sizeof(tag),"FRAME %u ",step);emit(tag,frame.rgb);
 }
 std::printf("blank handoff cold %s: 8 future frames, %u checks\n",importing?"import":"export",checks);return 0;
}
}
int main(int argc,char** argv){try{if(argc==2&&(std::string(argv[1])=="--blank-export"||std::string(argv[1])=="--blank-import"))return blankCold(std::string(argv[1])=="--blank-import");nativeJobContracts();captionViewPublication();captionPublicationLifetime();presentationReplacement();if(argc==2)fontAliases(argv[1]);dependentCopies();Source source;std::string error;SceneRuntime scene=fresh(source);const auto blank=image(scene);
 check(scene.applyDialogueTransfers({canvas(0x11)},error)&&scene.pendingGraphicsCopies()==1&&scene.fontPublicationPending(),"producer posts a completed font copy and continuation barrier");check(image(scene).rgb==blank.rgb,"producer cannot publish during its own worker");
 NativeSaveSection pending;check(SceneStateCodec::encode(scene,pending,error),"pending completed copy is supported graphics state");SceneRuntime restored;check(SceneStateCodec::decode(pending,source,restored,error)&&restored.pendingGraphicsCopies()==1,"restore retains copy source and publication age");
 check(scene.beginFrame(true,error)&&scene.pendingGraphicsCopies()==1&&image(scene).rgb==blank.rgb,"pause preserves pending publication");
 check(restored.fontPublicationPending()&&scene.beginFrame(false,error)&&restored.beginFrame(false,error)&&!scene.fontPublicationPending()&&!restored.fontPublicationPending(),"native publication releases both cold continuation barriers");const auto red=image(scene);check(red.rgb==image(restored).rgb&&red.rgb!=blank.rgb,"restored full frame exactly matches publication");
 check(scene.applyDialogueTransfers({canvas(0x11),canvas(0x22)},error)&&scene.beginFrame(false,error),"multiple references observe final shared canvas");const auto green=image(scene);check(green.rgb!=red.rgb,"late shared source replaces early snapshot");
 SceneRuntime history=scene;check(history.applyDialogueTransfers({canvas(0x11)},error),"pending source before module reset");history.resetForModule();check(history.pendingGraphicsCopies()==1&&history.configure(scene.presentationLayout(),error)&&history.setPalette(std::vector<uint16_t>(128,0),0,error)&&history.beginFrame(false,error),"module reset preserves completed copies and staging allocations");
 SceneGraphicsQueue queue;SceneGraphicsTask task;task.flags=1;for(unsigned i=0;i<2;++i){task.transfers.push_back(canvas(0));task.transfers.back().destination=uint16_t(i*32);task.stagingOffsets.push_back(0);task.copyLengths.push_back(32);task.stagingSources.push_back(std::vector<uint8_t>(32,uint8_t(i+1)));}
 std::vector<GraphicsTransfer> ready;std::vector<SceneGraphicsCopy> copies;
 check(queue.enqueue(task,error)&&queue.tick(false,ready,error,&copies)&&ready.size()==1&&ready[0].decoded[0]==1&&queue.pending()==1,"compressed task emits only its first record");
 check(queue.tick(true,ready,error,&copies)&&ready.empty()&&copies.empty()&&queue.pending()==1,"busy holds exact compressed record cursor");
 check(queue.tick(false,ready,error,&copies)&&ready.size()==1&&ready[0].destination==32&&ready[0].decoded[0]==2&&!queue.pending(),"next worker emits successor and retires task");
 task.flags=2;for(unsigned i=0;i<2;++i)task.transfers[i].decoded.assign(32,uint8_t(i+1));check(queue.enqueue(task,error)&&queue.tick(false,ready,error,&copies)&&copies.size()==2,"raw family retains same-worker multiple records");
 GraphicsTransfer resolved;check(queue.resolveCopy(copies[0],resolved,error)&&resolved.decoded==std::vector<uint8_t>(32,2),"published raw reference observes later overlapping record");
 auto bad=pending;bad.bytes[bad.bytes.size()-6]=2;SceneRuntime unchanged=restored;NativeSaveSection before,after;check(SceneStateCodec::encode(unchanged,before,error)&&!SceneStateCodec::decode(bad,source,unchanged,error)&&SceneStateCodec::encode(unchanged,after,error)&&before.bytes==after.bytes,"invalid completed copy rejects transactionally");
 std::printf("graphics copy publication: %u checks passed\n",checks);return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
