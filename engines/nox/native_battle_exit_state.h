/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_EXIT_STATE_H
#define NOX_NATIVE_BATTLE_EXIT_STATE_H
namespace Nox { namespace Native {
// 0ordinary input,1confirmation,2refusal acknowledgement,3committed flee.
// Control preserves the source no-flee bit or the subsequent three-step grace.
struct BattleExitState {
    byte phase,control;
    BattleExitState() : phase(0),control(0) {}
    bool valid() const { return phase<=3 && (phase!=3 || control==3); }
};
} }
#endif
