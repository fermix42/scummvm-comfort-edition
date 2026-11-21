/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef BACKENDS_PLATFORM_SDL_WIN32_PROCESS_METRICS_H
#define BACKENDS_PLATFORM_SDL_WIN32_PROCESS_METRICS_H
#include "common/scummsys.h"
namespace Win32 {
struct ProcessMetrics {
 bool known=false;uint64 workingSet=0,privateBytes=0,cpu100ns=0,availablePhysical=0;uint32 handles=0;
};
// Read-only metrics for this process, without enumerating other applications.
ProcessMetrics processMetrics();
}
#endif
