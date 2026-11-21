/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "shooting.h"
#include <sstream>
#include "runtime.h"
namespace Snatcher {
uint16_t drawRandom(RandomWords& state){
 const unsigned a=state.first,b=state.second;
 unsigned nextA=(((((a>>8)+(a&255))&255)<<8)|(a&255))+b+((b&255)<<8)+(b>>8);
 const unsigned mixedHigh=(b>>8)+(b&255);
 unsigned nextB=((mixedHigh&255)<<8)|(b&255);
 nextA+=mixedHigh>>8;++nextB;nextA+=nextB>>16;
 state.first=uint16_t(nextA);state.second=uint16_t(nextB);return state.first;
}
uint16_t drawRandomRange(RandomWords& state,uint16_t lower,uint16_t span){return uint16_t(uint32_t(lower)+((uint32_t(drawRandom(state))*span)>>16));}
bool quantizeShootingAim(unsigned x,unsigned y,unsigned& cell,std::string& error){
 cell=0;error.clear();if(x>255||y>255){error="Shooting aim is outside normalized byte domain";return false;}
 cell=(x>=160?2:x>=96?1:0)+3*(y>=88?2:y>=48?1:0);return true;
}
bool shootingHitRegionAt(int16_t x,int16_t y,unsigned& region,std::string& error){
 error.clear();if(x<0||x>255||y<0||y>255){error="Absolute shooting position is outside source hit-region domain";return false;}
 const int edgesX[]={0,96,160,256},edgesY[]={0,48,88,256};
 for(unsigned row=0;row<3;++row)for(unsigned column=0;column<3;++column)
  if(x>=edgesX[column]&&x<edgesX[column+1]&&y>=edgesY[row]&&y<edgesY[row+1]){region=row*3+column;return true;}
 error="Absolute shooting position has no original hit region";return false;
}
bool readShootingAimLayout(const uint8_t* data,size_t size,ShootingAimLayout& layout,std::string& error){
 error.clear();const size_t offset=0x11fe0-0xd400;if(!data||size<offset+12){error="shooting aim table exceeds SUBCODE extent";return false;}
 ShootingAimLayout staged;for(unsigned i=0;i<3;++i){staged.x[i]=uint16_t(unsigned(data[offset+2*i])<<8|data[offset+2*i+1]);staged.y[i]=uint16_t(unsigned(data[offset+6+2*i])<<8|data[offset+7+2*i]);}layout=staged;return true;
}
bool digitalShootingAim(uint8_t held,const ShootingAimLayout& layout,uint16_t& x,uint16_t& y,unsigned& cell,std::string& error){
 error.clear();const int horizontal=int((held&8)!=0)-int((held&4)!=0),vertical=int((held&2)!=0)-int((held&1)!=0);
 x=layout.x[1+horizontal];y=layout.y[1+vertical];cell=unsigned(4+horizontal+3*vertical);return true;
}
bool targetCellFromDraw(uint16_t draw,const uint8_t *table,size_t count,unsigned& cell,std::string& error){
 cell=0;error.clear();if(!table||count!=32){error="Shooting cell distribution must contain 32 entries";return false;}
 for(size_t i=0;i<count;++i)if(table[i]>8){error="Shooting distribution contains invalid target slot";return false;}
 cell=table[draw&31];return true;
}
TargetHitClass markTargetHit(uint8_t& value){unsigned phase=value&15;if(phase<2||phase>=5)return kTargetPhaseIneligible;value=uint8_t((value&0xf0)|6);return value&0x10?kTargetTypeBitSet:kTargetTypeBitClear;}
uint16_t decreaseTimingAdjustment(uint16_t current){return current>=2?uint16_t(current-2):current;}
uint16_t increaseTimingAdjustment(uint16_t current,uint16_t mode,bool alternate){unsigned limit=(mode==0||mode==11||!alternate)?32:48;return current<limit?uint16_t(current+1):current;}
uint16_t advanceMissCounter(uint16_t current){uint16_t next=uint16_t(uint32_t(current)+1);return next?next:0xff;}
bool readShootingModeProfiles(const uint8_t *data,size_t size,std::vector<ShootingModeProfile>& profiles,std::string& error){
 profiles.clear();error.clear();const size_t offsets[]={0x4b10,0x4b30,0x4b50,0x4b60,0x4b70,0x4bd0};
 if(!data||size<offsets[5]+15){error="Shooting mode tables exceed SUBCODE extent";return false;}
 std::vector<ShootingModeProfile> out;
 for(unsigned i=0;i<15;++i){ShootingModeProfile p;p.hitTypeSetSound=data[offsets[0]+i];p.fireSound=data[offsets[1]+i];p.initialPhase=data[offsets[2]+i];p.initialSpawnDelay=data[offsets[3]+i];p.initialTiming=data[offsets[4]+i];p.policy=data[offsets[5]+i];
  if(p.initialPhase>5){error="Shooting mode initial phase outside coordinator dispatch";return false;}out.push_back(p);
 }profiles.swap(out);return true;
}
bool resolveShootingShot(unsigned mode,const std::vector<ShootingModeProfile>& profiles,bool alternate,ShootingShotState& state,ShootingShotEffects& effects,std::string& error){
 effects=ShootingShotEffects();error.clear();if(mode>=15||profiles.size()!=15){error="Shooting mode/profile extent";return false;}
 if(mode==10){error="Shooting mode 10 grid reaction and damage ownership unresolved";return false;}
 ShootingShotState next=state;ShootingShotEffects result;const ShootingModeProfile& p=profiles[mode];result.hitClass=markTargetHit(next.targetPhaseAndType);
 if(result.hitClass==kTargetTypeBitClear){next.typeClearHits=uint16_t(uint32_t(next.typeClearHits)+1);next.timingAdjustment=decreaseTimingAdjustment(next.timingAdjustment);if(!p.policy){result.soundRequested=true;result.sound=14;}}
 else if(result.hitClass==kTargetTypeBitSet){if((next.typeSetHits&7)==7)next.timingAdjustment=decreaseTimingAdjustment(next.timingAdjustment);next.typeSetHits=uint16_t(uint32_t(next.typeSetHits)+1);result.reactTypeSetActor=mode==0||mode==11;result.soundRequested=true;result.sound=p.hitTypeSetSound;}
 else {if(alternate||!p.policy)next.timingAdjustment=increaseTimingAdjustment(next.timingAdjustment,uint16_t(mode),alternate);next.misses=advanceMissCounter(next.misses);}
 result.requestModeNineRefresh=result.hitClass!=kTargetPhaseIneligible&&mode==9;result.releaseFireActor=true;state=next;effects=result;return true;
}

bool acceptShootingFire(unsigned mode,const ShootingFireContext& c,ShootingFireState& state,bool& accepted,std::string& error){
 accepted=false;error.clear();if(mode>=15||state.phase!=0){error="shooting fire gate mode/entry phase unsupported";return false;}
 uint8_t edges=c.alternateInput?c.alternateEdges:c.primaryEdges;
 if(c.actorActive||c.actorTarget!=0xff||c.encounterPhase==4||c.aimPulse!=1||(edges&0x40)||!(edges&0x20))return true;
 ShootingFireState next=state;if(mode==14)next.modeFourteenLatch=1;next.shots=uint16_t(uint32_t(next.shots)+1);next.phase=4;if(c.alternateInput)next.aimUpdateCounter=1;state=next;accepted=true;return true;
}
bool completeShootingFire(unsigned mode,uint16_t frame,ShootingFireState& state,ShootingFireCompletion& effects,std::string& error){
 effects=ShootingFireCompletion();error.clear();if(mode>=15||state.phase!=2){error="shooting fire completion mode/entry phase unsupported";return false;}
 if(frame<4)return true;ShootingFireCompletion result;result.completed=true;
 if(mode==14){result.encounterTransition=true;result.soundRequested=true;result.sound=0x47;result.nextEncounterPhase=3;result.clearedEncounterSubphase=0;result.typeClearHits=1;}
 else state.phase=3;effects=result;return true;
}

bool readShootingFireLayout(const uint8_t* sub,size_t size,ShootingFireLayout& layout,std::string& error){
 error.clear();const size_t at=0x11fec-0xd400;if(!sub||size<at+36){error="shooting fire layout exceeds SUBCODE extent";return false;}ShootingFireLayout next;
 for(unsigned i=0;i<9;++i){next.x[i]=uint16_t(unsigned(sub[at+i*2])<<8|sub[at+i*2+1]);next.y[i]=uint16_t(unsigned(sub[at+18+i*2])<<8|sub[at+19+i*2]);}layout=next;return true;
}
bool advanceShootingFire(unsigned mode,uint16_t timing,unsigned aimCell,const ShootingFireLayout& layout,const std::vector<ShootingModeProfile>& profiles,ShootingFireState& state,ShootingFireLaunch& effects,std::string& error){
 effects=ShootingFireLaunch();error.clear();if(mode>=15||profiles.size()!=15||aimCell>=9||(state.phase!=1&&state.phase!=4&&state.phase!=5)){error="shooting fire launch mode/aim/phase unsupported";return false;}
 ShootingFireState next=state;ShootingFireLaunch result;
 if(next.phase==4&&timing>=16){next.phase=5;state=next;return true;}
 if(next.phase==4||next.phase==5){next.phase=1;if(timing>=4){state=next;return true;}}
 result.launched=result.soundRequested=true;result.cell=uint8_t(mode==14?4:aimCell);result.sound=profiles[mode].fireSound;result.x=layout.x[result.cell];result.y=layout.y[result.cell];next.phase=2;state=next;effects=result;return true;
}
namespace {
void shootingPut(std::vector<uint8_t>& b,uint32_t value,unsigned width){for(unsigned i=width;i;--i)b.push_back(uint8_t(value>>((i-1)*8)));}
uint32_t shootingGet(const std::vector<uint8_t>& b,size_t& cursor,unsigned width){uint32_t value=0;for(unsigned i=0;i<width;++i)value=(value<<8)|b[cursor++];return value;}
bool shootingValid(const ShootingSnapshot& s,std::string& e){if(!s.profileIdentity||s.mode>=15||s.aimCell>=9||s.fire.phase>5){e="shooting snapshot profile/mode/aim/fire phase invalid";return false;}return true;}
}
uint32_t shootingProfileIdentity(const std::vector<ShootingModeProfile>& profiles){
 if(profiles.size()!=15)return 0;uint32_t value=2166136261u;for(const ShootingModeProfile& p:profiles){const uint8_t fields[]={p.hitTypeSetSound,p.fireSound,p.initialPhase,p.initialSpawnDelay,p.initialTiming,p.policy};for(uint8_t field:fields)value=(value^field)*16777619u;}return value?value:1;
}
bool encodeShootingState(const ShootingSnapshot& state,NativeSaveSection& section,std::string& error){
 error.clear();if(!shootingValid(state,error))return false;NativeSaveSection next(kSaveShooting);next.required=true;const uint8_t header[]={'S','N','S','H',1,state.mode,state.aimCell,uint8_t(state.alternateInput),state.aimX,state.aimY};next.bytes.assign(header,header+10);shootingPut(next.bytes,state.profileIdentity,4);
 const uint16_t words[]={state.randomState.first,state.randomState.second,state.fire.phase,state.fire.shots,state.fire.aimUpdateCounter,state.fire.modeFourteenLatch,state.shot.timingAdjustment,state.shot.typeClearHits,state.shot.typeSetHits,state.shot.misses};for(uint16_t word:words)shootingPut(next.bytes,word,2);next.bytes.push_back(state.shot.targetPhaseAndType);section=next;return true;
}
bool decodeShootingState(const NativeSaveSection& section,uint32_t expected,ShootingSnapshot& state,std::string& error){
 error.clear();const std::vector<uint8_t>& b=section.bytes;if(section.id!=kSaveShooting||section.version!=1||b.size()!=35||b[0]!='S'||b[1]!='N'||b[2]!='S'||b[3]!='H'||b[4]!=1||b[7]>1){error="shooting snapshot section/header/extent invalid";return false;}ShootingSnapshot next;next.mode=b[5];next.aimCell=b[6];next.alternateInput=b[7]!=0;next.aimX=b[8];next.aimY=b[9];size_t cursor=10;next.profileIdentity=shootingGet(b,cursor,4);
 uint16_t* words[]={&next.randomState.first,&next.randomState.second,&next.fire.phase,&next.fire.shots,&next.fire.aimUpdateCounter,&next.fire.modeFourteenLatch,&next.shot.timingAdjustment,&next.shot.typeClearHits,&next.shot.typeSetHits,&next.shot.misses};for(uint16_t* word:words)*word=uint16_t(shootingGet(b,cursor,2));next.shot.targetPhaseAndType=b[cursor];if(next.profileIdentity!=expected){error="shooting snapshot source profile identity mismatch";return false;}if(!shootingValid(next,error))return false;state=next;return true;
}

bool readShootingEncounterData(const uint8_t* sub,size_t size,ShootingEncounterData& data,std::string& error){
 error.clear();const size_t base=0xd400;if(!sub||size<0x12ebc-base+12){error="shooting encounter source table extent";return false;}ShootingEncounterData next;if(!readShootingModeProfiles(sub,size,next.profiles,error))return false;
 for(unsigned i=0;i<15;++i){ShootingEncounterRule rule={sub[0x11f40-base+i],sub[0x11f20-base+i],sub[0x11f80-base+i],sub[0x11f90-base+i],sub[0x11fa0-base+i],sub[0x11fb0-base+i],sub[0x11fc0-base+i]};next.rules.push_back(rule);}
 for(unsigned i=0;i<32;++i){next.cellDistribution[i]=sub[0x12010-base+i];if(next.cellDistribution[i]>8){error="shooting source distribution outside nine cells";return false;}}
 for(unsigned i=0;i<9;++i){next.typeDistribution[i]=sub[0x12c9a-base+i];next.gridTypes[i]=sub[0x12ce2-base+i];}for(unsigned i=0;i<6;++i)next.healthDestinations[i]=int16_t(uint16_t(unsigned(sub[0x12ebc-base+2*i])<<8|sub[0x12ebc-base+2*i+1]));next.loaded=true;data=next;return true;
}
namespace {
bool encounterValid(const ShootingEncounterData& data,const ShootingEncounterState& state,std::string& error){if(!data.loaded||data.profiles.size()!=15||data.rules.size()!=15||!state.initialized||state.mode>=15||state.phase!=2||state.spawnPhase>2||state.selectedCell>=9||state.activeTargets>9){error="shooting encounter source/state ownership invalid";return false;}return true;}
bool cellDraw(const ShootingEncounterData& data,NarrativeRandomSource& source,uint16_t& value,std::string& error){uint16_t draw;if(!source.next(draw,error)){if(error.empty())error="shared shooting RNG owner unavailable";return false;}value=data.cellDistribution[draw&31];return true;}
bool targetDelay(const ShootingEncounterData& data,NarrativeRandomSource& source,uint16_t timing,ShootingTargetState& target,std::string& error){uint16_t cell;if(!cellDraw(data,source,cell,error))return false;target.timer=uint16_t(cell*2+timing);return true;}
bool targetAdvance(const ShootingEncounterData& data,NarrativeRandomSource& source,uint16_t timing,unsigned marker,ShootingTargetState& target,bool& advanced,std::string& error){advanced=false;if(target.record!=target.markers[marker])return true;target.record=uint16_t(target.record+2);target.phaseAndType=uint8_t(target.phaseAndType+1);advanced=true;return targetDelay(data,source,timing,target,error);}
bool targetForce(const ShootingEncounterData& data,NarrativeRandomSource& source,uint16_t timing,unsigned phase,ShootingTargetState& target,std::string& error){target.phaseAndType=uint8_t((target.phaseAndType&0xf0)|phase);target.record=uint16_t(target.markers[phase-1]+2);return targetDelay(data,source,timing,target,error);}
}
bool initializeShootingEncounter(const ShootingEncounterData& data,unsigned mode,bool alternate,uint16_t priorTiming,ShootingEncounterState& state,std::string& error){
 error.clear();if(!data.loaded||data.profiles.size()!=15||data.rules.size()!=15||mode>=15){error="shooting encounter initialization source/mode invalid";return false;}ShootingEncounterState next;next.initialized=true;next.alternateInput=alternate;next.mode=uint8_t(mode);next.phase=data.profiles[mode].initialPhase;next.timing=priorTiming?priorTiming:data.profiles[mode].initialTiming;next.spawnDelay=data.profiles[mode].initialSpawnDelay;next.modeFourteenLatch=state.modeFourteenLatch;
 for(unsigned i=0;i<9;++i){next.targets[i]=state.targets[i];next.targets[i].record=2;next.targets[i].remaining=0;next.targets[i].phaseAndType=0;}
 // The original optional reset repeats the first active-word store nine times;
 // it does not advance its target pointer. Preserve that observed narrow effect.
 if(data.profiles[mode].policy)next.targets[0].active=false;state=next;return true;
}
bool tickShootingTargets(const ShootingEncounterData& data,NarrativeRandomSource& source,ShootingEncounterState& state,std::string& error){
 error.clear();if(!encounterValid(data,state,error))return false;if(state.mode==14){error="timed shooting mode has no ordinary target update";return false;}ShootingEncounterState next=state;next.activeTargets=0;const ShootingModeProfile& profile=data.profiles[next.mode];const ShootingEncounterRule& rule=data.rules[next.mode];const uint16_t goal=next.alternateInput?rule.alternateHitGoal:rule.hitGoal;
 for(unsigned i=0;i<9;++i){ShootingTargetState& target=next.targets[i];if(target.cleared){if(target.active||target.phaseAndType||target.markersKnown){error="shooting cleared target contradicts actual empty allocation";return false;}continue;}if(!target.markersKnown){error="shooting target animation marker owner unavailable";return false;}const unsigned phase=target.phaseAndType==255?0:target.phaseAndType&15;if(phase>7){error="shooting target phase outside source dispatch";return false;}if(phase)++next.activeTargets;
  if(!phase){if(target.markers[0]==2)target.flags=3;else if(target.markers[0]>=128||target.phaseAndType==255){target.phaseAndType=255;target.flags=3;target.active=false;}else target.active=true;continue;}
  bool advanced=false;
  if(phase==1){target.flags=0;if(!targetAdvance(data,source,next.timing,1,target,advanced,error))return false;}
  else if(phase>=2&&phase<=4){target.timer=uint16_t(target.timer-1);if(!(target.timer&0x8000))continue;if(!targetAdvance(data,source,next.timing,phase,target,advanced,error))return false;if(!advanced){target.timer=0;continue;}
   if(phase==2&&profile.policy&&next.typeSetHits>goal){if(!targetForce(data,source,next.timing,5,target,error))return false;}
   else if(phase==3&&profile.policy&&(target.phaseAndType&0xf0)!=0x20&&!next.attackPhase&&!next.cooldown&&next.inputGate<2){next.attackCell=uint16_t(i);next.attackPhase=1;}
   else if(phase==4&&profile.policy){bool force=(profile.policy&128)!=0;if(!force&&next.typeSetHits>goal){uint16_t cell;if(!cellDraw(data,source,cell,error))return false;force=cell<6;}if(force&&!targetForce(data,source,next.timing,3,target,error))return false;}
  }else if(phase==5){if(!targetAdvance(data,source,next.timing,5,target,advanced,error))return false;if(advanced){target.record=uint16_t(target.record-2);if(!profile.policy&&target.phaseAndType>=0x30)next.damage=uint16_t(next.damage+1);target.phaseAndType=0;}}
  else if(phase==6){target.phaseAndType=uint8_t((target.phaseAndType&0xf0)|7);target.record=target.markers[6];if(!targetDelay(data,source,next.timing,target,error))return false;}
  else if(phase==7){if(!targetAdvance(data,source,next.timing,7,target,advanced,error))return false;if(advanced){target.record=uint16_t(target.record-2);target.phaseAndType=0;if(next.mode!=0&&next.mode!=11&&next.mode!=7)target.flags=3;}}
 }state=next;return true;
}
bool tickShootingSpawner(const ShootingEncounterData& data,const std::vector<ShootingTemplateState>& templates,NarrativeRandomSource& source,ShootingEncounterState& state,ShootingEncounterEffects& effects,std::string& error){
 effects=ShootingEncounterEffects();error.clear();if(!encounterValid(data,state,error))return false;if(state.mode==14){error="timed shooting mode has no ordinary target spawner";return false;}ShootingEncounterState next=state;ShootingEncounterEffects out;const ShootingEncounterRule& rule=data.rules[next.mode];const uint16_t goal=next.alternateInput?rule.alternateHitGoal:rule.hitGoal,maximum=next.alternateInput?rule.alternateSimultaneous:rule.simultaneous;
 if(next.spawnPhase==0){next.spawnDelay=uint16_t(next.spawnDelay-1);if(!(next.spawnDelay&0x8000)){state=next;return true;}if((goal&&next.typeSetHits>=goal)||(rule.spawnLimit&&next.spawned==rule.spawnLimit)||next.activeTargets==maximum){state=next;return true;}uint16_t first,second;if(!cellDraw(data,source,first,error)||!cellDraw(data,source,second,error))return false;next.spawnDelay=second>=7?5:uint16_t(first*2+16+next.timing);next.spawnPhase=1;}
 else if(next.spawnPhase==1){uint16_t cell;if(!cellDraw(data,source,cell,error))return false;next.selectedCell=cell;ShootingTargetState& target=next.targets[cell];if(target.phaseAndType==255||(target.phaseAndType&15)){state=next;return true;}unsigned type=1;int templateIndex=-1;
  if(next.mode==0||next.mode==11){uint16_t draw;if(!cellDraw(data,source,draw,error))return false;type=data.typeDistribution[next.mode==0?(draw&3):draw];templateIndex=int(type)+(next.mode==0?-1:1);}
  else if(next.mode==2){unsigned variant=(cell<3?1:0)|next.variant;next.variant=(variant&2)^2;templateIndex=int(variant);type=variant*2+1;}
  else if(next.mode==4)templateIndex=0;
  else if(next.mode==10)type=data.gridTypes[cell];
  if(target.cleared&&templateIndex<0){error="shooting cleared target spawn requires an actual replacement template";return false;}if(type>15){error="shooting source type outside target nibble";return false;}if(templateIndex>=0){if(unsigned(templateIndex)>=templates.size()||!templates[unsigned(templateIndex)].known){error="shooting target template owner unavailable";return false;}const ShootingTemplateState& definition=templates[unsigned(templateIndex)];for(unsigned n=0;n<8;++n)target.markers[n]=definition.markers[n];target.markersKnown=true;target.cleared=false;target.remaining=definition.remaining;target.templateIndex=int16_t(templateIndex);out.templates.push_back(ShootingTemplateRequest(cell,unsigned(templateIndex)));if(next.mode==0||next.mode==11)target.record=2;}
  next.spawnPhase=2;next.spawned=uint16_t(next.spawned+1);target.phaseAndType=uint8_t(type<<4);if(type==2){next.specialTargets=uint16_t(next.specialTargets+1);next.spawned=uint16_t(next.spawned-1);}
 }else{ShootingTargetState& target=next.targets[next.selectedCell];if(target.cleared||!target.markersKnown){error="shooting target activation marker owner unavailable";return false;}target.record=target.markers[0]==2?2:uint16_t(target.markers[0]+2);target.active=true;if((target.phaseAndType&0xf0)!=0x20&&(next.mode==0||next.mode==11))out.modeCounterActorIncrement=true;target.phaseAndType=uint8_t((target.phaseAndType&0xf0)|1);next.spawnPhase=0;if(next.mode==0||next.mode==11)out.musicCommands.push_back(0x22);}
 state=next;effects=out;return true;
}
bool evaluateShootingOutcome(const ShootingEncounterData& data,const ShootingEncounterState& state,ShootingOutcome& outcome,std::string& error){
 outcome=kShootingOutcomePending;error.clear();if(!encounterValid(data,state,error))return false;if(state.mode==14){error="timed shooting mode outcome requires its original timer owner";return false;}const ShootingEncounterRule& rule=data.rules[state.mode];const ShootingModeProfile& profile=data.profiles[state.mode];if(profile.policy&&state.damage>=6){outcome=kShootingOutcomeFailure;return true;}const uint16_t goal=state.alternateInput?rule.alternateHitGoal:rule.hitGoal,maximum=state.alternateInput?rule.alternateSimultaneous:rule.simultaneous;
 if(goal&&state.typeSetHits<goal)return true;if(rule.spawnLimit&&(state.spawnPhase||state.spawned<rule.spawnLimit))return true;if(maximum!=1&&state.attackPhase)return true;if(state.firePhase)return true;for(unsigned i=0;i<9;++i){const uint8_t phase=state.targets[i].phaseAndType;if(phase!=255&&(phase&15))return true;}outcome=kShootingOutcomeReady;return true;
}
bool tickShootingCountdown(ShootingEncounterState& state,ShootingEncounterEffects& effects,std::string& error){
 effects=ShootingEncounterEffects();error.clear();if(!state.initialized||state.phase!=1||(state.mode!=0&&state.mode!=11)||state.firePhase>2){error="shooting countdown owner invalid";return false;}ShootingEncounterState next=state;ShootingEncounterEffects out;
 if(!next.firePhase){out.countdownActorsSetup=true;next.cooldown=128;next.firePhase=1;}
 else if(next.firePhase==1){if(next.cooldown){if(!(next.cooldown&63))out.musicCommands.push_back(0x11);--next.cooldown;}else next.firePhase=2;}
 else{out.countdownActorsRetire=true;out.musicCommands.push_back(0x12);next.firePhase=0;next.phase=2;}
 state=next;effects=out;return true;
}
bool tickShootingTimedMode(ShootingEncounterState& state,ShootingEncounterEffects& effects,bool& cleanupRequested,std::string& error){
 effects=ShootingEncounterEffects();cleanupRequested=false;error.clear();if(!state.initialized||state.mode!=14||state.phase!=2){error="shooting timed mode owner invalid";return false;}ShootingEncounterState next=state;ShootingEncounterEffects out;if(next.modeFourteenLatch){out.forcedActorPosition=true;out.forcedActor=22;out.forcedX=112;out.forcedY=56;}next.specialTimer=uint16_t(next.specialTimer+1);if(!(next.specialTimer&0x8000)&&next.specialTimer>=256){next.phase=3;cleanupRequested=true;}state=next;effects=out;return true;
}

bool startShootingCounterSettlement(uint16_t phase,ShootingCounterSettlement& state,std::string& error){error.clear();if(state.active||phase>2){error="shooting counter settlement owner/phase invalid";return false;}state.active=true;state.phase=phase;return true;}
bool tickShootingCounterSettlement(bool even,uint16_t& first,uint16_t& second,ShootingCounterSettlement& state,ShootingCounterEffects& effects,bool& completed,std::string& error){
 effects=ShootingCounterEffects();completed=false;error.clear();if(!state.active||state.phase>2){error="shooting counter settlement is not active";return false;}if(!even)return true;ShootingCounterSettlement next=state;ShootingCounterEffects out;uint16_t a=first,b=second;
 if(!next.phase){if(!a||(a&0x8000))next.phase=1;else{a=uint16_t(a-1);if(uint16_t(a-b)&0x8000)b=uint16_t(b-1);out.writeRecords=true;}}
 else if(next.phase==1){a=b=0xffff;out.writeRecords=out.hideCounterActors=out.clearCountdownRecords=true;next.phase=2;}
 else{next.phase=0;next.active=false;completed=true;}first=a;second=b;state=next;effects=out;return true;
}

namespace {
ShootingActorMutation& actorEffect(ShootingCoordinatorEffects& out,unsigned actor,unsigned fields){out.actors.push_back(ShootingActorMutation(actor));out.actors.back().fields=fields;return out.actors.back();}
void activeEffect(ShootingCoordinatorEffects& out,unsigned actor,bool active){actorEffect(out,actor,ShootingActorMutation::kActive).active=active;}
void recordEffect(ShootingCoordinatorEffects& out,unsigned actor,uint16_t record){actorEffect(out,actor,ShootingActorMutation::kRecord).record=record;}
void flagEffect(ShootingCoordinatorEffects& out,unsigned actor,uint8_t flags){actorEffect(out,actor,ShootingActorMutation::kFlags).flags=flags;}
void positionEffect(ShootingCoordinatorEffects& out,unsigned actor,int16_t x,int16_t y){auto& a=actorEffect(out,actor,ShootingActorMutation::kX|ShootingActorMutation::kY);a.x=x;a.y=y;}
void paletteEffect(ShootingCoordinatorEffects& out,size_t address){out.resources.push_back(ShootingResourceAction(ShootingResourceAction::kPalette,address-0xd400));}
void graphicsEffect(ShootingCoordinatorEffects& out,size_t address){out.resources.push_back(ShootingResourceAction(ShootingResourceAction::kGraphics,address-0xd400));}
bool realActor(const ShootingCoordinatorInput& input,unsigned actor,std::string& error){if(actor>=input.actors.size()||!input.actors[actor].owned){error="shooting scene actor ownership unavailable";return false;}return true;}
bool synchronizeTargets(const ShootingCoordinatorInput& input,ShootingEncounterState& state,std::string& error){for(unsigned i=0;i<9;++i){const auto& a=input.actors[32+i];auto& t=state.targets[i];if(a.owned&&!a.phaseKnown){error="shooting owned actor phase owner unavailable";return false;}if(a.cleared&&(!a.phaseKnown||a.phase||a.owned||a.active||a.markersKnown)){error="shooting source-cleared actor observation is contradictory";return false;}t.active=a.active;t.flags=a.flags;t.record=a.record;t.remaining=a.remaining;t.markersKnown=a.markersKnown;t.cleared=a.cleared;if(a.phaseKnown)t.phaseAndType=a.phase;for(unsigned j=0;j<8;++j)t.markers[j]=a.markers[j];}return true;}
bool targetActorsKnown(const ShootingCoordinatorInput& input,const ShootingEncounterState& state,bool initial,std::string& error){
 for(unsigned i=0;i<9;++i){const auto& a=input.actors[32+i];if(!initial&&a.cleared&&a.phaseKnown&&!a.phase&&!a.owned&&!a.active&&!a.markersKnown)continue;if(!a.owned||!a.markersKnown||!a.phaseKnown||a.cleared){std::ostringstream message;message<<"shooting "<<(initial?"initial":"actual")<<" target/marker owner unavailable actor="<<(32+i)<<" owned="<<a.owned<<" markers="<<a.markersKnown<<" phase_known="<<a.phaseKnown<<" phase="<<unsigned(state.targets[i].phaseAndType);error=message.str();return false;}}return true;
}
void publishTargets(const ShootingEncounterState& state,ShootingCoordinatorEffects& out){for(unsigned i=0;i<9;++i){const auto& t=state.targets[i];if(t.cleared)continue;auto& a=actorEffect(out,32+i,ShootingActorMutation::kActive|ShootingActorMutation::kFlags|ShootingActorMutation::kRecord|ShootingActorMutation::kRemaining);a.active=t.active;a.flags=t.flags;a.record=t.record;a.remaining=t.remaining;}}
void damageEffect(const ShootingEncounterData& data,ShootingCoordinatorState& state,ShootingCoordinatorEffects& out){if(state.encounter.damage>=6)return;if(state.healthRemaining) --state.healthRemaining;graphicsEffect(out,0x12ea8);out.resources.back().overrideValue=true;out.resources.back().value=data.healthDestinations[state.healthRemaining];++state.encounter.damage;}
bool updateAim(const ShootingEncounterData& data,const ShootingAimLayout& layout,const ShootingCoordinatorInput& input,NarrativeRandomSource& rng,ShootingCoordinatorState& state,ShootingCoordinatorEffects& out,std::string& error){
 auto& e=state.encounter;
 const bool mouseEntered=input.absoluteAim&&!state.absoluteAim;state.absoluteAim=input.absoluteAim;
 if(state.absoluteAim){state.aimX=uint16_t(input.mouseX);state.aimY=uint16_t(input.mouseY);}
 if(e.inputGate>=2){if(e.inputGate&2){uint16_t y,x;if(!rng.next(y,error)||!rng.next(x,error))return false;const int16_t dy=int16_t(int(y&15)-8),dx=int16_t(int(x&15)-8);if(!realActor(input,22,error))return false;positionEffect(out,23,dx,dy);positionEffect(out,22,int16_t(input.actors[22].x+dx),int16_t(input.actors[22].y+dy));e.cooldown=uint16_t((e.timing>>2)+24);}e.inputGate=uint16_t(e.inputGate-2);return true;}
 positionEffect(out,23,0,0);
 if(state.inputLocked||e.phase>=4)return true;
 if(state.absoluteAim&&!e.inputGate&&(mouseEntered||(input.pressed&0x20))){e.inputGate=1;state.aimToggle=1;out.effects.push_back(data.rules[e.mode].aimSound);activeEffect(out,22,true);activeEffect(out,23,true);}
 if(input.pressed&0x40){state.aimEnabled=false;e.inputGate^=1;bool raised=e.inputGate!=0;if(raised){out.effects.push_back(data.rules[e.mode].aimSound);state.aimToggle=1;}if(raised&&e.alternateInput){state.aimEnabled=true;activeEffect(out,22,false);}else{activeEffect(out,22,raised);if(!state.absoluteAim){state.aimCell=4;state.aimX=112;state.aimY=56;}positionEffect(out,22,int16_t(state.aimX),int16_t(state.aimY));}activeEffect(out,23,raised);}
 if(e.inputGate&&!state.fire.modeFourteenLatch){if(state.absoluteAim)positionEffect(out,22,int16_t(state.aimX),int16_t(state.aimY));else if(!e.alternateInput){unsigned cell;uint16_t x,y;if(!digitalShootingAim(input.held,layout,x,y,cell,error))return false;state.aimCell=uint16_t(cell);state.aimX=x;state.aimY=y;positionEffect(out,22,int16_t(x),int16_t(y));}}return true;
}
bool updateFire(const ShootingEncounterData& data,const ShootingFireLayout& layout,const ShootingCoordinatorInput& input,ShootingCoordinatorState& state,ShootingCoordinatorEffects& out,std::string& error){
 auto& e=state.encounter;if(!realActor(input,21,error))return false;const auto& fireActor=input.actors[21];
 if(e.alternateInput&&!state.absoluteAim&&state.pointerDelay){++state.pointerDelay;if(state.pointerDelay>=2){state.pointerDelay=0;unsigned cell;if(!quantizeShootingAim(input.pointerX,input.pointerY,cell,error))return false;state.aimCell=uint16_t(cell);}}
 if(!state.fire.phase){ShootingFireContext c;c.actorActive=fireActor.active;c.actorTarget=uint8_t(state.fireTarget);c.encounterPhase=e.phase;c.aimPulse=e.inputGate;c.primaryEdges=c.alternateEdges=input.pressed;c.alternateInput=e.alternateInput;bool accepted;if(!acceptShootingFire(e.mode,c,state.fire,accepted,error))return false;if(accepted){state.pointerDelay=state.fire.aimUpdateCounter;state.shots=state.fire.shots;}}
 else if(state.fire.phase==1||state.fire.phase==4||state.fire.phase==5){ShootingFireLaunch launch;if(!advanceShootingFire(e.mode,e.timing,state.aimCell,layout,data.profiles,state.fire,launch,error))return false;if(launch.launched){unsigned region=launch.cell;if(state.absoluteAim&&e.mode!=14&&!shootingHitRegionAt(int16_t(state.aimX),int16_t(state.aimY),region,error))return false;state.fireTarget=uint16_t(region);activeEffect(out,21,true);recordEffect(out,21,0);positionEffect(out,21,state.absoluteAim?int16_t(state.aimX):int16_t(launch.x),state.absoluteAim?int16_t(state.aimY):int16_t(launch.y));out.effects.push_back(launch.sound);}}
 else if(state.fire.phase==2){ShootingFireCompletion done;if(!completeShootingFire(e.mode,fireActor.record,state.fire,done,error))return false;if(done.encounterTransition){e.typeClearHits=1;e.phase=3;state.subphase=0;out.effects.push_back(done.sound);}}
 else if(state.fire.phase==3){if(state.fireTarget>=9){error="shooting accepted shot has no owned target cell";return false;}auto& target=e.targets[state.fireTarget];const TargetHitClass hit=markTargetHit(target.phaseAndType);
  if(hit==kTargetTypeBitClear){++e.typeClearHits;e.timing=decreaseTimingAdjustment(e.timing);if(!data.profiles[e.mode].policy)out.effects.push_back(14);}
  else if(hit==kTargetTypeBitSet){if((e.typeSetHits&7)==7)e.timing=decreaseTimingAdjustment(e.timing);++e.typeSetHits;if(e.mode==0||e.mode==11){activeEffect(out,59,true);flagEffect(out,59,1);if(!realActor(input,59,error))return false;recordEffect(out,59,uint16_t(input.actors[59].record+1));}out.effects.push_back(data.profiles[e.mode].hitTypeSetSound);}
  if(e.mode==10){if(data.gridTypes[state.fireTarget]&2){out.effects.push_back(0x32);out.music.push_back(0x13);damageEffect(data,state,out);}else state.misses=advanceMissCounter(state.misses);}
  else if(hit==kTargetPhaseIneligible){if(e.alternateInput||!data.profiles[e.mode].policy)e.timing=increaseTimingAdjustment(e.timing,e.mode,e.alternateInput);state.misses=advanceMissCounter(state.misses);}
  else if(e.mode==9){state.aimEnabled=false;state.inputLocked=true;}
  state.fireTarget=255;activeEffect(out,21,false);state.fire.phase=0;
 }else{error="shooting fire phase invalid";return false;}
 e.firePhase=state.fire.phase;e.modeFourteenLatch=state.fire.modeFourteenLatch;return true;
}
bool updateAttack(const ShootingEncounterData& data,const ShootingCoordinatorInput& input,ShootingCoordinatorState& state,ShootingCoordinatorEffects& out,std::string& error){
 auto& e=state.encounter;if(!data.profiles[e.mode].policy)return true;
 if(!e.attackPhase){activeEffect(out,24,false);recordEffect(out,24,0);if(e.cooldown)--e.cooldown;return true;}
 if(!realActor(input,24,error)||e.attackCell>=9){error="shooting attack actor/cell owner unavailable";return false;}
 if(e.attackPhase==1){activeEffect(out,24,true);recordEffect(out,24,2);e.attackPhase=2;if(data.rules[e.mode].attackSound)out.effects.push_back(data.rules[e.mode].attackSound);
 }else if(e.attackPhase==2){const auto& a=input.actors[24];if(!a.markersKnown){error="shooting attack terminal marker owner unavailable";return false;}if(a.record==a.markers[0])e.attackPhase=3;}
 else if(e.attackPhase==3){e.inputGate=uint16_t(e.inputGate+56);e.attackPhase=0;activeEffect(out,24,false);if(e.mode==4)out.effects.push_back(0x34);else out.music.push_back(0x13);damageEffect(data,state,out);}
 else{error="shooting attack phase invalid";return false;}return true;
}
}
bool startShootingCoordinator(const ShootingEncounterData& data,unsigned mode,bool alternate,uint16_t inheritedTiming,ShootingCoordinatorState& state,std::string& error){error.clear();if(state.active||!data.loaded||data.profiles.size()!=15||data.rules.size()!=15||mode>=15){error="shooting coordinator source/mode/entry owner invalid";return false;}ShootingCoordinatorState next;next.active=true;next.encounter.mode=uint8_t(mode);next.encounter.alternateInput=alternate;next.encounter.timing=inheritedTiming;state=next;return true;}
bool tickShootingCoordinator(const ShootingEncounterData& data,const ShootingAimLayout& aim,const ShootingFireLayout& layout,const ShootingCoordinatorInput& input,NarrativeRandomSource& rng,ShootingCoordinatorState& state,ShootingCoordinatorEffects& effects,std::string& error){
 if(input.absoluteAim&&(input.mouseX<0||input.mouseX>=256||input.mouseY<0||input.mouseY>=224)){effects=ShootingCoordinatorEffects();error="Absolute shooting input is outside game presentation";return false;}
 error.clear();effects=ShootingCoordinatorEffects();if(!state.active||state.completed||!data.loaded||data.profiles.size()!=15||data.rules.size()!=15||state.encounter.mode>=15||state.encounter.phase>5||state.encounter.attackCell>=9||state.aimCell>=9||(state.fireTarget>=9&&state.fireTarget!=255)||state.healthRemaining>6||state.fire.phase>5||input.actors.size()!=64){error="shooting coordinator source/lifecycle owner invalid";return false;}ShootingCoordinatorState next=state;ShootingCoordinatorEffects out;auto& e=next.encounter;const unsigned mode=e.mode;
 if(!e.phase){
  if(!next.subphase){out.resources.push_back(ShootingResourceAction(ShootingResourceAction::kProbeInput));paletteEffect(out,0x11e7e);next.timer=0;++next.subphase;}
  else if(next.subphase==1){++next.timer;if(next.timer>=2&&input.profileReady){next.timer=0;paletteEffect(out,0x11e6a);if(mode!=0&&mode!=11){graphicsEffect(out,0x11ee6);if(mode!=5)graphicsEffect(out,0x11ed8);graphicsEffect(out,0x11ef4);++next.subphase;}else next.subphase=3;}}
  else if(next.subphase==2){next.capturedScrollX=input.scrollX;next.capturedScrollY=input.scrollY;++next.subphase;}
  else if(next.subphase==3){if(mode!=14&&!targetActorsKnown(input,e,true,error))return false;if(!synchronizeTargets(input,e,error))return false;if(!initializeShootingEncounter(data,mode,e.alternateInput,e.timing,e,error))return false;next.subphase=0;next.timer=0;next.inputLocked=false;next.aimEnabled=false;next.aimToggle=0;next.fireTarget=255;next.fire=ShootingFireState();next.healthRemaining=6;publishTargets(e,out);for(unsigned a:{21u,22u,23u,24u}){activeEffect(out,a,false);flagEffect(out,a,0);}recordEffect(out,21,0);activeEffect(out,48,false);flagEffect(out,48,3);}
  else{error="shooting prelude phase invalid";return false;}
 }else if(e.phase==1){if(!updateAim(data,aim,input,rng,next,out,error))return false;ShootingEncounterEffects countdown;if(!tickShootingCountdown(e,countdown,error))return false;out.music=countdown.musicCommands;if(countdown.countdownActorsSetup)for(unsigned a:{56u,57u}){flagEffect(out,a,1);recordEffect(out,a,1);}if(countdown.countdownActorsRetire)for(unsigned a:{56u,57u})activeEffect(out,a,false);next.fire.phase=e.firePhase;}
 else if(e.phase==2){if(mode!=14&&!targetActorsKnown(input,e,false,error))return false;if(!synchronizeTargets(input,e,error))return false;if(!updateAim(data,aim,input,rng,next,out,error)||!updateFire(data,layout,input,next,out,error))return false;
  if(e.phase==2&&mode==14){ShootingEncounterEffects timed;bool cleanup;if(!tickShootingTimedMode(e,timed,cleanup,error))return false;if(timed.forcedActorPosition)positionEffect(out,22,112,56);if(cleanup)next.subphase=0;}
  else if(e.phase==2){if(!tickShootingTargets(data,rng,e,error))return false;std::vector<ShootingTemplateState> definitions(6);for(unsigned i=0;i<6;++i){const auto& a=input.actors[26+i];definitions[i].known=a.owned&&a.markersKnown;definitions[i].remaining=a.remaining;for(unsigned j=0;j<8;++j)definitions[i].markers[j]=a.markers[j];}ShootingEncounterEffects spawn;if(!tickShootingSpawner(data,definitions,rng,e,spawn,error))return false;out.templates=spawn.templates;out.music.insert(out.music.end(),spawn.musicCommands.begin(),spawn.musicCommands.end());if(spawn.modeCounterActorIncrement){if(!realActor(input,58,error))return false;activeEffect(out,58,true);flagEffect(out,58,1);recordEffect(out,58,uint16_t(input.actors[58].record+1));}if(e.attackPhase==1)positionEffect(out,24,int16_t(layout.x[e.attackCell]),int16_t(layout.y[e.attackCell]));if(!updateAttack(data,input,next,out,error))return false;ShootingOutcome result;if(!evaluateShootingOutcome(data,e,result,error))return false;if(result!=kShootingOutcomePending){if(result==kShootingOutcomeFailure){for(auto& t:e.targets)t.flags|=1;out.resources.push_back(ShootingResourceAction(ShootingResourceAction::kCancelPalettes));e.damage=6;}else{if(data.profiles[mode].policy&&mode!=7)for(auto& t:e.targets)if(t.phaseAndType!=255)t.active=false;if(mode==0||mode==11)for(unsigned a:{56u,57u}){activeEffect(out,a,true);recordEffect(out,a,2);}}e.phase=3;next.subphase=0;}}
  publishTargets(e,out);
 }else if(e.phase==3){next.allSpecialHit=e.specialTargets&&e.specialTargets==e.typeClearHits;if(mode!=0&&mode!=11)paletteEffect(out,e.damage>=6?0x11e8c:0x11e92);activeEffect(out,23,false);activeEffect(out,22,false);next.aimEnabled=false;e.phase=4;next.timer=0;out.music.push_back(255);}
 else if(e.phase==4){if(!next.subphase){if(!input.effectBusy){const auto& rule=data.rules[mode];unsigned maximum=e.alternateInput?rule.alternateSimultaneous:rule.simultaneous;if(maximum==1)++next.subphase;else{++next.timer;if(next.timer>=64)++next.subphase;else if(e.damage<6){if(mode!=14&&!targetActorsKnown(input,e,false,error))return false;if(!synchronizeTargets(input,e,error))return false;if(!updateAim(data,aim,input,rng,next,out,error)||!updateFire(data,layout,input,next,out,error))return false;}}}}
  else if(next.subphase==1){++next.subphase;activeEffect(out,21,false);out.resources.push_back(ShootingResourceAction(ShootingResourceAction::kCancelPalettes));}
  else if(next.subphase==2){++next.subphase;next.timer=0;paletteEffect(out,0x11e7e);}
  else if(next.subphase==3){if(mode==0||mode==11)++next.subphase;else if(++next.timer>=2){next.timer=0;graphicsEffect(out,0x11f02);++next.subphase;}}
  else if(next.subphase==4){if(++next.timer>=2){next.timer=0;e.phase=5;next.subphase=0;paletteEffect(out,0x11e74);}}
  else{error="shooting cleanup phase invalid";return false;}
 }else{if(++next.timer>=2){e.inputGate=0;e.phase=0;e.timing=0;next.aimEnabled=false;next.active=false;next.completed=true;out.completed=true;out.failed=data.profiles[mode].policy&&e.damage>=6;}}
 state=next;effects=out;return true;
}

}
