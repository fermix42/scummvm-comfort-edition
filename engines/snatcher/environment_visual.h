/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_ENVIRONMENT_VISUAL_H
#define SNATCHER_ENVIRONMENT_VISUAL_H
#include "presentation.h"
#include <array>
namespace Snatcher {
// Authored environmental draw-list data. This is a game compositor, not an
// audio loop, register model, or interpreter for executable instructions.
struct EnvironmentVisualTables {
 bool known;std::array<uint8_t,3> counts,minX,maxX,maxY;std::array<uint8_t,16> drift;
 std::array<uint8_t,32> initialX;std::array<uint16_t,8> smallTiles;
 std::array<uint8_t,112> crossingSteps;std::array<uint8_t,8> crossingOffsets;std::array<uint16_t,4> crossingTiles;
 uint64_t sourceStamp;
 EnvironmentVisualTables():known(false),sourceStamp(0){}
};
bool readEnvironmentVisualTables(const uint8_t* sub,size_t size,EnvironmentVisualTables& out,std::string& error);
struct EnvironmentVisualSlot {bool known;PlacedPiece piece;EnvironmentVisualSlot():known(false),piece{0,0,8,8,0}{}};
struct EnvironmentVisualState {std::array<EnvironmentVisualSlot,64> previous;};
// Each call is one real source draw-list submission. prefix is actors0..31;
// suffix actors32..63. Unwritten tail slots retain their own previous values.
// A screenshot/re-render must use the resulting cached submission, not call it
// again. Legacy archives without this history must retain unknown tail slots.
bool composeEnvironmentVisual(const EnvironmentVisualTables& tables,uint16_t mode,uint16_t counter,
 const std::vector<PlacedPiece>& prefix,const std::vector<PlacedPiece>& suffix,
 EnvironmentVisualState& state,std::vector<PlacedPiece>& out,std::string& error);
}
#endif
