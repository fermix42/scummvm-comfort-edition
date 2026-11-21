/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "audio_percussion.h"
namespace Snatcher {
bool PcmPercussionAssets::load(const uint8_t* index,size_t indexSize,const uint8_t* drums,size_t drumSize,std::string& error){
 error.clear();std::vector<PcmEffect> effects;std::vector<PcmSample> samples;size_t tail;
 if(!parsePcmEffects(index,indexSize,effects,error)||!parsePcmSamples(drums,drumSize,false,samples,tail,error))return false;
 // Sub160CE/1610C accepts IDs1..12;161F4 uses the same source records and
 // fixed drum-header table, never a currently resident dialogue bank.
 for(unsigned id=1;id<=12;++id){const PcmEffect* effect=nullptr;for(const PcmEffect& item:effects)if(item.id==id)effect=&item;
  PcmSample sample;if(!effect||effect->source!=kDrumSource||!resolvePcmEffect(*effect,samples,std::vector<PcmSample>(),sample,error)){if(error.empty())error="music percussion fixed source unavailable";return false;}}
 std::vector<uint8_t> data(drums,drums+drumSize);_effects.swap(effects);_drums.swap(data);return true;
}
bool PcmPercussionAssets::prepare(const PcmPercussionRequest& request,PreparedPcmEffect& output,bool& sound,std::string& error)const{
 error.clear();sound=false;if(!loaded()||request.track<6||request.track>7||request.level5>31){error="unowned music percussion source/track/level";return false;}
 // Source161F4 deliberately excludes these four IDs; zero and IDs>=13 are
 // rejected by160CE/1610C before reaching playback. They are specified silence.
 if(!request.id||request.id>=13||request.id==6||request.id==8||request.id==9||request.id==10)return true;
 const PcmEffect* record=nullptr;for(const PcmEffect& effect:_effects)if(effect.id==request.id)record=&effect;if(!record){error="music percussion record missing";return false;}
 PcmEffect effect=*record;effect.channel=uint8_t(request.track-6);unsigned level=effect.level,scale=unsigned(request.level5)<<3;
 if(scale)level=(level*scale)>>8;
 if(request.attenuate)level=(level*0x70)>>8;
 if(request.gain!=255)level=(level*request.gain)>>8;
 effect.level=uint8_t(level);PreparedPcmEffect staged;
 if(!preparePcmEffect(effect,_drums.data(),_drums.size(),sourcePcmEffectProfile(effect),request.mono,0,staged,error))return false;
 output=staged;sound=true;return true;
}
bool PcmPercussionOwner::start(const PcmPercussionRequest& request,const PreparedPcmEffect& effect,AudioOwner owner,uint32_t generation,bool& accepted,std::string& error){
 error.clear();accepted=false;if(request.track<6||request.track>7||request.id!=effect.source.id||effect.source.source!=kDrumSource||effect.source.channel!=request.track-6){error="music percussion prepared source/slot mismatch";return false;}
 Slot& slot=_slots[request.track-6];if(slot.token.id&&effect.source.priority<slot.priority)return true;
 AudioToken next;if(!_controller.start(effect.pcm,effect.profile,kAudioMusic,owner,generation,next,error))return false;
 if(slot.token.id){std::string ignored;_controller.cancel(slot.token,ignored);}slot.token=next;slot.priority=effect.source.priority;accepted=true;return true;
}
bool PcmPercussionOwner::consume(const AudioCompletion& completion){for(Slot& slot:_slots)if(slot.token.id&&slot.token.id==completion.token.id&&slot.token.generation==completion.token.generation){slot=Slot();return true;}return false;}
void PcmPercussionOwner::reset(){for(Slot& slot:_slots){if(slot.token.id){std::string ignored;_controller.cancel(slot.token,ignored);}slot=Slot();}}
size_t PcmPercussionOwner::activeCount()const{return size_t(_slots[0].token.id!=0)+size_t(_slots[1].token.id!=0);}
}
