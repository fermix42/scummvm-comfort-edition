/* SPDX-License-Identifier: GPL-3.0-or-later */
// Reused synthesis from the Nox standalone conversion; no host audio I/O.
#ifndef NOX_MOCKINGBOARD_H
#define NOX_MOCKINGBOARD_H
#include "common/scummsys.h"
namespace Nox {
typedef struct {
  byte reg[16],selected,tone[3],noise,attack,holding,env_step;
  uint32 tone_count[3],noise_count,env_count,lfsr;
  uint64 tick,writes;
} NoxAY;
typedef struct {
  byte reg[16],ifr,ier,running;
  uint16 latch;
  uint32 remaining;
  uint64 expirations;
} NoxVIA;
typedef struct {
  NoxAY ay[2];NoxVIA via[2];uint64 cycle;
  double last_input,last_output;
} Mockingboard;
void mock_init(Mockingboard *m);
void mock_advance(Mockingboard *m,uint64 cycle);
int mock_irq(const Mockingboard *m);
/* Read-only deadline query for bounded native compatibility operations. */
int mock_irq_before(const Mockingboard *m,uint64 cycle);
byte mock_read(Mockingboard *m,unsigned offset);
void mock_write(Mockingboard *m,unsigned offset,byte value,uint64 cycle);
int mock_sample(Mockingboard *m,uint64 cycle);
// Native content sequencers address AY registers directly; no VIA/IRQ protocol.
void mock_set_ay_register(Mockingboard *m,unsigned chip,unsigned reg,byte value,uint64 cycle);
} // namespace Nox
#endif
