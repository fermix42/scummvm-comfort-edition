/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_LINE_SCROLL_H
#define SNATCHER_LINE_SCROLL_H
#include <array>
#include <cstdint>
#include <string>
namespace Snatcher {
// Semantic horizontal sampling offsets for the source's persistent visual mode3.
// Rows0..7 and136 onward remain owned by the surrounding scene.
struct NativeLineScrollState {
 uint32_t version=2;
 uint8_t mode=0, phase=0;
 bool lineEnabled=false, completed=false;
 bool active=false, counterKnown=false;
 uint16_t lastGlobalCounter=0, publishedPrefix=0;
 std::array<uint16_t,5> counters{};
 std::array<uint16_t,128> computed{}, published{};
};
class NativeLineScroll {
public:
 void reset();
 void start();
 bool start(unsigned mode,std::string& error);
 bool tick(uint16_t globalCounter,std::string& error);
 const NativeLineScrollState& state()const{return _state;}
 bool restore(const NativeLineScrollState& state,std::string& error);
 int sourceX(unsigned row)const;
private:
 NativeLineScrollState _state;
};
}
#endif
