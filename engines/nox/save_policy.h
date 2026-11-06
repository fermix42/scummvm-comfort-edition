/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_SAVE_POLICY_H
#define NOX_SAVE_POLICY_H
namespace Nox {
enum SaveMode { kModernSaveMode=0, kClassicSaveMode=1 };
enum { kMaximumSaveSlot=999 };
inline bool allowedSaveSlot(SaveMode mode,int slot) {
	return mode==kClassicSaveMode ? slot==1||slot==2 : mode==kModernSaveMode&&slot>=0&&slot<=kMaximumSaveSlot;
}
}
#endif
