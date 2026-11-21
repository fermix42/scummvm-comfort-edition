/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_TIMING_STATE_H
#define SNATCHER_TIMING_STATE_H
#include "game_clock.h"
#include "delay.h"
#include "events.h"
#include "state_codec.h"
namespace Snatcher {
// Semantic timing owner only. Host milliseconds and audio/device positions are
// deliberately absent. This does not establish a permitted gameplay save point.
struct TimingSnapshot {
 GameClockState clock;NarrativeDelaySnapshot delay;EventToken delayToken;
};
bool encodeTimingState(const TimingSnapshot& state,NativeSaveSection& section,std::string& error);
bool decodeTimingState(const NativeSaveSection& section,TimingSnapshot& state,std::string& error);
// Restores into temporary owners first. A live delay must correspond to the
// saved scheduler command event and its fresh, unresolved restored token.
bool restoreTimingState(const TimingSnapshot& state,const EventSnapshot& savedEvents,const std::vector<EventRebind>& rebind,GameClock& clock,NarrativeDelay& delay,EventToken& token,std::string& error);
}
#endif
