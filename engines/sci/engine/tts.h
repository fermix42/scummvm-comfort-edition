/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef SCI_ENGINE_TTS_H
#define SCI_ENGINE_TTS_H

#include "common/text-to-speech.h"
#include "common/hashmap.h"
#include "common/file.h"
#include "audio/mixer.h"
#include "sci/sci.h"

namespace Sci {

class SciTTS {
public:
	SciTTS();
	~SciTTS();
	const byte *editionAsset(const Common::String &name, uint32 size);
	void bindVoice(reg_t buffer, const Common::String &id, const Common::String &text);
	void recordedText(const Common::String &text, reg_t buffer = NULL_REG);
	void recordedCel(int view, int loop, int cel);
	void recordedPicture(int picture);
	void resetVoices();
	void speakSelection(int module, int noun, int verb, int condition, int sequence, int argc, reg_t *argv, const char *talkerName = "rogTalker");
	bool selectionAt(reg_t pc, reg_t object, int script);
	void updateRecordedTiming();
	int recordedMouthCel(reg_t object, int celCount);
	int endingFloMouthCel();
	int recordedDrawMouthCel(int view, int loop, int celCount);
	bool recordedMouthCycle(reg_t object);
	bool skipLogSection();
	void beginDialogueHandoff();
	bool deferDialogueFrame();
	byte logCharacter(reg_t buffer, uint16 offset, byte value);

	void button(const Common::String &text);
	void text(const Common::String &text);
	void stop();
	void setMessage(const Common::String &text);

private:
	Common::TextToSpeechManager *_ttsMan;
	Common::String _curMessage;
	struct VoiceBinding { Common::String id, text; };
	Common::HashMap<uint32, VoiceBinding> _voiceBindings;
	Common::HashMap<Common::String, bool> _captured;
	Audio::SoundHandle _voiceHandle;
	Common::String _lastVoice;
	Common::String _letterNextText;
	Common::DumpFile *_voiceCapture;
	bool _recordedVoices;
	Common::HashMap<Common::String, Common::Array<byte> > _editionAssets;
	bool _speakingSelection = false;
	bool _floGreetingPlayed = false;
	VoiceBinding _latestBinding;
	reg_t _selectionHandledPC;
	reg_t _choiceObject;
	int _choiceModule = 0, _choiceNoun = 0, _choiceVerb = 0, _choiceCondition = 0, _choiceSequence = 0;
	reg_t _voiceNarrator;
	reg_t _voicePortraitOwner;
	reg_t _heldMouth;
	int _heldMouthRoom = -1, _heldMouthView = -1, _heldMouthLoop = -1;
	Common::Array<float> _voiceEnvelope;
	float _voiceEnvelopeReference = 1.0f;
	reg_t _labAutoCloseTalker;
	uint16 _labKeepWindow = 0;
	reg_t _logScript;
	Common::String _logText;
	reg_t _logBuffer;
	struct LogWord { uint32 end, ms; };
	Common::Array<LogWord> _logWords;
	void loadLogWords(const Common::String &id);
	void updateLogTiming();
	uint32 _dialogueHandoffStart = 0;
	int _dialogueHandoffRoom = -1;
	bool _dialogueHandoffPending = false;
	uint32 _voiceDurationMs = 0;
	bool _voiceTimingPending = false;

	void playRecorded(const Common::String &id, const Common::String &text);
	Common::String getMessage(const Common::String &text);
	bool shouldPerformTTS(const Common::String &message) const;
};

} // End of namespace Sci

#endif // SCI_ENGINE_TTS_H
