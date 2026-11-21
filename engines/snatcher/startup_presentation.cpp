/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "startup_presentation.h"
#include <algorithm>
namespace Snatcher {
StartupOptions::StartupOptions():_known(false),_row(1),_previous(0){}
void StartupOptions::establishFresh(){_settings=StartupSettings();_known=true;_row=1;_previous=0;}
bool StartupOptions::restoreVerified(const StartupSettings& settings,std::string& error){error.clear();if(settings.controlMapping>=6){error="startup control mapping outside six source mappings";return false;}_settings=settings;_known=true;_row=1;_previous=0;return true;}
bool StartupOptions::settings(StartupSettings& out,std::string& error)const{error.clear();if(!_known){error="startup settings require fresh or verified saved ownership";return false;}out=_settings;return true;}
bool StartupOptions::select(unsigned row,std::string& error){error.clear();if(!_known||(row!=1&&row!=2&&row!=4)){error="PC options row unavailable";return false;}_row=row;return true;}
bool StartupOptions::poll(uint8_t pressed,StartupOptionsUpdate& out,std::string& error){
 error.clear();if(!_known){error="startup options require known settings";return false;}StartupOptionsUpdate update;
 // The resource callback suppresses an identical nonzero mask, not all holds.
 uint8_t previous=_previous;_previous=pressed;if(pressed&&pressed==previous){out=update;return true;}
 if(pressed&2){do{_row=(_row+1)%5;}while(_row==0||_row==3);}
 else if(pressed&1){do{_row=(_row+4)%5;}while(_row==0||_row==3);}
 if(_row==1){
  // Original Right returns immediately unless it wraps; Left is then checked.
  bool checkLeft=true;if(pressed&8){++_settings.controlMapping;if(_settings.controlMapping==6)_settings.controlMapping=0;else checkLeft=false;}
  if(checkLeft&&(pressed&4))_settings.controlMapping=(_settings.controlMapping+5)%6;
 }
 if(_row==2&&(pressed&12)){_settings.mono=!_settings.mono;update.fmCommands.push_back(_settings.mono?0xf3:0xf4);update.pcmCommands.push_back(_settings.mono?0xfc:0xfd);}
 if((pressed&0x80)||((pressed&0x70)&&_row>=3)){
  update.action=kStartupOptionsExit;update.pcmCommands.push_back(0x38);
 }
 out=update;return true;
}
bool StartupOptions::applyActorSelection(SceneRuntime& scene,std::string& error)const{
 if(!_known){error="startup actor selection requires known settings";return false;}
 SceneRuntime next=scene;if(!next.setActorFlags(32,3,error)||!next.setActorFlags(33,3,error)||!next.setActorFlags(36,3,error))return false;
 if(!next.selectActorRecord(34,((_row==1?0:6)+_settings.controlMapping)*2,0,error)||!next.selectActorRecord(35,((_row==2?0:2)+unsigned(_settings.mono))*2,0,error))return false;
 scene=next;return true;
}
StartupLicense::StartupLicense():_prepare(0),_phase(0),_counter(0),_gradient(0),_offset(0),_spriteTile(4),_visibleSpriteTile(4),_scrollX(0),_scrollY(0),_spriteX(0),_visibleScrollX(0),_visibleScrollY(0),_visibleSpriteX(0),_visibleSprite(false),_started(false),_finished(false),_previouslyPresented(false),_display(true),_sprite(false){}
bool StartupLicense::asset(size_t source,uint16_t destination,std::string& error){
 if(source>_main.size()||_main.size()-source<2){error="truncated Main presentation graphics";return false;}GraphicsTransfer transfer;transfer.destination=destination;transfer.delay=0;size_t consumed=0;unsigned length=unsigned(_main[source])*256+_main[source+1];
 if(!decodeGraphics(&_main[source+2],_main.size()-source-2,length,65536,transfer.decoded,consumed,error))return false;return _renderer.applyTransfer(transfer,error);
}
bool StartupLicense::pages(unsigned a,unsigned b,unsigned c,unsigned d,std::string& error){unsigned values[]={a,b,c,d};for(unsigned n=0;n<4;++n){size_t at=0x1abe+values[n]*32;if(at>_main.size()||_main.size()-at<32){error="truncated Main palette page";return false;}for(unsigned i=0;i<16;++i)_colors[n*16+i]=uint16_t(unsigned(_main[at+i*2])*256+_main[at+i*2+1]);}return true;}
bool StartupLicense::cells(unsigned allocation,unsigned first,unsigned count,uint16_t attr,std::string& error){unsigned plane=_layout.planeAllocation[0]==allocation?0:1;while(count){unsigned x=first%unsigned(_layout.columns),y=first/unsigned(_layout.columns),n=unsigned(_layout.columns)-x;if(n>count)n=count;if(!_renderer.fillPlaneCells(plane,x,y,n,1,attr,error))return false;first+=n;count-=n;}return true;}
bool StartupLicense::start(const std::vector<uint8_t>& mainIp,bool previous,std::string& error){
 error.clear();if(mainIp.size()<0x30ff){error="Main presentation resources truncated";return false;}StartupLicense next;next._main=mainIp;next._colors.assign(64,0);next._visible=next._colors;next._previouslyPresented=previous;
 if(!readSceneLayoutPreset(mainIp.data(),mainIp.size(),kSceneSquareLayout,next._layout,error)||!next._renderer.configure(next._layout,error)||!next._renderer.establishSceneReset(kSceneSquareLayout,error)||!next._renderer.setPalette(next._colors,0,error))return false;next._started=true;next._music.push_back(0xf2);*this=next;return true;
}
bool StartupLicense::finish(std::string& error){_scrollX=_scrollY=0;_sprite=false;if(!readSceneLayoutPreset(_main.data(),_main.size(),kSceneTallLayout,_layout,error)||!_renderer.configure(_layout,error)||!_renderer.establishSceneReset(kSceneTallLayout,error))return false;_colors.assign(64,0);_finished=true;return true;}
bool StartupLicense::tick(const StartupLicenseInput& input,std::string& error){
 error.clear();if(!_started){error="Main license not started";return false;}if(_finished)return true;_visible=_colors;_visibleScrollX=_scrollX;_visibleScrollY=_scrollY;_visibleSpriteX=_spriteX;_visibleSpriteTile=_spriteTile;_visibleSprite=_sprite;
 if(_prepare<7){if(_prepare==3&&!asset(0x1b9e,0,error))return false;++_prepare;return true;}
 if(_phase>=2&&input.startPressed){_music.push_back(0xf2);return finish(error);}
 auto word=[this](size_t at){return uint16_t(unsigned(_main[at])*256+_main[at+1]);};
 switch(_phase){
 case 0:
  if(_previouslyPresented){_phase=2;if(!pages(4,4,4,5,error))return false;}
  else{
   if(!asset(0x286e,0x2000,error)||!pages(6,6,6,6,error))return false;
   const unsigned sources[]={0,24,28,88,148,172},destinations[]={0xe294,0xe39c,0xe482,0xe704,0xea94,0xeb8c},lengths[]={22,4,60,58,22,42};
   for(unsigned n=0;n<6;++n)for(unsigned j=0;j<lengths[n]/2;++j)if(!cells(0xe000,(destinations[n]-0xe000)/2+j,1,uint16_t(0x100|_main[0x3094+sources[n]/2+j]),error))return false;
   ++_phase;return true;
  }
  // Source cached-title entry runs the same palette-gradient initializer.
  _colors[50]=_colors[51]=0;for(unsigned j=0;j<5;++j)_colors[52+j]=uint16_t((j+1)*0x20);return true;
 case 1:
  ++_counter;if(_counter<180||!input.discReady||!input.soundDriverReady||!input.moduleReady)return true;
  if(!pages(4,4,4,5,error))return false;_colors[50]=_colors[51]=0;for(unsigned j=0;j<5;++j)_colors[52+j]=uint16_t((j+1)*0x20);++_phase;return true;
 case 2:_display=false;if(!asset(0x25c8,0xc000,error))return false;_counter=2;++_phase;return true;
 case 3:
  if(--_counter)return true;if(!asset(0x24b4,0xe000,error)||!cells(0xe000,0xa0,1536,0x6001,error)||!cells(0xe000,0x60,32,0x6002,error)||!cells(0xe000,0xa0,32,0x6003,error)||!cells(0xe000,0x40,1,0x6002,error)||!cells(0xe000,0x80,1,0x6003,error))return false;
  _display=true;_sprite=true;_spriteX=0;_spriteTile=4;++_phase;return true;
 case 4:{unsigned speed=input.alternateVideo?8:6;_spriteTile=4+4*unsigned((input.sourceFrame>>1)&3);_spriteX+=speed;_scrollX=(_scrollX+speed)&0x1ff;if(_scrollX>=256){_scrollX=256;_counter=16;_gradient=5;++_phase;}return true;}
 case 5:{unsigned old=_counter;if(_counter==0){++_phase;return true;}--_counter;if((old&7)==0){for(unsigned j=0;j<=_gradient;++j)_colors[56-j]=uint16_t(_colors[56-j]+0x20);++_gradient;}return true;}
 case 6:
  _scrollY=(_scrollY-(input.alternateVideo?3:2))&0x3ff;if(_scrollY<0x318){++_phase;_offset=30;for(unsigned j=0;j<32;++j)_colors[16+j]=word(0x1ade+j*2);_music.push_back(0x3f);}return true;
 case 7:
  if(input.sourceFrame&1)return true;for(unsigned j=0;j<(32-_offset)/2;++j){_colors[16+_offset/2+j]=word(0x1aea+j*2);_colors[32+_offset/2+j]=word(0x1b0a+j*2);}_offset-=2;if(_offset==10){_offset=30;++_phase;for(unsigned j=0;j<16;++j)_colors[j]=word(0x1abe+j*2);_colors[0]=0;}return true;
 case 8:
  if(input.sourceFrame&1)return true;for(unsigned j=0;j<(32-_offset)/2;++j)_colors[_offset/2+j]=word(0x1aca+j*2);_offset-=2;if(_offset==10){_offset=0;_counter=3;++_phase;}return true;
 case 9:
  if(--_counter)return true;_counter=3;_colors[0]=uint16_t(_colors[0]+0x222);if(_colors[0]==0xeee){if(!pages(0,1,2,3,error))return false;_offset=512;++_phase;}return true;
 case 10:if(--_offset==0){_offset=40;++_phase;}return true;
 case 11:
  if(--_offset==0)return finish(error);if(!(_offset&1))for(uint16_t& color:_colors){unsigned b=color&0xe00,g=color&0xe0,r=color&0xe;color=uint16_t((b?b-0x200:0)|(g?g-0x20:0)|(r?r-2:0));}return true;
 default:error="invalid Main license semantic phase";return false;
 }
}
bool StartupLicense::render(SceneColorEncoding encoding,SceneImage& image,std::string& error){
 if(!_started){error="Main license render requires ownership";return false;}if(!_display){image=SceneImage();image.width=_layout.width;image.height=_layout.height;image.indexed.assign(image.width*image.height,0);image.rgb.assign(image.width*image.height*3,0);return true;}
 SceneLayout layout=_layout;layout.sourceX[0]=-_visibleScrollX;layout.sourceY[0]=_visibleScrollY;
 if(!_renderer.configure(layout,error)||!_renderer.setPalette(_visible,0,error))return false;std::vector<PlacedPiece> pieces;if(_visibleSprite){PlacedPiece piece;piece.x=_visibleSpriteX;piece.y=8;piece.width=piece.height=16;piece.attributes=uint16_t(0xe000|_visibleSpriteTile);pieces.push_back(piece);}return _renderer.render(pieces,encoding,image,error);
}
StartupCinematic::StartupCinematic():_index(0),_track(0),_nextId(1),_started(false),_trackReady(false),_finished(false){}
bool StartupCinematic::start(uint16_t resource,const std::vector<uint8_t>& data,std::string& error){
 error.clear();if(resource!=2&&resource!=3){error="cinematic requires DATA_D0 or DATA_D1 resource identity";return false;}
 size_t table=resource==3?0x1298:0x285a;unsigned count=resource==3?29:35;
 if(table>data.size()||count*6>data.size()-table){error="truncated cinematic timing table";return false;}std::vector<Entry> entries;
 for(unsigned i=0;i<count;++i){size_t at=table+i*6;uint16_t raw=uint16_t(unsigned(data[at])<<8|data[at+1]);int relative=raw<0x8000?int(raw):int(raw)-65536;int handler=int(table)+relative;
  Entry entry;entry.clock=uint32_t(data[at+2])<<24|uint32_t(data[at+3])<<16|uint32_t(data[at+4])<<8|data[at+5];entry.event.resource=resource;
  StartupCinematicEvent& e=entry.event;
  if(resource==3){
   const unsigned scenes[]={0x1376,0x137c,0x1382,0x1398,0x139e,0x13a4,0x13aa,0x13b0,0x13b6,0x13bc,0x13c2,0x13c8};
   for(unsigned n=0;n<12;++n)if(handler==int(scenes[n])){e.action=kStartupCinematicScene;e.value=n;if(n==2)e.auxiliary=0x110;}
   if(handler==0x1422||handler==0x1430){e.action=kStartupCinematicPalette;e.paletteOffset=handler==0x1422?0x142c:0x143a;}
   if(handler>=0x13ea&&handler<=0x140a&&((handler-0x13ea)%8)==0){e.action=kStartupCinematicVisual;e.value=8+unsigned(handler-0x13ea)/8;}
   if(handler==0x13da)e.action=kStartupCinematicComplete;
  }else{
   const unsigned scenes[]={0x298e,0x2994,0x299a,0x29a0,0x29ae,0x29bc,0x29c2,0x29c8,0x29ce,0x29d4,0x29da,0x29e0,0x29e6,0x29ec,0x29f2,0x29f8};
   for(unsigned n=0;n<16;++n)if(handler==int(scenes[n])){e.action=kStartupCinematicScene;e.value=n;if(n==3)e.auxiliary=0xff;if(n==4)e.auxiliary=1;}
   if(handler==0x2a40||handler==0x2a4e||handler==0x2a5c){e.action=kStartupCinematicPalette;e.paletteOffset=handler==0x2a40?0x2a4a:handler==0x2a4e?0x2a58:0x2a66;}
   if(handler==0x295c){e.action=kStartupCinematicDisplayFlag;e.value=0x10;}
   if(handler==0x2966){e.action=kStartupCinematicTrack;e.value=4;}
   // 2970 is an actual CD-start completion gate, not a timed visual event.
   if(handler==0x2970){e.action=kStartupCinematicNone;e.value=4;}
   if(handler==0x29fe){e.action=kStartupCinematicActorFlags;e.value=16;e.auxiliary=1;}
   if(handler==0x2a12)e.action=kStartupCinematicComplete;
  }
  if(e.action==kStartupCinematicNone&&!(resource==2&&handler==0x2970)){error="unrecognized cinematic semantic handler";return false;}entries.push_back(entry);
 }
 _entries.swap(entries);_index=0;_track=resource==3?3:5;_started=true;_trackReady=false;_finished=false;_pending=StartupCinematicEvent();return true;
}
bool StartupCinematic::poll(bool known,unsigned track,uint32_t clock,bool startPressed,StartupCinematicEvent& out,std::string& error){
 error.clear();out=StartupCinematicEvent();if(!_started){error="cinematic not started";return false;}if(_finished)return true;if(_pending.action!=kStartupCinematicNone){out=_pending;return true;}
 StartupCinematicEvent event;
 if(!_trackReady){event.action=kStartupCinematicTrack;event.value=_track;}
 else{
  if(!known||track!=_track)return true;
  if(_index>=_entries.size()){error="cinematic table exhausted without owned completion";return false;}
  const Entry& entry=_entries[_index];
  if(clock<entry.clock){if(!startPressed)return true;event.action=kStartupCinematicSkip;}
  else if(entry.event.action==kStartupCinematicNone){++_index;return true;}
  else event=entry.event;
 }
 event.id=_nextId++;_pending=event;out=event;return true;
}
bool StartupCinematic::complete(const StartupCinematicEvent& event,std::string& error){
 error.clear();if(_pending.action==kStartupCinematicNone||event.id!=_pending.id||event.action!=_pending.action||event.value!=_pending.value||event.resource!=_pending.resource||event.auxiliary!=_pending.auxiliary||event.paletteOffset!=_pending.paletteOffset){error="stale or mismatched cinematic completion";return false;}
 if(event.action==kStartupCinematicTrack){if(_trackReady)++_index;_track=event.value;_trackReady=true;}
 else if(event.action==kStartupCinematicComplete||event.action==kStartupCinematicSkip)_finished=true;
 else ++_index;
 _pending=StartupCinematicEvent();return true;
}
bool validateStartupStorage(const StartupStorage& storage,std::string& error){
 error.clear();if(!storage.known){error="startup storage availability is unknown";return false;}
 for(const StartupSavedSlot& slot:storage.slots){if(!slot.known){if(storage.available||slot.present||slot.metadataKnown){error="startup slot presence is unknown or has contradictory backing flags";return false;}continue;}if(slot.present&&(!storage.available||!slot.metadataKnown)){error="startup occupied slot requires actual readable metadata";return false;}if(slot.metadataKnown&&(slot.counter>99||slot.hours>99||slot.minutes>59||slot.progression>2)){error="startup slot metadata outside source digit/act catalog";return false;}}
 return true;
}
bool StartupSaveSelector::start(const StartupStorage& storage,std::string& error){
 if(!validateStartupStorage(storage,error))return false;unsigned first=4;for(unsigned i=0;i<4;++i)if(storage.slots[i].present){first=i;break;}if(first==4){error="startup file selector requires an occupied slot";return false;}_storage=storage;_selected=first;_previous=0;_known=true;return true;
}
bool StartupSaveSelector::select(unsigned slot,std::string& error){error.clear();if(!_known||slot<1||slot>4||!_storage.slots[slot-1].present){error="mouse slot is not occupied";return false;}_selected=slot-1;return true;}
bool StartupSaveSelector::poll(uint8_t pressed,StartupSlotAction& action,std::string& error){
 error.clear();action=kStartupSlotNone;if(!_known){error="startup file selector has no source slot owner";return false;}uint8_t previous=_previous;_previous=pressed;if(pressed&&pressed==previous)return true;
 if(pressed&3){unsigned direction=(pressed&2)?1:3;do{_selected=(_selected+direction)&3;}while(!_storage.slots[_selected].present);}
 if(pressed&0x80)action=kStartupSlotRead;else if(pressed&0x10)action=kStartupSlotCancel;return true;
}
bool decorateStartupSlotMap(std::vector<uint8_t>& map,const StartupStorage& storage,std::string& error){
 if(!validateStartupStorage(storage,error))return false;if(map.size()!=2048){error="startup metadata requires original 2048-byte name canvas";return false;}std::vector<uint8_t> next=map;
 auto put=[&](size_t at,unsigned value){next[at]=uint8_t(value>>8);next[at+1]=uint8_t(value);};
 auto digit=[&](size_t at,unsigned value){put(at,0x4580+value*2);put(at+0x40,0x4581+value*2);};
 for(unsigned i=0;i<4;++i){const StartupSavedSlot& slot=storage.slots[i];size_t at=0x256+i*0x100;if(!slot.present){for(unsigned c=0;c<5;++c){put(at+c*2,0x459c+c);put(at+0x40+c*2,0x45a1+c);}continue;}
  if(slot.hours>=10)digit(at,slot.hours/10);digit(at+2,slot.hours%10);put(at+4,0x4594);put(at+0x44,0x4595);digit(at+6,slot.minutes/10);digit(at+8,slot.minutes%10);put(at-0x68,0x4596+slot.progression*2);put(at-0x28,0x4597+slot.progression*2);if(slot.counter>=10)digit(at+0x16,slot.counter/10);digit(at+0x18,slot.counter%10);
 }map.swap(next);return true;
}
StartupPresenter::StartupPresenter():_phase(kStartupNativeDormant),_delay(0),_paint(0),_savedGames(false),_trackPosted(false),_soundDriverSuspended(false),_continueSelected(false),_cancelLoad(false),_loadedSlot(0),_previousTitle(0),_previousModule(0){}
bool StartupPresenter::start(const std::vector<uint8_t>& mainIp,const std::vector<uint8_t>& title,const std::vector<uint8_t>& first,const std::vector<uint8_t>& second,const std::vector<uint8_t>& subcode,bool saves,std::string& error){
 if(saves){error="saved startup requires actual four-slot metadata";return false;}StartupStorage storage;storage.known=storage.available=true;for(StartupSavedSlot& slot:storage.slots)slot.known=true;return start(mainIp,title,first,second,subcode,storage,false,nullptr,error);
}
bool StartupPresenter::start(const std::vector<uint8_t>& mainIp,const std::vector<uint8_t>& title,const std::vector<uint8_t>& first,const std::vector<uint8_t>& second,const std::vector<uint8_t>& subcode,const StartupStorage& storage,bool returned,const StartupSettings* settings,std::string& error){
 if(!validateStartupStorage(storage,error))return false;if(returned&&!settings){error="return to title requires retained settings ownership";return false;}
 StartupPresenter next;if(!next._license.start(mainIp,returned,error))return false;next._main=mainIp;next._title=title;next._first=first;next._second=second;next._subcode=subcode;if(!next._pcOptionsFont.load(subcode.data(),subcode.size(),error))return false;next._storage=storage;for(const StartupSavedSlot& slot:storage.slots)next._savedGames|=slot.present;next._continueSelected=next._savedGames;if(settings){if(!next._options.restoreVerified(*settings,error))return false;}else next._options.establishFresh();next._phase=kStartupNativeLicense;*this=next;return true;
}
bool StartupPresenter::acknowledgeLoad(unsigned slot,const StartupSettings& settings,std::string& error){
 error.clear();if(_phase!=kStartupNativeLoadPending||slot!=_selector.selectedSlot()){error="stale startup record read acknowledgement";return false;}if(!_options.restoreVerified(settings,error))return false;_loadedSlot=slot;_cancelLoad=false;_paint=0;_fm.push_back(settings.mono?0xf3:0xf4);_pcm.push_back(settings.mono?0xfc:0xfd);_phase=kStartupNativeLoadClose;return true;
}
bool StartupPresenter::mouseActionReady(uint8_t mask)const{
 if(mask&&mask==_previousModule)return false;
 if(_phase==kStartupNativeTitleReady)return !mask||mask!=_previousTitle;
 if(_phase==kStartupNativeOptionsPoll)return _options.mouseActionReady(mask);
 if(_phase==kStartupNativeLoadPoll)return _selector.mouseActionReady(mask);
 return false;
}
std::vector<MouseTarget> StartupPresenter::mouseTargets(const SceneRuntime& scene,std::string& error)const{
 std::vector<MouseTarget> out;error.clear();unsigned actor=0,count=0;
 if(_phase==kStartupNativeTitleReady){actor=_savedGames?17:16;count=_savedGames?2:1;}
 else if(_phase==kStartupNativeOptionsPoll){out.push_back(MouseTarget(MouseRect(24,64,208,24),1));out.push_back(MouseTarget(MouseRect(24,104,208,24),2));out.push_back(MouseTarget(MouseRect(24,144,208,24),4));return out;}
 else if(_phase==kStartupNativeLoadPoll){actor=24;count=4;}else return out;
 StartupSettings settings;if(_phase==kStartupNativeOptionsPoll&&!_options.settings(settings,error))return out;
 for(unsigned i=0;i<count;++i){if(_phase==kStartupNativeLoadPoll&&!_storage.slots[i].present)continue;
 int record=_phase==kStartupNativeTitleReady?(_savedGames?int(i*2+1):-1):int(i*4);std::vector<PlacedPiece> pieces;if(!scene.actorPieces(actor,pieces,error,record))return std::vector<MouseTarget>();if(pieces.empty())continue;
 // Saved title frames contain BOTH text rows; only the leftmost cursor piece
 // moves with the selected choice. Bound its row, never the composite frame.
 if(_phase==kStartupNativeTitleReady&&_savedGames){const PlacedPiece* marker=&pieces[0];for(const PlacedPiece& p:pieces)if(p.x<marker->x)marker=&p;int rowTop=marker->y,rowBottom=rowTop+int(marker->height);pieces.erase(std::remove_if(pieces.begin(),pieces.end(),[&](const PlacedPiece& p){return p.y>=rowBottom||p.y+int(p.height)<=rowTop;}),pieces.end());}
 int x=pieces[0].x,y=pieces[0].y,right=x+int(pieces[0].width),bottom=y+int(pieces[0].height);for(const PlacedPiece& p:pieces){x=std::min(x,p.x);y=std::min(y,p.y);right=std::max(right,p.x+int(p.width));bottom=std::max(bottom,p.y+int(p.height));}
 out.push_back(MouseTarget(MouseRect(x,y,right-x,bottom-y),_phase==kStartupNativeLoadPoll?i+1:i));
 }return out;
}
bool StartupPresenter::mouseSelect(unsigned target,SceneRuntime& scene,std::string& error){
 error.clear();if(_phase==kStartupNativeTitleReady){if(target>1||(target==1&&!_savedGames)){error="mouse title choice unavailable";return false;}_continueSelected=target==1;return selectTitle(scene,error);}
 if(_phase==kStartupNativeOptionsPoll){if(!_options.select(target,error))return false;return _options.applyActorSelection(scene,error);}
 if(_phase==kStartupNativeLoadPoll){if(!_selector.select(target,error))return false;return applySlotSelection(scene,true,error);}
 error="startup mouse selection has no ready owner";return false;
}
bool StartupPresenter::selectTitle(SceneRuntime& scene,std::string& error){
 if(!scene.setActorFlags(16,_savedGames?3:2,error)||!scene.setActorFlags(17,_savedGames?2:3,error))return false;
 if(_savedGames&&!scene.selectActorRecord(17,_continueSelected?3:1,0,error))return false;return true;
}
bool StartupPresenter::applySlotSelection(SceneRuntime& scene,bool visible,std::string& error){
 for(unsigned actor=24;actor<=25;++actor){if(!scene.setActorFlags(actor,visible?0:3,error))return false;if(visible&&!scene.selectActorRecord(actor,(_selector.selectedSlot()-1)*4,0,error))return false;}return true;
}
bool StartupPresenter::palette(const std::vector<uint8_t>& bytes,size_t offset,SceneRuntime& scene,std::string& error){PaletteEffect effect;if(!readPaletteEffect(bytes.data(),bytes.size(),offset,bytes.size(),effect,error))return false;return scene.queuePaletteEffect(effect,error);}
bool StartupPresenter::install(SceneRuntime& scene,uint16_t resource,const std::vector<uint8_t>& bytes,unsigned index,std::string& error){if(scene.graphicsServiceBusy()){error="startup scene installation requires prior graphics service completion";return false;}if(!scene.preparePresentationReplacement(error)||!scene.install(resource,bytes,index,error))return false;return true;}
bool StartupPresenter::tick(const StartupFrame& frame,SceneRuntime& scene,std::string& error){
 error.clear();if(_phase==kStartupNativeDormant){error="native startup not started";return false;}if(finished()||loaded())return true;
 if(_phase==kStartupNativeLicense){StartupLicenseInput input;input.startPressed=(frame.pressed&0x80)!=0||_mouseSkipRequested;input.discReady=true;input.soundDriverReady=frame.soundDriverReady;input.moduleReady=!_title.empty();input.sourceFrame=frame.sourceFrame;
  if(!_license.tick(input,error))return false;std::vector<uint16_t> music=_license.takeMusicCommands();_fm.insert(_fm.end(),music.begin(),music.end());if(!_license.finished())return true;
  _mouseSkipRequested=false;scene.inheritLicenseRowHistory();
  SceneLayout layout;if(!readSceneLayoutPreset(_main.data(),_main.size(),kSceneTallLayout,layout,error)||!scene.configure(layout,error)||!scene.establishSceneReset(kSceneTallLayout,error)||!scene.establishFreshPalette(error)||!scene.setTransferChannel(0,error))return false;scene.establishFreshModuleDialogueContext();scene.establishSceneFlags(0);_phase=kStartupNativeTitleMusic;_trackPosted=false;return true;
 }
 scene.setCdClock(frame.cdKnown,frame.cdClock);
 if(!scene.beginFrame(false,error))return false;
 bool admitted=false;if(!scene.advanceCoordinatorClock(error,&admitted))return false;
 if(admitted){
 // DATA_D2's callback updates its previous pressed mask before dispatch,
 // including paint, fade, calibration and return states. Actors still update.
 bool dispatch=true;if(_phase!=kStartupNativeCinematicFirst&&_phase!=kStartupNativeCinematicSecond&&_phase!=kStartupNativeSkipFade){uint8_t old=_previousModule;_previousModule=frame.pressed;dispatch=!frame.pressed||frame.pressed!=old;}
 if(dispatch)switch(_phase){
 case kStartupNativeTitleMusic:
  if(!_trackPosted){_tracks.push_back(2);_trackPosted=true;}
  if(frame.cdKnown&&frame.cdTrack==2)_phase=kStartupNativeTitleInstall;break;
 case kStartupNativeTitleInstall:
  if(!scene.preparePresentationReplacement(error)||!scene.requestVisualCommand(1,error)||!scene.queueScrollPosition(0,kScrollY,256,error)||!scene.install(4,_title,0,error))return false;_delay=24;_phase=kStartupNativeTitleDelay;break;
 case kStartupNativeTitleDelay:
  if(--_delay==0){if(!scene.requestVisualCommand(7,error))return false;_delay=4;_phase=kStartupNativeTitleFlashDelay;}break;
 case kStartupNativeTitleFlashDelay:
  if(--_delay==0){if(!palette(_title,0x8e6,scene,error))return false;_delay=86;_phase=kStartupNativeTitleReveal;}break;
 case kStartupNativeTitleReveal:
  if(--_delay==0){if(!selectTitle(scene,error))return false;_phase=kStartupNativeTitleReady;}break;
 case kStartupNativeTitleReady:{
  uint8_t pressed=frame.pressed;uint8_t previous=_previousTitle;_previousTitle=pressed;if(pressed&&pressed==previous)break;
  if(_savedGames&&(pressed&3)){_continueSelected=!_continueSelected;if(!selectTitle(scene,error))return false;}
  if(pressed&0x80){if(!scene.setActorFlags(16,3,error)||!scene.setActorFlags(17,3,error))return false;_fm.push_back(0x38);_paint=0;
   if(_savedGames&&_continueSelected){if(!_selector.start(_storage,error))return false;_phase=kStartupNativeLoadPaint;}else{_loadedSlot=0;_phase=kStartupNativeOptionsPaint;}}
  break;}
 case kStartupNativeLoadPaint:{
  std::vector<SceneGraphicsTask> tasks;if(!readStartupPaint(_title,kStartupPaintLoadOpen,_paint,tasks,error))return false;
  // Original028D26 edits B8000, the first name staging canvas after decompression,
  // before its first metadata-bearing copy; later tasks inherit that canvas.
  for(SceneGraphicsTask& task:tasks)if(((task.flags>>4)&3)==0)for(std::vector<uint8_t>& source:task.stagingSources)if(source.size()==2048&&!decorateStartupSlotMap(source,_storage,error))return false;
  if(!scene.queueGraphicsTasks(tasks,error))return false;if(++_paint==21){if(!applySlotSelection(scene,true,error))return false;_phase=kStartupNativeLoadPoll;}break;}
 case kStartupNativeLoadPoll:{
  StartupSlotAction action;if(!_selector.poll(frame.pressed,action,error)||!applySlotSelection(scene,true,error))return false;
  if(action==kStartupSlotRead){_fm.push_back(0x38);_phase=kStartupNativeLoadPending;}
  else if(action==kStartupSlotCancel){_cancelLoad=true;_paint=0;if(!applySlotSelection(scene,false,error))return false;_phase=kStartupNativeLoadClose;}break;}
 case kStartupNativeLoadPending:break;
 case kStartupNativeLoadClose:{
  if(!applySlotSelection(scene,false,error))return false;std::vector<SceneGraphicsTask> tasks;if(!readStartupPaint(_title,kStartupPaintClose,_paint,tasks,error)||!scene.queueGraphicsTasks(tasks,error))return false;
  if(++_paint==20){_paint=0;if(_cancelLoad){SceneGraphicsTask task;if(!readSceneGraphicsTask(_title.data(),_title.size(),0x1806,0x28000,task,error)||!scene.queueGraphicsTasks(std::vector<SceneGraphicsTask>(1,task),error))return false;_delay=2;_phase=kStartupNativeLoadCancelDelay;}else _phase=kStartupNativeOptionsPaint;}break;}
 case kStartupNativeLoadCancelDelay:
  if(--_delay==0){_continueSelected=true;_previousTitle=0;if(!selectTitle(scene,error))return false;_phase=kStartupNativeTitleReady;}break;
 case kStartupNativeOptionsPaint:{std::vector<SceneGraphicsTask> tasks;if(!readStartupPaint(_title,kStartupPaintOptionsOpen,_paint,tasks,error)||!scene.queueGraphicsTasks(tasks,error))return false;if(++_paint==24)_phase=kStartupNativeOptionsPoll;break;}
 case kStartupNativeOptionsPoll:{StartupOptionsUpdate update;if(!_options.poll(frame.pressed,update,error)||!_options.applyActorSelection(scene,error))return false;_fm.insert(_fm.end(),update.fmCommands.begin(),update.fmCommands.end());_pcm.insert(_pcm.end(),update.pcmCommands.begin(),update.pcmCommands.end());
  if(update.action==kStartupOptionsExit){if(!palette(_title,0x10f2,scene,error))return false;_delay=10;_phase=kStartupNativeOptionsFade;}break;}
 case kStartupNativeOptionsFade:
  if(--_delay==0){if(!scene.preparePresentationReplacement(error))return false;if(_loadedSlot)_phase=kStartupNativeLoaded;else{if(!_cinematic.start(3,_first,error))return false;_phase=kStartupNativeCinematicFirst;_trackPosted=false;}}break;
 case kStartupNativeCinematicFirst:case kStartupNativeCinematicSecond:{
  StartupCinematicEvent event;if(!_cinematic.poll(frame.cdKnown,frame.cdTrack,frame.cdClock,(frame.pressed&0x80)!=0||_mouseSkipRequested,event,error))return false;if(event.action==kStartupCinematicNone)break;
  if(event.action==kStartupCinematicSkip||event.action==kStartupCinematicComplete)_mouseSkipRequested=false;
  const std::vector<uint8_t>& bytes=_phase==kStartupNativeCinematicFirst?_first:_second;
  if(event.action==kStartupCinematicTrack){if(!_trackPosted){_tracks.push_back(event.value);_trackPosted=true;}if(!frame.cdKnown||frame.cdTrack!=event.value)break;_trackPosted=false;}
  else if(event.action==kStartupCinematicScene){if(!install(scene,event.resource,bytes,event.value,error))return false;if(event.resource==3&&event.auxiliary){if(!scene.requestVisualCommand(1,error)||!scene.queueScrollPosition(0,kScrollY,int16_t(event.auxiliary),error))return false;}else if(event.resource==2&&event.auxiliary){if(!scene.setPlaneAllocation(1,event.auxiliary==0xff?0xe000:0xc000,error))return false;}}
  else if(event.action==kStartupCinematicPalette){if(!palette(bytes,event.paletteOffset,scene,error))return false;}
  else if(event.action==kStartupCinematicVisual){if(!scene.requestVisualCommand(uint16_t(event.value),error))return false;}
  else if(event.action==kStartupCinematicActorFlags){if(!scene.setActorFlags(event.value,uint8_t(event.auxiliary),error))return false;}
  else if(event.action==kStartupCinematicDisplayFlag){if(event.value!=0x10){error="unknown cinematic transfer ownership flag";return false;}_soundDriverSuspended=true;}
  else if(event.action==kStartupCinematicSkip){if(!palette(_subcode,0x14b1c-0xd400,scene,error))return false;}
  else if(event.action==kStartupCinematicComplete){if(!scene.preparePresentationReplacement(error))return false;}
  if(!_cinematic.complete(event,error))return false;
  if(_cinematic.finished()){if(event.action==kStartupCinematicSkip){_delay=10;_phase=kStartupNativeSkipFade;}else if(_phase==kStartupNativeCinematicFirst){if(!_cinematic.start(2,_second,error))return false;_phase=kStartupNativeCinematicSecond;_trackPosted=false;}else _phase=kStartupNativeComplete;_soundDriverSuspended=false;}break;
 }
 case kStartupNativeSkipFade:
  if(--_delay==1){SceneGraphicsTask task;if(!readSceneGraphicsTask(_subcode.data(),_subcode.size(),0x14b4e-0xd400,0xd400,task,error)||!scene.queueGraphicsTasks(std::vector<SceneGraphicsTask>(1,task),error))return false;}
  else if(_delay==0){if(!scene.preparePresentationReplacement(error))return false;_phase=kStartupNativeComplete;_soundDriverSuspended=false;}break;
 default:break;
 }
 }
 if(!scene.updateObjects(false,error))return false;if(scene.graphicsServiceBusy()&&!scene.commitTransfers(error))return false;return true;
}
bool StartupPresenter::renderPcOptions(SceneColorEncoding encoding,SceneImage& image,std::string& error)const{
 StartupSettings settings;if(!_options.settings(settings,error))return false;SceneImage next;next.width=256;next.height=224;next.indexed.assign(256*224,0);
 auto text=[&](const std::string& value,int x,int y,uint8_t color){for(unsigned char c:value){FontGlyph glyph;if(!_pcOptionsFont.glyph(c,glyph,error)||!drawGlyph(_pcOptionsFont,c,x,y,color,next.indexed,256,224,error))return false;x+=glyph.advance;}return true;};
 if(!text("SNATCHER OPTIONS",40,24,1)||!text("CONTROLS: "+std::to_string(settings.controlMapping+1),40,72,_options.row()==1?2:1)||!text(settings.mono?"SOUND: MONO":"SOUND: STEREO",40,112,_options.row()==2?2:1)||!text("CONTINUE",40,152,_options.row()==4?2:1)||!text("ARROWS / CLICK TO CHANGE",16,192,1))return false;
 if(!text(">",24,_options.row()==1?72:_options.row()==2?112:152,2))return false;
 // This is the explicitly native PC system UI, not an original-frame repair.
 uint16_t colors[]={0,0xeee,0xee};unsigned scale=_phase==kStartupNativeOptionsFade?_delay:10;next.rgb.resize(256*224*3);
 for(size_t i=0;i<next.indexed.size();++i){uint16_t color=colors[next.indexed[i]];if(scale<10){unsigned r=((color>>0)&14)*scale/10,g=((color>>4)&14)*scale/10,b=((color>>8)&14)*scale/10;color=uint16_t((r&14)|((g&14)<<4)|((b&14)<<8));}if(!sceneColor(color,encoding,next.rgb[i*3],next.rgb[i*3+1],next.rgb[i*3+2],error))return false;}
 image=next;return true;
}
bool StartupPresenter::render(SceneRuntime& scene,SceneColorEncoding encoding,SceneImage& image,std::string& error){if(_phase==kStartupNativeOptionsPaint||_phase==kStartupNativeOptionsPoll||_phase==kStartupNativeOptionsFade)return renderPcOptions(encoding,image,error);return _phase==kStartupNativeLicense?_license.render(encoding,image,error):scene.render(encoding,image,error);}
unsigned startupPaintSteps(StartupPaintKind kind){return kind==kStartupPaintOptionsOpen?24:kind==kStartupPaintLoadOpen?21:kind==kStartupPaintClose?20:0;}
bool readStartupPaint(const std::vector<uint8_t>& bytes,StartupPaintKind kind,unsigned step,std::vector<SceneGraphicsTask>& out,std::string& error){
 error.clear();if(step>=startupPaintSteps(kind)){error="startup paint step outside source table";return false;}
 size_t primary=kind==kStartupPaintOptionsOpen?0x1292:kind==kStartupPaintLoadOpen?0x1142:0x16c6;
 size_t secondary=kind==kStartupPaintOptionsOpen?0x1554:0x1412;std::vector<SceneGraphicsTask> tasks;
 for(unsigned n=0;n<((kind!=kStartupPaintClose&&step)?2u:1u);++n){size_t table=n?secondary:primary;size_t at=table+step*2;
  if(at>bytes.size()||bytes.size()-at<2){error="truncated startup paint table";return false;}
  uint16_t raw=uint16_t(unsigned(bytes[at])<<8|bytes[at+1]);int relative=raw<0x8000?int(raw):int(raw)-65536;
  if(relative<0&&size_t(-relative)>table){error="startup paint pointer precedes resource";return false;}size_t target=relative<0?table-size_t(-relative):table+size_t(relative);
  SceneGraphicsTask task;if(!readSceneGraphicsTask(bytes.data(),bytes.size(),target,0x28000,task,error))return false;tasks.push_back(task);
 }
 out.swap(tasks);return true;
}
}
