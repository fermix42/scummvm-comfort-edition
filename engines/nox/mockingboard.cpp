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
void mock_advance(Mockingboard *m,uint64 cycle){
  if(cycle<m->cycle)return;
  uint64 clocks=cycle-m->cycle;m->cycle=cycle;
  for(unsigned i=0;i<2;i++){
    NoxVIA *v=&m->via[i];if(!v->running)continue;
    if(clocks<v->remaining){v->remaining-=(uint32)clocks;continue;}
    uint64 extra=clocks-v->remaining;v->ifr|=64;v->expirations++;
    if(v->reg[11]&64){uint32 reload=(uint32)v->latch+2;v->expirations+=extra/reload;v->remaining=reload-(uint32)(extra%reload);}
    else{v->remaining=65536-(uint32)(extra%65536);v->running=0;}
  }
}
int mock_irq(const Mockingboard *m){return ((m->via[0].ifr&m->via[0].ier)|(m->via[1].ifr&m->via[1].ier))!=0;}
int mock_irq_before(const Mockingboard *m,uint64 cycle){
  if(mock_irq(m))return 1;
  if(cycle<=m->cycle)return 0;
  uint64 duration=cycle-m->cycle;
  for(unsigned i=0;i<2;i++)if(m->via[i].running&&(m->via[i].ier&0x40)&&m->via[i].remaining<=duration)return 1;
  return 0;
}
byte mock_read(Mockingboard *m,unsigned offset){
  unsigned i=(offset>>7)&1,r=offset&15;NoxVIA *v=&m->via[i];
  if(r==13)return v->ifr|((v->ifr&v->ier)?128:0);
  if(r==14)return v->ier|128;
  if(r==4){v->ifr&=~64;return (byte)(v->remaining-1);}
  if(r==5)return (byte)((v->remaining-1)>>8);
  if(r==6)return (byte)v->latch;if(r==7)return v->latch>>8;
  if(r==1||r==15){
    if((v->reg[0]&7)==5)return m->ay[i].reg[m->ay[i].selected&15];
    return (v->reg[1]&v->reg[3])|((byte)~v->reg[3]);
  }
  return v->reg[r];
}
void mock_write(Mockingboard *m,unsigned offset,byte value,uint64 cycle){
  mock_advance(m,cycle);unsigned i=(offset>>7)&1,r=offset&15;
  NoxVIA *v=&m->via[i];NoxAY *a=&m->ay[i];
  if(r==13){v->ifr&=~(value&127);return;}
  if(r==14){if(value&128)v->ier|=value&127;else v->ier&=~value;return;}
  v->reg[r]=value;
  if(r==15)v->reg[1]=value;
  if(r==4||r==6)v->latch=(v->latch&0xff00)|value;
  if(r==5||r==7){v->latch=(v->latch&255)|(value<<8);v->ifr&=~64;
    if(r==5){v->remaining=(uint32)v->latch+2;v->running=1;}}
  if(r==0){
    ay_advance(a,cycle);unsigned control=value&v->reg[2]&7;
    if(!(control&4))ay_reset(a);
    else if(control==7)a->selected=v->reg[1]&15;
    else if(control==6)ay_write(a,v->reg[1]);
  }
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
