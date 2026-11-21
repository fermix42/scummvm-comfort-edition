/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_PRESENTATION_BOOKMARK_H
#define SNATCHER_PRESENTATION_BOOKMARK_H
#include <array>
#include <cstdint>
#include <string>
namespace Snatcher {
class SceneRuntime;
class PointerInteraction;
struct NativePresentationBookmark {
 bool valid=false,pcmKnown=false,quiet=false,suppressed=false;
 uint16_t presentation=0,stream=0xffff,ambient=0,interaction=0;
 uint8_t music=0;std::array<uint8_t,64> actors{};
};
// Original1500E restores policies while queuing display jobs. It does not
// perform native10's position reset, nor clear suppression when its bit is0.
// Cold pointer initialization belongs to the preceding source module reset.
bool restorePresentationBookmarkPolicies(const NativePresentationBookmark& bookmark,
 SceneRuntime& scene,PointerInteraction& pointer,std::string& error);
}
#endif
