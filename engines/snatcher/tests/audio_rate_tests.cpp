/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../audio_rate.h"
#include <cstdio>
#include <limits>
namespace {unsigned checks,failures;void check(bool value,const char* message){++checks;if(!value){++failures;std::fprintf(stderr,"FAIL: %s\n",message);}}}
int runAudioRateTests(){using namespace Snatcher;std::string error;uint64_t count=0;uint32_t mixer=0;RationalAudioRate stream(12500000,768),normalized=stream;
 check(normalizeAudioRate(normalized,error)&&normalized.numerator==390625&&normalized.denominator==24,"rational normalization");
 check(nearestMixerRate(stream,mixer,error)&&mixer==16276,"nearest CE mixer rate");
 check(audioDurationNanoseconds(68281,stream,count,error)&&count==4195184640ULL,"source stream duration exact");
 check(resampledFrameCount(68281,stream,44100,count,error)&&count==185008,"44.1k output retains final sample");
 check(resampledFrameCount(68281,stream,48000,count,error)&&count==201369,"48k output retains final sample");
 check(resampledFrameCount(1,stream,44100,count,error)&&count==3,"one source frame rounds up");
 check(resampledFrameCount(0,stream,44100,count,error)&&count==0,"empty source frame extent");
 check(audioDurationNanoseconds(0,stream,count,error)&&count==0,"empty duration");
 const uint64_t maximum=std::numeric_limits<uint64_t>::max();
 check(resampledFrameCount(maximum,RationalAudioRate(1,1),1,count,error)&&count==maximum,"full 64bit identity");
 check(resampledFrameCount(maximum,RationalAudioRate(7,1),2,count,error)&&count==5270498306774157605ULL,"large quotient without intermediate multiplication overflow");
 check(!resampledFrameCount(maximum,RationalAudioRate(1,1),2,count,error)&&count==0,"actual frame overflow rejects and clears");
 check(!audioDurationNanoseconds(maximum,RationalAudioRate(1,1),count,error)&&count==0,"duration overflow rejects and clears");
 for(const RationalAudioRate& invalid:{RationalAudioRate(0,1),RationalAudioRate(1,0),RationalAudioRate(1,2),RationalAudioRate(384001,1)}){
  RationalAudioRate copy=invalid;check(!normalizeAudioRate(copy,error)&&copy.numerator==invalid.numerator&&copy.denominator==invalid.denominator,"invalid normalization transactional");
  check(!nearestMixerRate(invalid,mixer,error)&&mixer==0,"invalid mixer rate clears");check(!audioDurationNanoseconds(1,invalid,count,error)&&count==0,"invalid duration clears");
 }
 check(!resampledFrameCount(1,stream,0,count,error)&&count==0,"zero target rejects");check(!resampledFrameCount(1,stream,384001,count,error)&&count==0,"large target rejects");
 // Exhaustive small-integer oracle uses a product that independently fits64bits.
 for(uint32_t n=1;n<=16;++n)for(uint32_t d=1;d<=n;++d)for(uint32_t target=1;target<=16;++target)for(uint64_t input=0;input<=32;++input){uint64_t expected=(input*target*d+n-1)/n;check(resampledFrameCount(input,RationalAudioRate(n,d),target,count,error)&&count==expected,"small exact rational oracle");}
 std::printf("audio rate contracts: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
#ifdef SNATCHER_AUDIO_RATE_TEST_MAIN
int main(){return runAudioRateTests();}
#endif
