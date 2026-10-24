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

#include "sci/sci.h"
#include "sci/engine/state.h"
#include "sci/engine/tts.h"
#include "common/system.h"
#include "common/debug.h"
#include "common/text-to-speech.h"
#include "common/config-manager.h"
#include "audio/decoders/wave.h"
#include "audio/audiostream.h"
#include "common/path.h"
#include "common/fs.h"
#include "common/md5.h"
#include "common/algorithm.h"
#include "sci/engine/selector.h"
#include "sci/engine/kernel.h"
#include "sci/engine/seg_manager.h"
#include "sci/engine/script.h"
#include "sci/engine/message.h"
#include "sci/event.h"

namespace Sci {

SciTTS::SciTTS() : _curMessage(""), _voiceCapture(nullptr), _recordedVoices(false) {
	_recordedVoices = g_sci->isSQ5TrashTalkin() && ConfMan.hasKey("sq5_voice_path");
	if (_recordedVoices && ConfMan.hasKey("sq5_capture_file")) {
		_voiceCapture = new Common::DumpFile();
		if (!_voiceCapture->open(ConfMan.getPath("sq5_capture_file"))) {
			delete _voiceCapture;
			_voiceCapture = nullptr;
		}
	}
	_ttsMan = ConfMan.getBool("tts_enabled") ? g_system->getTextToSpeechManager() : nullptr;
	if (_ttsMan != nullptr) {
		_ttsMan->setLanguage(ConfMan.get("language"));
		_ttsMan->enable(true);
	}
}


// SQ5 Voice Studio: bind actual message buffers to stable SCI tuples. Lookups
// that only query message size/talker never trigger or reassign playback.
SciTTS::~SciTTS() {
	stop();
	delete _voiceCapture;
}

const byte *SciTTS::editionAsset(const Common::String &name, uint32 size) {
	if (!_recordedVoices) return nullptr;
	if (!_editionAssets.contains(name)) {
		Common::Array<byte> data;
		Common::File file;
		if (file.open(Common::FSNode(ConfMan.getPath("sq5_voice_path").join(name + ".bin"))) && file.size() == size) {
			data.resize(size);
			if (file.read(data.data(), size) != size) data.clear();
		}
		_editionAssets[name] = data;
	}
	const Common::Array<byte> &data = _editionAssets[name];
	return data.size() == size ? data.data() : nullptr;
}

void SciTTS::bindVoice(reg_t buffer, const Common::String &id, const Common::String &text) {
	if (!_recordedVoices) return;
	uint32 key = (uint32(buffer.getSegment()) << 16) | buffer.getOffset();
	VoiceBinding b; b.id = id; b.text = text;
	_voiceBindings[key] = b;
	_latestBinding = b;
	// Start the six log recordings at message retrieval, before the word reveal.
	if (id.hasPrefix("104-1-0-0-") && id.size() == 11 && id.lastChar() >= '1' && id.lastChar() <= '6' &&
		g_sci->getEngineState()->currentRoomNumber() == 104) {
		for (const auto &frame : g_sci->getEngineState()->_executionStack) {
			if (Common::String(g_sci->getEngineState()->_segMan->getObjectName(frame.objp)) == "sCaptainsLog") {
				playRecorded(id, text);
				if (_lastVoice == id && (!ConfMan.hasKey("sq5_voice_timing") || ConfMan.getBool("sq5_voice_timing"))) {
					_logScript = frame.objp;
					_logText = text;
					_logBuffer = buffer;
					loadLogWords(id);
				}
				break;
			}
		}
	}
}

void SciTTS::resetVoices() {
	stop();
	_heldMouth = NULL_REG;
	_dialogueHandoffPending = false;
	_floGreetingPlayed = false;
	_voiceBindings.clear();
	_latestBinding = VoiceBinding();
	_selectionHandledPC = _choiceObject = NULL_REG;
}

// Coalesce a spoken window's removal and its immediate successor into one
// presented frame. SCI still disposes/restores normally; only presentation is
// held briefly, so saved backgrounds, input and script timing remain intact.
void SciTTS::beginDialogueHandoff() {
	if (!_recordedVoices || _lastVoice.empty() || _dialogueHandoffPending) return;
	_dialogueHandoffPending = true;
	_dialogueHandoffStart = g_system->getMillis();
	_dialogueHandoffRoom = g_sci->getEngineState()->currentRoomNumber();
}

bool SciTTS::deferDialogueFrame() {
	if (!_dialogueHandoffPending) return false;
	if (g_sci->getEngineState()->currentRoomNumber() != _dialogueHandoffRoom ||
		g_system->getMillis() - _dialogueHandoffStart >= 150) {
		_dialogueHandoffPending = false;
		return false;
	}
	return true;
}

void SciTTS::recordedText(const Common::String &text, reg_t buffer) {
	if (!_recordedVoices || text.empty()) return;
	_dialogueHandoffPending = false; // The replacement is drawn before the next presentation.
	if (text.equalsIgnoreCase("tool box") || text.equalsIgnoreCase("toolbox")) return;
	Common::String id;
	uint32 key = (uint32(buffer.getSegment()) << 16) | buffer.getOffset();
	if (!buffer.isNull() && _voiceBindings.contains(key) &&
		(_voiceBindings[key].text == text || (text.size() >= 12 && _voiceBindings[key].text.hasPrefix(text))))
		id = _voiceBindings[key].id;
	// Some scripts copy message buffers. Only use a text fallback when all
	// matching bindings agree; never arbitrarily pick another speaker's line.
	if (id.empty()) {
		bool ambiguous = false;
		for (auto i = _voiceBindings.begin(); i != _voiceBindings.end(); ++i) {
			if (i->_value.text == text || (text.size() >= 12 && i->_value.text.hasPrefix(text))) {
				if (!id.empty() && id != i->_value.id) { ambiguous = true; break; }
				id = i->_value.id;
			}
		}
		if (ambiguous) {
			id.clear();
			// The freshly retrieved message disambiguates repeated transmission
			// text in different resources without choosing a random talker.
			if (_latestBinding.text == text) id = _latestBinding.id;
		}
	}
	if (id.empty()) {
		uint32 hash = 2166136261u;
		for (uint i = 0; i < text.size(); ++i) hash = (hash ^ byte(text[i])) * 16777619u;
		id = Common::String::format("x-%u-%08x", g_sci->getEngineState()->currentRoomNumber(), hash);
	}
	playRecorded(id, text);
}

void SciTTS::recordedPicture(int picture) {
	// English SQ5 picture 127 contains WD40's graphic-only battle cry.
	// Picture draws are discrete scene events, unlike per-frame cel redraws.
	if (_recordedVoices && picture == 127)
		playRecorded("127-200-0-0-1", "Freeze, scum!");
}

void SciTTS::recordedCel(int view, int loop, int cel) {
	if (!_recordedVoices) return;
	const int room = g_sci->getEngineState()->currentRoomNumber();
	if (room != 760) _floGreetingPlayed = false;
	// Genetix communicator greeting is the hello object's painted text cel.
	if (room == 760 && view == 606 && loop == 3 && cel == 0) {
		if (!_floGreetingPlayed) {
			playRecorded("760-18-0-0-1", "Hello?");
			_floGreetingPlayed = true;
		}
		return;
	}
	if (room != 104) return;
	// English SQ5 1.04 intro speech is painted into these four view cels.
	// Match the artwork, not message lookups (which this cutscene never makes).
	int sequence = 0;
	const char *text = nullptr;
	if (view == 100 && loop == 3 && cel == 1) { sequence = 1; text = "Admiral! Strike ships coming in at point three-five!"; }
	if (view == 101 && loop == 2 && cel == 0) { sequence = 2; text = "Shields up! Battle stations! Lock weapons!"; }
	if (view == 101 && loop == 3 && cel == 0) { sequence = 3; text = "Neutron beams locked. Proton torpedoes armed."; }
	if (view == 97 && loop == 3 && cel == 0) { sequence = 4; text = "Tactical, fire neutron beams! Helm, hard to port!"; }
	if (sequence) playRecorded(Common::String::format("104-2-0-0-%d", sequence), text);
}

void SciTTS::playRecorded(const Common::String &id, const Common::String &text) {
	// Science logs are one recording per complete page, never fragment reads.
	if (g_sci->getEngineState()->currentRoomNumber() == 750 &&
		!id.hasPrefix("752-200-")) {
		static const char *const dates[] = {"3234.22", "3237.12", "3243.19", "3252.06", "3263.11"};
		for (uint page = 0; page < ARRAYSIZE(dates); ++page) {
			if (text.hasPrefix("Science Log, Stardate:") && text.contains(dates[page])) {
				playRecorded(Common::String::format("752-200-0-%u-1", page + 6), text);
				return;
			}
		}
	}
	// Only Genetix terminal output is silent. Verb-zero messages are the
	// computer's pages/notices; verb-one LOOK responses remain narrated,
	// including global responses such as looking at Roger in fly form.
	int module = -1, noun = -1, verb = -1, condition = -1, sequence = -1;
	const bool messageId = sscanf(id.c_str(), "%d-%d-%d-%d-%d",
		&module, &noun, &verb, &condition, &sequence) == 5;
	if ((messageId && module >= 750 && module <= 752 && verb == 0 && !(module == 752 && noun == 200 && condition >= 6 && condition <= 10 && sequence == 1)) ||
		id.hasPrefix("x-750-") || id.hasPrefix("x-751-") || id.hasPrefix("x-752-")) return;
	// SQ5 resource 205 contains command-menu labels, not spoken dialogue.
	// Never voice these on hover, even if a voice pack supplies recordings.
	if (id.hasPrefix("205-") && !_speakingSelection) return;
	// Toolbox verb-4 resources label controls drawn as the panel opens.
	// Keep them silent; verb-1 descriptions and actual pickup responses speak.
	if (id == "226-3-4-0-1" || id == "226-7-4-0-1" || id == "226-17-4-0-1") return;
	// Script 30's ChoiceNarrator/ChoiceTalker display selectable answers when
	// normal is false. The same classes delegate ordinary spoken dialogue
	// when normal is true, so do not suppress the whole conversation script.
	EngineState *choiceState = g_sci->getEngineState();
	const int displaySelector = g_sci->getKernel()->findSelector("display");
	const int normalSelector = g_sci->getKernel()->findSelector("normal");
	// These lines are spoken crew menu prompts, not selectable answers.
	// ChoiceTalker displays the prompt with normal=false too. Keep the prompt
	// audible while still suppressing every selectable Roger answer on hover.
	static const char *const spokenMenuGreetings[] = {
		"240-15-0-20-1", // Roger asks Spike; answers below remain selectable.
		"246-1-2-4-1", // Cliffy opens the engineering map conversation.
		"202-4-0-5-1",
		"202-26-0-31-1",
		"202-26-0-36-1",
		"202-26-0-55-1",
		"202-26-0-63-1",
		"202-26-0-70-1",
		"202-26-0-74-1",
		"202-12-0-8-1",
		"202-12-0-27-1",
		"202-12-0-32-1",
		"202-12-0-41-1",
		"202-12-0-42-1",
		"202-12-0-51-1",
		"202-12-0-61-1",
		"202-12-0-64-1",
		"202-12-0-79-1",
		"202-12-0-83-1",
		"620-8-0-4-1",
		"620-10-0-4-1",
		"640-5-0-4-1",
		"640-6-0-4-1",
		"1040-5-0-1-1", // Flo opens the Goliath evacuation choices.
	};
	bool spokenMenuGreeting = false;
	for (uint i = 0; i < ARRAYSIZE(spokenMenuGreetings); ++i)
		if (id == spokenMenuGreetings[i]) { spokenMenuGreeting = true; break; }

	for (const auto &frame : choiceState->_executionStack) {
		Script *script = choiceState->_segMan->getScriptIfLoaded(frame.local_segment);
		if (!_speakingSelection && !spokenMenuGreeting && script && script->getScriptNumber() == 30 && frame.debugSelector == displaySelector &&
			normalSelector >= 0 &&
			lookupSelector(choiceState->_segMan, frame.objp, normalSelector, nullptr, nullptr) == kSelectorVariable &&
			!readSelectorValue(choiceState->_segMan, frame.objp, normalSelector))
			return;
	}

	if (_voiceCapture && !_captured.contains(id)) {
		// Text is hex-encoded to preserve the game's encoding and line breaks.
		Common::String line = id + "\t";
		for (uint i = 0; i < text.size(); ++i) line += Common::String::format("%02x", byte(text[i]));
		line += "\n";
		_voiceCapture->write(line.c_str(), line.size());
		_voiceCapture->flush();
		_captured[id] = true;
	}
	// Script 166 draws both letter paragraphs in the same state, not as
	// separate dialogue boxes. Queue paragraph two instead of cutting off one.
	if (id == "166-1-0-0-2" && _lastVoice == "166-1-0-0-1" &&
		g_sci->getEngineState()->currentRoomNumber() == 166 &&
		g_system->getMixer()->isSoundHandleActive(_voiceHandle)) {
		_letterNextText = text;
		return;
	}
	if (id == _lastVoice) return; // redraws of the same open text control
	Common::File *file = new Common::File();
	Common::Path path = ConfMan.getPath("sq5_voice_path").join(id + ".wav");
	Common::FSNode node(path);
	if (!node.exists() || !file->open(node)) { delete file; return; }
	Audio::SeekableAudioStream *stream = Audio::makeWAVStream(file, DisposeAfterUse::YES);
	if (!stream) return; // makeWAVStream owns file, including failure paths
	_voiceNarrator = NULL_REG;
	_voiceTimingPending = false;
	_voiceDurationMs = stream->getLength().msecs();
	// Analyze the exact audio that will play; regenerated and edited takes
	// require no separate lip-sync asset. Use the prototype's 25 ms RMS bins.
	_voiceEnvelope.clear();
	const int channels = stream->isStereo() ? 2 : 1;
	const int frames = (stream->getRate() + 20) / 40;
	Common::Array<int16> samples;
	samples.resize(frames * channels);
	int count;
	while ((count = stream->readBuffer(samples.data(), samples.size())) > 0) {
		double sum = 0;
		const int n = count / channels;
		for (int i = 0; i < n; ++i) {
			double mono = samples[i * channels] / 32768.0;
			if (channels == 2) mono = (mono + samples[i * channels + 1] / 32768.0) / 2;
			sum += mono * mono;
		}
		_voiceEnvelope.push_back(n ? float(sqrt(sum / n)) : 0);
	}
	Common::Array<float> sorted = _voiceEnvelope;
	Common::sort(sorted.begin(), sorted.end());
	_voiceEnvelopeReference = sorted.empty() ? 0.1f : sorted[sorted.size() * 9 / 10];
	if (_voiceEnvelopeReference <= 0) _voiceEnvelopeReference = 0.1f;
	if (!stream->rewind()) { delete stream; _voiceEnvelope.clear(); return; }

	// Only the standard script-928 Narrator:say lifecycle owns this timer.
	// Do not synthesize input, alter game time, or time choice/custom talkers.
	EngineState *state = g_sci->getEngineState();
	int saySelector = g_sci->getKernel()->findSelector("say");
	for (const auto &frame : state->_executionStack) {
		Script *script = state->_segMan->getScriptIfLoaded(frame.local_segment);
		if (!script) continue;
		if (script->getScriptNumber() == 928 && frame.debugSelector == saySelector)
			_voiceNarrator = frame.objp;
	}
	_heldMouth = NULL_REG; // A new recording establishes a new mouth owner.
	_voicePortraitOwner = _voiceNarrator;
	// Klorox has custom display methods. Bind the concrete visible talker,
	// rather than relying on the generic Narrator:say frame being present.
	if (state->currentRoomNumber() == 440 && id.hasPrefix("440-")) {
		const int mouthSelector = g_sci->getKernel()->findSelector("mouth");
		for (const auto &frame : state->_executionStack) {
			if (!state->_segMan->getObject(frame.objp)) continue;
			const Common::String name(state->_segMan->getObjectName(frame.objp));
			if ((name == "pukoid" || name == "roger") && mouthSelector >= 0 &&
				lookupSelector(state->_segMan, frame.objp, mouthSelector, nullptr, nullptr) == kSelectorVariable &&
				!readSelector(state->_segMan, frame.objp, mouthSelector).isNull()) {
				_voicePortraitOwner = frame.objp;
				_voiceNarrator = frame.objp;
			}
		}
	}

	// Selectable script-30 displays are rejected before playback above.
	// Ordinary spoken lines may still have a conversation caller on the stack.
	if (ConfMan.hasKey("sq5_voice_timing") && !ConfMan.getBool("sq5_voice_timing")) _voiceNarrator = NULL_REG;
	_voiceTimingPending = !_voiceNarrator.isNull();
	g_system->getMixer()->stopHandle(_voiceHandle);
	g_system->getMixer()->playStream(Audio::Mixer::kSpeechSoundType, &_voiceHandle, stream);
	if (_voiceCapture) {
		Common::String event = "#play\t" + id + "\n";
		_voiceCapture->write(event.c_str(), event.size());
		_voiceCapture->flush();
	}
	_lastVoice = id;
}

// These are post-selection instruction boundaries in the supported English
// SQ5 scripts. Keeping the resources at their original sizes preserves the
// script/heap addresses stored in existing save games.
bool SciTTS::selectionAt(reg_t pc, reg_t object, int script) {
	const int room = g_sci->getEngineState()->currentRoomNumber();
	if (!_recordedVoices || _speakingSelection || (room != 201 && room != 240 && room != 620 && room != 640 && room != 1040)) return false;
	if (pc == _selectionHandledPC) return false;
	_selectionHandledPC = NULL_REG;
	EngineState *s = g_sci->getEngineState();
	const int offset = pc.getOffset();
	auto value = [&](const char *name) { return readSelectorValue(s->_segMan, object, g_sci->getKernel()->findSelector(name)); };
	Script *resource = s->_segMan->getScriptIfLoaded(pc.getSegment());
	if (!resource || uint32(offset) + 2 > resource->getScriptSize()) return false;
	const byte *code = resource->getBuf(offset);
	if (script == 30) {
		if ((offset == 0x444 && code[0] == 0x65 && code[1] == 0x84) ||
			(offset == 0x988 && code[0] == 0x65 && code[1] == 0x98)) {
			_choiceObject = object;
			_choiceModule = value("modNum"); _choiceNoun = value("curNoun");
			_choiceVerb = value("curVerb"); _choiceCondition = value("curCase");
			_choiceSequence = s->r_acc.toSint16() + 1;
		} else if ((offset == 0x44c || offset == 0x990) && code[0] == 0x33 && object == _choiceObject) {
			_selectionHandledPC = pc;
			_choiceObject = NULL_REG;
			if ((room == 240 && _choiceModule == 240 && _choiceNoun == 15 && _choiceCondition == 20 && _choiceSequence >= 2 && _choiceSequence <= 5) || (room == 240 && _choiceModule == 246 && _choiceNoun == 1 && _choiceVerb == 2 && _choiceCondition == 4 && _choiceSequence >= 2 && _choiceSequence <= 4) || (room == 1040 && _choiceModule == 1040 && _choiceNoun == 5 && _choiceVerb == 0 && _choiceCondition == 1 && _choiceSequence >= 2 && _choiceSequence <= 4) || room == 201 || ((_choiceModule == 620 || _choiceModule == 640) && _choiceCondition == 4 && _choiceSequence >= 2))
				speakSelection(_choiceModule, _choiceNoun, _choiceVerb, _choiceCondition, _choiceSequence, 0, s->xs->sp, (room == 240 || room == 1040) ? "tkrRoger" : (room == 201 ? "rogTalker" : "myRogTalker"));
			return true;
		}
		return false;
	}
	if (room != 201) return false;
	int first = 0;
	if (script == 216 && offset == 0x8c7 && code[0] == 0x36) first = 12;
	if (script == 217 && offset == 0xe31 && code[0] == 0x65 && code[1] == 0x24) first = 1;
	if (script == 218 && offset == 0x131 && code[0] == 0x65 && code[1] == 0x24) first = 19;
	if (script == 219 && offset == 0x139 && code[0] == 0x65 && code[1] == 0x24) first = 24;
	if (!first) return false;
	_selectionHandledPC = pc;
	const int index = s->r_acc.toSint16();
	if (index >= 0) speakSelection(205, 1, 0, 0, first + index, 0, s->xs->sp);
	return true;
}

void SciTTS::speakSelection(int module, int noun, int verb, int condition, int sequence, int argc, reg_t *argv, const char *talkerName) {
	EngineState *s = g_sci->getEngineState();
	const int room = s->currentRoomNumber();
	if (!_recordedVoices || _speakingSelection || (room != 201 && room != 240 && room != 620 && room != 640 && room != 1040) || sequence < 1) return;
	reg_t talker = NULL_REG;
	for (const reg_t candidate : s->_segMan->findObjectsByName(talkerName))
		if (candidate.getSegment() == s->_segMan->getScriptSegment(room)) { talker = candidate; break; }
	if (talker.isNull()) return;
	MessageState message(s->_segMan);
	MessageTuple tuple(noun, verb, condition, sequence);
	int size = message.messageSize(module, tuple);
	if (size <= 1 || size > 8192) return;
	reg_t buffer;
	s->_segMan->allocDynmem(size + 1, "SQ5 selected dialogue", &buffer);
	message.getMessage(module, tuple, buffer);
	Common::String text = s->_segMan->getString(buffer);
	// Spoken orders may expand terse menu labels without altering resources.
	if (module == 205) {
		Common::File spoken;
		const Common::String id = Common::String::format("%d-%d-%d-%d-%d", module, noun, verb, condition, sequence);
		if (spoken.open(Common::FSNode(ConfMan.getPath("sq5_voice_path").join(id + ".spoken.txt"))) && spoken.size() > 0 && spoken.size() < 2048) {
			char expanded[2048];
			const uint32 length = spoken.read(expanded, spoken.size());
			expanded[length] = 0;
			s->_segMan->freeDynmem(buffer);
			s->_segMan->allocDynmem(length + 1, "SQ5 spoken order", &buffer);
			s->_segMan->strcpy_(buffer, expanded);
			text = expanded;
			bindVoice(buffer, id, text);
		}
	}
	const reg_t acc = s->r_acc;
	const int rest = s->r_rest;
	_speakingSelection = true;
	// Use the current room's real Roger talker, including its normal position and
	// colors. Narrator:say opens asynchronously; hold this menu return until
	// the recording completes, then dispose the bubble before resuming SCI.
	// The lab's Roger is also a menu talker. Render the chosen answer as
	// ordinary dialogue, then restore his menu mode after disposal.
	const int normalSelector = g_sci->getKernel()->findSelector("normal");
	const bool labChoice = (room == 240 || room == 1040) && normalSelector >= 0 &&
		lookupSelector(s->_segMan, talker, normalSelector, nullptr, nullptr) == kSelectorVariable;
	const uint16 previousNormal = labChoice ? readSelectorValue(s->_segMan, talker, normalSelector) : 0;
	if (labChoice) writeSelectorValue(s->_segMan, talker, normalSelector, 1);
	const reg_t params[] = {buffer, NULL_REG};
	invokeSelector(s, talker, g_sci->getKernel()->findSelector("say"), argc, argv, 2, params);
	const bool hasAudio = g_system->getMixer()->isSoundHandleActive(_voiceHandle);
	const uint32 start = g_system->getMillis();
	const uint32 fallbackDuration = MAX<uint32>(2000, text.size() * 55);
	while (!g_sci->shouldQuit()) {
		SciEvent event = g_sci->getEventManager()->getSciEvent(kSciEventAny);
		if (event.type == kSciEventQuit || event.type == kSciEventMousePress || event.type == kSciEventKeyDown) break;
		if (hasAudio ? !g_system->getMixer()->isSoundHandleActive(_voiceHandle) : g_system->getMillis() - start >= fallbackDuration) break;
		// This synchronous selection wait pauses the room's usual doit loop.
		// Tick the actual talker so its original eyes and waveform-driven mouth
		// continue animating for every selected reply, including bridge orders.
		if (s->_segMan->getObject(talker))
			invokeSelector(s, talker, g_sci->getKernel()->findSelector("doit"), argc, argv, 0, nullptr);
		g_system->updateScreen();
		g_system->delayMillis(10);
	}
	const reg_t disposeArgs[] = {make_reg(0, 1)};
	invokeSelector(s, talker, g_sci->getKernel()->findSelector("dispose"), argc, argv, 1, disposeArgs);
	stop();
	if (labChoice && s->_segMan->getObject(talker))
		writeSelectorValue(s->_segMan, talker, normalSelector, previousNormal);
	_speakingSelection = false;
	s->_segMan->freeDynmem(buffer);
	s->r_acc = acc;
	s->r_rest = rest;
}

void SciTTS::loadLogWords(const Common::String &id) {
	_logWords.clear();
	Common::File file, audio;
	const Common::Path root = ConfMan.getPath("sq5_voice_path");
	if (!file.open(Common::FSNode(root.join(id + ".tim"))) || file.size() < 52) return;
	char magic[8], digest[33];
	file.read(magic, 8);
	if (memcmp(magic, "SQ5WORD1", 8)) return;
	uint32 hash = 2166136261u;
	for (uint i = 0; i < _logText.size(); ++i) hash = (hash ^ byte(_logText[i])) * 16777619u;
	if (file.readUint32LE() != hash) return;
	const uint32 duration = file.readUint32LE(), count = file.readUint32LE();
	if (ABS(int64(duration) - _voiceDurationMs) > 2 || !count || count > _logText.size() || file.size() != 52 + count * 8) return;
	file.read(digest, 32); digest[32] = 0;
	if (!audio.open(Common::FSNode(root.join(id + ".wav"))) || Common::computeStreamMD5AsString(audio) != digest) return;
	for (uint i = 0; i < count; ++i) {
		LogWord word; word.end = file.readUint32LE(); word.ms = file.readUint32LE();
		if (!word.end || word.end > _logText.size() || word.ms > duration + 250 ||
			(i && (word.end <= _logWords.back().end || word.ms < _logWords.back().ms))) { _logWords.clear(); return; }
		_logWords.push_back(word);
	}
	if (_voiceCapture) {
		Common::String event = Common::String::format("#word-sync\t%s\t%u\n", id.c_str(), count);
		_voiceCapture->write(event.c_str(), event.size()); _voiceCapture->flush();
	}
}

byte SciTTS::logCharacter(reg_t buffer, uint16 offset, byte value) {
	// The log tests the character it just copied to decide whether a word ended.
	// Stop at aligned word ends too, including words adjacent to a line break.
	// The copy itself reads offset == shown and must always get the real byte.
	if (_logWords.empty() || buffer != _logBuffer || _logScript.isNull()) return value;
	EngineState *s = g_sci->getEngineState();
	Script *script = s->_segMan->getScriptIfLoaded(_logScript.getSegment());
	if (!script || script->getLocalsCount() <= 414 || script->getLocalsBegin()[414].toUint16() != offset + 1) return value;
	if (value && value <= 32) return ' ';
	for (const auto &word : _logWords) if (word.end == uint32(offset) + 1) return ' ';
	return value;
}

bool SciTTS::skipLogSection() {
	EngineState *s = g_sci->getEngineState();
	if (_logScript.isNull() || s->currentRoomNumber() != 104 ||
		!s->_segMan->getObject(_logScript) ||
		Common::String(s->_segMan->getObjectName(_logScript)) != "sCaptainsLog") return false;
	Script *script = s->_segMan->getScriptIfLoaded(_logScript.getSegment());
	if (!script || script->getScriptNumber() != 104 || script->getLocalsCount() <= 416) return false;
	const uint16 phase = readSelectorValue(s->_segMan, _logScript, SELECTOR(state));
	if (phase > 1) return false;
	// Word reveal waits in state 0; the original click handler only skips
	// from state 1. Schedule the existing erase/next-section state (2).
	// Do not invoke script methods recursively inside kGetEvent.
	writeSelectorValue(s->_segMan, _logScript, SELECTOR(state), 1);
	writeSelectorValue(s->_segMan, _logScript, g_sci->getKernel()->findSelector("ticks"), 0);
	writeSelectorValue(s->_segMan, _logScript, g_sci->getKernel()->findSelector("seconds"), 0);
	writeSelectorValue(s->_segMan, _logScript, g_sci->getKernel()->findSelector("cycles"), 1);
	stop();
	return true;
}

void SciTTS::updateLogTiming() {
	if (_logScript.isNull()) return;
	EngineState *s = g_sci->getEngineState();
	if (s->currentRoomNumber() != 104 || !s->_segMan->getObject(_logScript) ||
		Common::String(s->_segMan->getObjectName(_logScript)) != "sCaptainsLog") {
		stop();
		return;
	}
	Script *script = s->_segMan->getScriptIfLoaded(_logScript.getSegment());
	if (!script || script->getScriptNumber() != 104 || script->getLocalsCount() <= 416 ||
		s->variablesMax[VAR_GLOBAL] <= 88) { _logScript = NULL_REG; return; }
	// Only the supplied English intro's word-buffer layout is supported.
	reg_t *locals = script->getLocalsBegin();
	const uint32 length = locals[413].toUint16();
	const uint32 shown = locals[414].toUint16();
	if (!length || length != _logText.size() || shown > length + 1) { _logScript = NULL_REG; return; }
	const uint16 phase = readSelectorValue(s->_segMan, _logScript, SELECTOR(state));
	const bool playing = g_system->getMixer()->isSoundHandleActive(_voiceHandle);
	if (phase == 0 && shown < length) {
		uint32 next = shown;
		while (next < length && _logText[next] != ' ') ++next;
		if (next < length) ++next;
		uint32 target = uint64(_voiceDurationMs) * next / length;
		if (!_logWords.empty()) {
			target = 0;
			if (byte(_logText[shown]) > 32)
				for (const auto &word : _logWords) if (word.end > shown) { target = word.ms; break; }
			// Initial cycles=1 would otherwise bypass the first word's timestamp.
			writeSelectorValue(s->_segMan, _logScript, g_sci->getKernel()->findSelector("cycles"), 0);
		}
		const uint32 elapsed = g_system->getMixer()->getSoundElapsedTime(_voiceHandle);
		const uint32 remaining = playing && target > elapsed ? target - elapsed : 0;
		writeSelectorValue(s->_segMan, _logScript, g_sci->getKernel()->findSelector("ticks"), MIN<uint32>(32767, (remaining * 60 + 999) / 1000 + 1));
	} else if (phase == 1 && shown >= length && !playing) {
		// Script:cue advances to the existing erase-and-next-section state.
		writeSelectorValue(s->_segMan, _logScript, g_sci->getKernel()->findSelector("cycles"), 1);
		if (_voiceCapture) {
			Common::String event = "#log-complete\t" + _lastVoice + "\n";
			_voiceCapture->write(event.c_str(), event.size()); _voiceCapture->flush();
		}
		_logScript = NULL_REG;
	} else if (phase >= 2) {
		stop(); // manual next/skip still follows the game's own event handling
	}
}

#include "sci/engine/recorded_mouth_profiles.h"

int SciTTS::endingFloMouthCel() {
	if (!_recordedVoices || g_sci->getEngineState()->currentRoomNumber() != 1041) return -1;
	SegManager *seg = g_sci->getEngineState()->_segMan;
	const auto closing = seg->findObjectsByName("sClosing");
	if (closing.empty() || readSelectorValue(seg, closing[0], SELECTOR(state)) != 65535) return -1;
	if (_lastVoice != "1041-2-0-0-3" || !g_system->getMixer()->isSoundHandleActive(_voiceHandle)) return 0;
	const uint32 ms = g_system->getMixer()->getSoundElapsedTime(_voiceHandle);
	const uint32 index = uint64(ms) * 40 / 1000;
	const float energy = index < _voiceEnvelope.size() ? _voiceEnvelope[index] / _voiceEnvelopeReference : 0;
	if (energy <= 0.13f) return 0;
	return energy > 0.65f && (ms / 83) % 3 != 0 ? 2 : 1;
}

int SciTTS::recordedMouthCel(reg_t object, int celCount) {
	if (!_recordedVoices || celCount < 2) return -1;
	SegManager *seg = g_sci->getEngineState()->_segMan;
	const int mouth = g_sci->getKernel()->findSelector("mouth");
	if (!seg->getObject(object)) return -1;
	const bool ownedMouth = !_voicePortraitOwner.isNull() && seg->getObject(_voicePortraitOwner) && mouth >= 0 &&
		lookupSelector(seg, _voicePortraitOwner, mouth, nullptr, nullptr) == kSelectorVariable &&
		readSelector(seg, _voicePortraitOwner, mouth) == object;
	// The opening simulator uses a cast mouth with graphic speech bubbles,
	// rather than a Talker owner. Restrict this exception to its two spoken shots.
	const bool simulatorMouth = g_sci->getEngineState()->currentRoomNumber() == 104 &&
		Common::String(seg->getObjectName(object)) == "rogMouth" &&
		((_lastVoice == "104-2-0-0-2" && readSelectorValue(seg, object, SELECTOR(view)) == 101) ||
		 (_lastVoice == "104-2-0-0-4" && readSelectorValue(seg, object, SELECTOR(view)) == 97)) &&
		readSelectorValue(seg, object, SELECTOR(loop)) == 1;
	const int view = readSelectorValue(seg, object, SELECTOR(view));
	const int loop = readSelectorValue(seg, object, SELECTOR(loop));
	const int room = g_sci->getEngineState()->currentRoomNumber();
	// Keep the last reviewed mouth neutral after the recording or its window
	// ends. Otherwise returning -1 gives control back to SCI's random cycling.
	// Match the object, room and speech loop so action/eye animations are exempt.
	const bool heldMouth = object == _heldMouth && room == _heldMouthRoom &&
		view == _heldMouthView && loop == _heldMouthLoop;
	if (!ownedMouth && !simulatorMouth && !heldMouth) return -1;
	const RecordedMouthProfile *profile = nullptr;
	for (uint i = 0; i < ARRAYSIZE(kRecordedMouthProfiles); ++i) {
		const RecordedMouthProfile &p = kRecordedMouthProfiles[i];
		if (p.view == view && p.loop == loop && (p.room < 0 || p.room == room)) {
			profile = &p; break;
		}
	}
	// Only the active talker's actual mouth and its reviewed speech loop qualify.
	// Arcade/action loops and eye layers never match.
	if (!profile || profile->count != celCount || profile->closed >= celCount) return -1;
	_heldMouth = object;
	_heldMouthRoom = room;
	_heldMouthView = view;
	_heldMouthLoop = loop;
	if (!g_system->getMixer()->isSoundHandleActive(_voiceHandle)) return profile->closed;
	const uint32 step = uint64(g_system->getMixer()->getSoundElapsedTime(_voiceHandle)) * 24 / (1000 * profile->timing);
	const uint32 index = uint64(step) * profile->timing * 40 / 24;
	const float energy = index < _voiceEnvelope.size() ? _voiceEnvelope[index] / _voiceEnvelopeReference * profile->sensitivity : 0;
	if (energy <= 0.13f) return profile->closed;
	static const int strong[] = { 1, 2, 4, 2 };
	static const int soft[] = { 3, 5, 3, 6 };
	const int shape = energy > 0.65f ? strong[step % 4] : soft[step % 4];
	int cel = (shape - 1) % (celCount - 1);
	if (cel >= profile->closed) ++cel;
	return cel;
}

bool SciTTS::recordedMouthCycle(reg_t object) {
	if (!_recordedVoices) return false;
	SegManager *seg = g_sci->getEngineState()->_segMan;
	if (!seg->getObject(object)) return false;
	for (uint i = 0; i < ARRAYSIZE(kRecordedMouthProfiles); ++i) {
		const RecordedMouthProfile &p = kRecordedMouthProfiles[i];
		if (readSelectorValue(seg, object, SELECTOR(view)) == (uint)p.view &&
			readSelectorValue(seg, object, SELECTOR(loop)) == (uint)p.loop)
		{
			const bool active = recordedMouthCel(object, p.count) >= 0;
			return active;
		}
	}
	return false;
}

int SciTTS::recordedDrawMouthCel(int view, int loop, int celCount) {
	if (!_recordedVoices) return -1;
	if (!_heldMouth.isNull() && view == _heldMouthView && loop == _heldMouthLoop &&
		g_sci->getEngineState()->currentRoomNumber() == _heldMouthRoom &&
		!g_system->getMixer()->isSoundHandleActive(_voiceHandle))
		return recordedMouthCel(_heldMouth, celCount);
	SegManager *seg = g_sci->getEngineState()->_segMan;
	const int selector = g_sci->getKernel()->findSelector("mouth");
	if (_voicePortraitOwner.isNull() || !seg->getObject(_voicePortraitOwner) || selector < 0 ||
		lookupSelector(seg, _voicePortraitOwner, selector, nullptr, nullptr) != kSelectorVariable) return -1;
	const reg_t object = readSelector(seg, _voicePortraitOwner, selector);
	if (!seg->getObject(object) || readSelectorValue(seg, object, SELECTOR(view)) != (uint)view ||
		readSelectorValue(seg, object, SELECTOR(loop)) != (uint)loop) return -1;
	return recordedMouthCel(object, celCount);
}

void SciTTS::updateRecordedTiming() {
	if (_recordedVoices && _lastVoice.hasPrefix("752-200-")) {
		EngineState *state = g_sci->getEngineState();
		Script *terminal = state->_segMan->getScriptIfLoaded(state->_segMan->getScriptSegment(750));
		int module, noun, verb, page, sequence;
		if (sscanf(_lastVoice.c_str(), "%d-%d-%d-%d-%d", &module, &noun, &verb, &page, &sequence) != 5 ||
			state->currentRoomNumber() != 750 || !terminal || terminal->getLocalsCount() <= 3 ||
			terminal->getLocalsBegin()[3].toUint16() != page)
			stop();
	}
	// Klorox's computer paints its logs directly; there is no Narrator:say
	// dismissal to stop the recording. The log belongs to the room-460 UI.
	if (_recordedVoices && _lastVoice.hasPrefix("460-") &&
		g_sci->getEngineState()->currentRoomNumber() != 460) {
		stop();
	}
	updateLogTiming();
	if (_recordedVoices && (_lastVoice == "166-1-0-0-1" || _lastVoice == "166-1-0-0-2")) {
		EngineState *s = g_sci->getEngineState();
		Script *letter = s->_segMan->getScriptIfLoaded(s->_segMan->getScriptSegment(166));
		// Local 201 is set while the letter is open and cleared on dismissal.
		if (s->currentRoomNumber() != 166 || !letter || letter->getLocalsCount() <= 201 ||
			letter->getLocalsBegin()[201].isNull()) {
			stop();
		} else if (!_letterNextText.empty() && !g_system->getMixer()->isSoundHandleActive(_voiceHandle)) {
			Common::String next = _letterNextText;
			_letterNextText.clear();
			playRecorded("166-1-0-0-2", next);
		}
	}
	if (!_recordedVoices || _voiceNarrator.isNull()) return;
	EngineState *state = g_sci->getEngineState();
	int ticksSelector = g_sci->getKernel()->findSelector("ticks");
	if (ticksSelector < 0 || !state->_segMan->getObject(_voiceNarrator) ||
		lookupSelector(state->_segMan, _voiceNarrator, ticksSelector, nullptr, nullptr) != kSelectorVariable ||
		state->variablesMax[VAR_GLOBAL] <= 0x58) {
		_voiceNarrator = NULL_REG;
		return;
	}
	// Let Narrator:say finish initializing ticks before changing its deadline.
	int saySelector = g_sci->getKernel()->findSelector("say");
	for (const auto &frame : state->_executionStack)
		if (frame.objp == _voiceNarrator && frame.debugSelector == saySelector) return;
	uint16 ticks = readSelectorValue(state->_segMan, _voiceNarrator, ticksSelector);
	if (ticks == 0xffff) { _voiceNarrator = NULL_REG; return; } // no open message
	uint16 now = state->variables[VAR_GLOBAL][0x58].toUint16();
	bool playing = g_system->getMixer()->isSoundHandleActive(_voiceHandle);
	{
		// Refresh a short hold while the mixer is playing, instead of guessing
		// an end time from WAV length. SCI time and audio time can diverge.
		// Narrator:doit tests strictly greater-than, hence now - 1 on completion.
		uint16 deadline = playing ? uint16(now + 60) : uint16(now - 1);
		if (deadline == 0xffff) deadline = 0; // SCI's idle sentinel is never a deadline
		writeSelectorValue(state->_segMan, _voiceNarrator, ticksSelector, deadline);
		if (_voiceCapture && _voiceTimingPending) {
			Common::String event = "#timed\t" + _lastVoice + "\n";
			_voiceCapture->write(event.c_str(), event.size()); _voiceCapture->flush();
		}
		_voiceTimingPending = false;
		// Let the existing Narrator:doit close spoken lab dialogue normally.
		// Never invoke script disposal recursively from inside kGetEvent.
		const bool labRoger = _lastVoice == "240-1-2-1-2" || _lastVoice == "240-1-2-1-4" ||
			_lastVoice == "240-1-2-1-6" || _lastVoice == "240-1-2-1-10" ||
			_lastVoice == "240-1-2-1-12" || _lastVoice == "240-1-2-1-15";
		// ChoiceTalker retains windows for menus, but normal spoken lines
		// must finish through Narrator:doit when their recording ends. Cover
		// every normal-mode talker, not a growing list of message IDs.
		const int normal = g_sci->getKernel()->findSelector("normal");
		const bool ordinarySpeech = normal < 0 ||
			lookupSelector(state->_segMan, _voiceNarrator, normal, nullptr, nullptr) != kSelectorVariable ||
			readSelectorValue(state->_segMan, _voiceNarrator, normal) != 0;
		if (!playing && (ordinarySpeech || (labRoger && state->currentRoomNumber() == 240)) &&
			_labAutoCloseTalker.isNull()) {
			const int keep = g_sci->getKernel()->findSelector("keepWindow");
			if (keep >= 0 && lookupSelector(state->_segMan, _voiceNarrator, keep, nullptr, nullptr) == kSelectorVariable) {
				_labAutoCloseTalker = _voiceNarrator;
				_labKeepWindow = readSelectorValue(state->_segMan, _voiceNarrator, keep);
				writeSelectorValue(state->_segMan, _voiceNarrator, keep, 0);
			}
		}
		if (!playing) _voiceNarrator = NULL_REG;
	}
}

void SciTTS::button(const Common::String &text) {
	if (_ttsMan != nullptr && shouldPerformTTS(text))
		_ttsMan->say(getMessage(text), Common::TextToSpeechManager::QUEUE_NO_REPEAT);
}

void SciTTS::text(const Common::String &text) {
	if (_ttsMan != nullptr && shouldPerformTTS(text))
		_ttsMan->say(getMessage(text), Common::TextToSpeechManager::INTERRUPT);
}

void SciTTS::stop() {
	_voicePortraitOwner = NULL_REG;
	_voiceEnvelope.clear();
	// kDisposeWindow calls stop after the normal dialogue lifecycle has elected
	// to close. Restore the choice speaker's original policy for later menus.
	if (!_labAutoCloseTalker.isNull()) {
		SegManager *seg = g_sci->getEngineState()->_segMan;
		const int keep = g_sci->getKernel()->findSelector("keepWindow");
		if (seg->getObject(_labAutoCloseTalker) && keep >= 0)
			writeSelectorValue(seg, _labAutoCloseTalker, keep, _labKeepWindow);
		_labAutoCloseTalker = NULL_REG;
	}
	_letterNextText.clear();
	_logScript = NULL_REG;
	_logText.clear();
	_logWords.clear();
	_logBuffer = NULL_REG;
	_voiceNarrator = NULL_REG;
	_voiceTimingPending = false;
	g_system->getMixer()->stopHandle(_voiceHandle);
	_lastVoice.clear();
	if (_ttsMan != nullptr)
		_ttsMan->stop();
}

void SciTTS::setMessage(const Common::String &text) {
	if (text.size() > 0)
		_curMessage = text;
}

Common::String SciTTS::getMessage(const Common::String &text) {
	Common::String message = text;

	// If the current message contains a substring of the text to be displayed,
	// minus the first letter, prefer the message instead. The first letter is
	// chopped off in messages in games such as KQ6 and is replaced with tabs in
	// KQ6 or spaces in KQ5, so that a calligraphic first letter is drawn instead.
	if (_curMessage.size() > 0 && text.size() > 0 && text.hasSuffix(_curMessage.substr(1)))
		message = _curMessage;

	// Strip color code characters in SCI1.1
	if (getSciVersion() == SCI_VERSION_1_1) {
		int32 index = message.find('|');

		while (index >= 0) {
			do {
				message.deleteChar(index);
			} while (message.size() > 0 && message[index] != '|');

			if (message.size() > 0)
				message.deleteChar(index);

			index = message.find('|');
		}
	}

	return message;
}

bool SciTTS::shouldPerformTTS(const Common::String &message) const {
	SciGameId gameId = g_sci->getGameId();
	uint16 roomNumber = g_sci->getEngineState()->currentRoomNumber();

	// Check if it's an actual message, by checking for the
	// existence of any vowel.
	// For example, when talking to the alien in SQ5 room 500, a
	// series of symbols is shown, as part of a joke.
	if (!message.contains('a') &&
		!message.contains('e') &&
		!message.contains('i') &&
		!message.contains('o') &&
		!message.contains('u'))
		return false;

	// Skip TTS for QFG4 room 140 (character creation screen).
	if (gameId == GID_QFG4 && roomNumber == 140)
		return false;

	return true;
}

} // End of namespace Sci
