/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_AUDIO_FM_H
#define SNATCHER_AUDIO_FM_H
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
namespace Snatcher {
// Original game instrument data normalized into synthesis parameters. No
// device register writes, synthesis, oscillator timing or sound-chip state.
struct FmOperator {
 uint8_t detune,multiple,totalLevel,rateScale,attackRate,amplitudeModulation,decayRate,sustainRate,sustainLevel,releaseRate;
};
struct FmInstrument {
 uint8_t algorithm,feedback,source[25];FmOperator operators[4]; // retained source operator order
};
bool parseFmInstruments(const uint8_t *data,size_t size,std::vector<FmInstrument>& instruments,std::string& error);
struct FmCommandAsset {
 uint8_t command,firstChannel,channelCount,priority;bool music;
 uint32_t sequenceIndex;size_t sourceOffset;std::vector<uint8_t> bytes;std::vector<uint32_t> channels;
 FmCommandAsset():command(0),firstChannel(0),channelCount(0),priority(0),music(false),sequenceIndex(0),sourceOffset(0){}
};
// Reads source-owned format tables from main-IP/FMWR resources. Main-IP starts
// after the512-byte disc boot header. A command is not a CD audio track number.
bool readFmCommandAsset(const uint8_t *mainIp,size_t mainSize,const uint8_t *fm,size_t fmSize,uint16_t command,FmCommandAsset& asset,std::string& error);
enum FmSequenceEventKind {kFmNote,kFmRest,kFmInstrument,kFmTempo,kFmVolume,kFmTranspose,kFmJump,kFmCall,kFmReturn,kFmLoop,kFmIgnoredByte,kFmSequenceEnd,kFmHold,kFmPan,kFmVibrato,kFmRandomPitch,kFmPanCycle,kFmGlobalLfo,kFmVolumeRamp,kFmPortamento,kFmPitchOffset,kFmAmplitudeModulation,kFmPitchSensitivity,kFmVibratoRamp,kFmRelease,kFmSlideIncrement,kFmPercussion};
struct FmSequenceEvent {
 FmSequenceEventKind kind;size_t offset;uint8_t opcode,note,duration,gate,level,value;
 uint8_t operands[4],operandCount;bool hasSlide;uint8_t slideDelay,slideTicks,slideNote;int16_t slideIncrement;
 bool reusedDuration,reusedGate;
 FmSequenceEvent():kind(kFmIgnoredByte),offset(0),opcode(0),note(0),duration(0),gate(0),level(0),value(0),operandCount(0),hasSlide(false),slideDelay(0),slideTicks(0),slideNote(0),slideIncrement(0),reusedDuration(false),reusedGate(false){for(unsigned i=0;i<4;++i)operands[i]=0;}
};
// Strict semantic event reader. There is intentionally no sound output or
// playback completion. Controls are typed musical events; truncated payloads
// stop transactionally instead of being guessed to be note data.
class FmSequenceReader {
public:
 FmSequenceReader();
 bool start(const std::vector<uint8_t>& bytes,size_t channelOffset,std::string& error,bool percussion=false);
 bool next(FmSequenceEvent& event,std::string& error);
 size_t cursor()const{return _cursor;}bool ended()const{return _ended;}
 uint8_t peek()const{return _bytes&&_cursor<_bytes->size()?(*_bytes)[_cursor]:0xff;}
 int16_t loopPitch()const{return int16_t(uint16_t(_loopPitch[0]+_loopPitch[1]));}
 int16_t loopVolume()const{return int16_t(uint16_t(_loopVolume[0]+_loopVolume[1]));}
private:
 friend class FmPlayer;
 // Sequence bytes are immutable; transactional cursor copies share their owner.
 std::shared_ptr<const std::vector<uint8_t>> _bytes;size_t _cursor,_loop[2],_return,_phrase,_phraseReturn;uint8_t _duration,_gate,_level,_repeat[2],_phraseState;
 int16_t _loopPitch[2],_loopVolume[2];bool _percussion;
 bool _ended,_haveLoop[2],_haveReturn,_havePhrase;
};
}
#endif
