/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_PHONE_VISUAL_H
#define SNATCHER_PHONE_VISUAL_H
#include <cstdint>
#include <string>
#include <vector>
#include "scene_renderer.h"
namespace Snatcher {
// Native game view state recovered from Main348E; no display registers or IRQs.
struct PhoneVisualState {
 uint8_t mode,tag;uint16_t phase,basePhase;int16_t offset,baseOffset,publishedPlaneBY;bool temporaryDialogue;
 PhoneVisualState():mode(0),tag(0),phase(0),basePhase(0),offset(0),baseOffset(0),publishedPlaneBY(0),temporaryDialogue(false){}
};
struct PhoneVisualBand {unsigned begin,end;int16_t planeAY,planeBY;};
// Physical frame program selected before accepting this frame's mailbox.
// publishedPlaneBY in PhoneVisualState is the EB00 shadow; upperBY is the
// already-published physical value and survives a later full cleanup.
struct PhoneFramePublication {
 enum Program {kDelegate,kSplit80Restore144,kSplit80,kSplit73};
 bool known;uint8_t tag,program;int16_t upperBY;
 PhoneFramePublication():known(false),tag(0),program(kDelegate),upperBY(0){}
};
bool validatePhoneFramePublication(const PhoneFramePublication& frame,std::string& error);
bool beginPhonePublication(PhoneVisualState& state,PhoneFramePublication& frame,std::string& error);
bool validatePhoneVisualState(const PhoneVisualState& state,std::string& error);
enum PhoneVisualRequestResult {kPhoneVisualAccepted,kPhoneVisualStartOrdinaryDialogue,kPhoneVisualDelegate,kPhoneVisualInvalid};
// 0F is special only while17 owns the view. FE retains the source published
// offset unless17 or the recovered full cleanup explicitly clears it.
PhoneVisualRequestResult requestPhoneVisual(PhoneVisualState& state,uint16_t command,std::string& error);
bool tickPhoneVisual(PhoneVisualState& state,std::string& error);
bool phoneVisualBusy(const PhoneVisualState& state);
// Main's lower-view publication copies only secondary colors80..127 into
// displayed colors16..63. The first16 text colors retain their existing owner.
// Source payload extraction is independent of publication timing.
bool readLowerViewPalette(const std::vector<uint16_t>& sourceColors,const std::vector<bool>& known,std::vector<uint16_t>& colors,std::string& error);
// Main0C6C publishes twelve four-color groups at rows136..147.
// The caller owns the retained displayed baseline before these publications.
bool lowerViewPaletteSpans(const std::vector<uint16_t>& sourceColors,const std::vector<bool>& known,std::vector<ScenePaletteSpan>& spans,std::string& error);

enum GameLayoutRequest {kGameLayoutUnchanged,kGameLayoutTall,kGameLayoutWide};
// Display-list controller8 is an exact word policy; FFFF is wide, not FF.
GameLayoutRequest decodeGameLayoutRequest(uint16_t word);
// Unknown legacy publication or the ordinary program delegates to stored layout.
bool phoneVisualBands(const PhoneFramePublication& frame,const PhoneVisualState& state,unsigned height,std::vector<PhoneVisualBand>& bands,std::string& error);
}
#endif
