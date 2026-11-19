/* SPDX-License-Identifier: GPL-3.0-or-later */
// Reused standalone synthesis, with no host I/O.
#include "mockingboard.h"
#include <string.h>
namespace Nox {
/* Slot-4 Mockingboard: two 6522 VIAs and two AY-3-8910 PSGs.
   Implements the timer/port path exercised by Nox's original driver. */
static unsigned period(unsigned n){return n?n:1;}
static void ay_reset(NoxAY *a){
  uint64 writes=a->writes,tick=a->tick;memset(a,0,sizeof(*a));
  a->writes=writes;a->tick=tick;a->lfsr=1;a->noise=1;
  for(int c=0;c<3;c++){a->tone[c]=1;a->tone_count[c]=1;}
  a->noise_count=2;a->env_count=2;a->env_step=15;
}
void mock_init(Mockingboard *m){memset(m,0,sizeof(*m));for(int i=0;i<2;i++)ay_reset(&m->ay[i]);}
static void ay_advance(NoxAY *a,uint64 cycle){
  uint64 stop=cycle/8;
  while(a->tick<stop){
    a->tick++;
    for(unsigned c=0;c<3;c++)if(!--a->tone_count[c]){
      a->tone[c]^=1;a->tone_count[c]=period(a->reg[c*2]|((a->reg[c*2+1]&15)<<8));
    }
    if(!--a->noise_count){
      unsigned feedback=(a->lfsr^(a->lfsr>>3))&1;
      a->lfsr=(a->lfsr>>1)|(feedback<<16);a->noise=a->lfsr&1;
      a->noise_count=period(a->reg[6]&31)*2;
    }
    if(!--a->env_count){
      if(!a->holding){
        if(a->env_step)a->env_step--;
        else{
          unsigned shape=a->reg[13];
          if(!(shape&8)){a->holding=1;a->attack=0;a->env_step=0;}
          else if(shape&1){if(shape&2)a->attack^=15;a->holding=1;}
          else{if(shape&2)a->attack^=15;a->env_step=15;}
        }
      }
      a->env_count=period(a->reg[11]|(a->reg[12]<<8))*2;
    }
  }
}
static void ay_write(NoxAY *a,byte value){
  static const byte mask[16]={255,15,255,15,255,15,31,255,31,31,31,255,255,15,255,255};
  unsigned r=a->selected&15;a->reg[r]=value&mask[r];a->writes++;
  if(r==13){a->attack=(value&4)?15:0;a->env_step=15;a->holding=0;a->env_count=period(a->reg[11]|(a->reg[12]<<8))*2;}
}
void mock_set_ay_register(Mockingboard *m,unsigned chip,unsigned reg,byte value,uint64 cycle){
  if(chip>=2||reg>=16)return;
  ay_advance(&m->ay[chip],cycle);m->ay[chip].selected=byte(reg);ay_write(&m->ay[chip],value);
}
int mock_sample(Mockingboard *m,uint64 cycle){
  /* Logarithmic 16-level DAC approximation, six channels mixed to mono. */
  static const int amplitude[16]={0,28,40,57,81,114,161,228,322,455,644,910,1287,1820,2574,3640};
  int sum=0;
  for(unsigned i=0;i<2;i++){
    NoxAY *a=&m->ay[i];ay_advance(a,cycle);
    for(unsigned c=0;c<3;c++){
      unsigned volume=a->reg[8+c],level=(volume&16)?(a->env_step^a->attack):(volume&15);
      int gate=(a->tone[c]||((a->reg[7]>>c)&1))&&(a->noise||((a->reg[7]>>(c+3))&1));
      if(gate)sum+=amplitude[level];
    }
  }
  double filtered=sum-m->last_input+0.995*m->last_output;
  m->last_input=sum;m->last_output=filtered;return (int)filtered;
}

} // namespace Nox
