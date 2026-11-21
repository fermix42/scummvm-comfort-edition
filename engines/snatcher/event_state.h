/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_EVENT_STATE_H
#define SNATCHER_EVENT_STATE_H
#include "events.h"
#include "state_codec.h"
namespace Snatcher {
// Native scheduler-owned SNSV payload only. A full gameplay save additionally
// needs every owner snapshot and an established permitted save boundary.
bool encodeEventState(const EventSnapshot& state,NativeSaveSection& section,std::string& error);
bool decodeEventState(const NativeSaveSection& section,EventSnapshot& state,std::string& error);
}
#endif
