/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "audio_rate.h"
#include "native_archive.h"
#include <limits>
namespace Snatcher {
namespace {
bool valid(const RationalAudioRate& rate,std::string& error){
 if(!rate.numerator||!rate.denominator||rate.numerator<rate.denominator||uint64_t(rate.numerator)>uint64_t(rate.denominator)*384000){error="audio rate outside positive 1..384000 domain";return false;}return true;
}
bool multiply(uint64_t a,uint64_t b,uint64_t& out){if(b&&a>std::numeric_limits<uint64_t>::max()/b)return false;out=a*b;return true;}
bool add(uint64_t a,uint64_t b,uint64_t& out){if(a>std::numeric_limits<uint64_t>::max()-b)return false;out=a+b;return true;}
// Exact ceil(a*b/d), where d is at most UINT32_MAX. Binary long division
// keeps each remainder below d; it never requires a nonportable 128-bit type.
bool scaled(uint64_t a,uint64_t b,uint32_t d,uint64_t& out,std::string& error){
 out=0;uint64_t whole;if(!multiply(a/d,b,whole)){error="audio duration/frame count overflow";return false;}
 uint64_t fraction=0,remainder=0,part=a%d;
 for(int bit=63;bit>=0;--bit){fraction*=2;remainder*=2;if((b>>bit)&1)remainder+=part;fraction+=remainder/d;remainder%=d;}
 uint64_t result;if(!add(whole,fraction,result)||(remainder&&!add(result,1,result))){error="audio duration/frame count overflow";return false;}
 out=result;return true;
}
}
bool normalizeAudioRate(RationalAudioRate& rate,std::string& error){error.clear();if(!valid(rate,error))return false;uint32_t a=rate.numerator,b=rate.denominator;while(b){uint32_t remainder=a%b;a=b;b=remainder;}rate.numerator/=a;rate.denominator/=a;return true;}
bool nearestMixerRate(const RationalAudioRate& rate,uint32_t& rounded,std::string& error){rounded=0;error.clear();if(!valid(rate,error))return false;rounded=uint32_t((uint64_t(rate.numerator)+rate.denominator/2)/rate.denominator);return true;}
bool resampledFrameCount(uint64_t frames,const RationalAudioRate& source,uint32_t target,uint64_t& output,std::string& error){output=0;error.clear();if(!valid(source,error))return false;if(!target||target>384000){error="target audio rate outside 1..384000 domain";return false;}return scaled(frames,uint64_t(target)*source.denominator,source.numerator,output,error);}
bool audioDurationNanoseconds(uint64_t frames,const RationalAudioRate& source,uint64_t& output,std::string& error){output=0;error.clear();if(!valid(source,error))return false;return scaled(frames,uint64_t(1000000000)*source.denominator,source.numerator,output,error);}
RationalPcmTransport::RationalPcmTransport():_outputRate(0),_step(0),_position(0),_boundary(0),_rendered(0),_consumed(0){_last[0]=_last[1]=_current[0]=_current[1]=0;}
bool RationalPcmTransport::start(const std::shared_ptr<NativeAudioFeed>& source,const RationalAudioRate& input,uint32_t outputRate,std::string& error){
 return bind(source,input,outputRate,false,error);
}
bool RationalPcmTransport::bind(const std::shared_ptr<NativeAudioFeed>& source,const RationalAudioRate& input,uint32_t outputRate,bool continuation,std::string& error){
 error.clear();RationalAudioRate rate=input;uint32_t nominal=0;
 if(_source||!source||(!continuation&&source->ended())||!source->error().empty()||!normalizeAudioRate(rate,error)||!nearestMixerRate(rate,nominal,error)||source->sourceRate()!=nominal||!outputRate||outputRate>384000){if(error.empty())error="Rational PCM transport requires an owned source and exact rate";return false;}
 uint64_t boundary=uint64_t(rate.denominator)*outputRate;
 // Bound the signed interpolation product without nonportable wide integers.
 if(boundary>uint64_t(std::numeric_limits<int64_t>::max()/131070)){error="Rational PCM interpolation denominator exceeds bounded arithmetic";return false;}
 _source=source;_outputRate=outputRate;_step=rate.numerator;_position=_boundary=boundary;return true;
}
bool RationalPcmTransport::restore(const std::shared_ptr<NativeAudioFeed>& source,const RationalAudioRate& input,uint32_t outputRate,NativeArchive& a,std::string& error){
 // A source may have delivered its last sample while the transport still owns
 // interpolation history, or while its controller has not polled completion.
 // Restore that owner; do not restart playback or discard its final samples.
 RationalPcmTransport next;
 if(_source||!a.reading||!next.bind(source,input,outputRate,true,error))return false;
 const uint32_t step=next._step;const uint64_t boundary=next._boundary;
 if(!next.archive(a,error))return false;
 if(next._outputRate!=outputRate||next._step!=step||next._boundary!=boundary){error="Rational PCM continuation rate differs from its source";return false;}
 *this=next;return true;
}
size_t RationalPcmTransport::readFrames(int16_t* stereo,size_t frames){
 if(!_source||!stereo||!frames||!_source->error().empty())return 0;size_t made=0;
 while(made<frames){
  while(_position>=_boundary){int16_t next[2];if(_source->readFrames(next,1)!=1)return made;_last[0]=_current[0];_last[1]=_current[1];_current[0]=next[0];_current[1]=next[1];_position-=_boundary;++_consumed;}
  for(unsigned c=0;c<2;++c){int64_t value=int64_t(int(_current[c])-int(_last[c]))*int64_t(_position)+int64_t(_boundary/2);
   // Explicit floor division matches the existing arithmetic-shift rule for
   // negative deltas without depending on signed right-shift semantics.
   int64_t delta=value>=0?value/int64_t(_boundary):-((-value+int64_t(_boundary)-1)/int64_t(_boundary));
   stereo[2*made+c]=int16_t(int64_t(_last[c])+delta);
  }
  ++made;++_rendered;_position+=_step;
 }return made;
}
bool RationalPcmTransport::ended()const{return _source&&_position>=_boundary&&_source->ended()&&_source->error().empty();}
std::string RationalPcmTransport::error()const{return _source?_source->error():std::string("Rational PCM transport has no source");}
}

namespace Snatcher {
bool RationalPcmTransport::archive(NativeArchive& a,std::string& error){
 a.field(_outputRate);a.field(_step);a.field(_position);a.field(_boundary);a.field(_rendered);a.field(_consumed);for(unsigned c=0;c<2;++c){a.field(_last[c]);a.field(_current[c]);}
 if(!_source||_outputRate<1000||_outputRate>384000||!_step||!_boundary||_position>_boundary+_step)a.ok=false;
 if(!a.ok)error="Invalid rational PCM continuation";return a.ok;
}
}
