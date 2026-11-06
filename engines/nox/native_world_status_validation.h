/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_STATUS_VALIDATION_H
#define NOX_NATIVE_WORLD_STATUS_VALIDATION_H
#include "nox/native_world_status.h"
namespace Nox {
inline bool validateNativeWorldStatus(const Native::WorldStatus &status,Common::SeekableReadStream &input,Common::String &report,Common::String &error) {
    if (input.readUint32BE()!=MKTAG('N','H','U','V') || input.readUint32LE()!=1) return false;
    const uint32 count=input.readUint32LE();if (count!=1452 || input.size()-input.pos()!=int64(count)*326) return false;
    for (uint i=0;i<count;++i) {
        const uint16 gold=input.readUint16LE(),food=input.readUint16LE();Native::TurnClock clock;
        clock.hour=input.readByte();clock.minute=input.readByte();byte expected[320];if (input.read(expected,320)!=320) return false;
        Native::HgrFrame frame;if (!status.initializeFrame(frame) || !status.paintStatus(gold,food,clock,frame)) return false;
        uint at=0;
        for (uint y=8;y<72;++y) for (uint x=34;x<39;++x,++at) {
            const byte actual=frame.data()[Native::HgrFrame::rowOffset(y)+x];
            if (actual!=expected[at]) { error=Common::String::format("Native status differs from original case%u at%u/%u: %u vs%u",i,x,y,uint(actual),uint(expected[at]));return false; }
        }
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report+=Common::String::format("native-world-status\tcases=%u\tvalid_clock_minutes=1440\tcurrency_widths=12\tvisible_bytes=%u\tcpu_execution=0\n",count,count*320);return true;
}
}
#endif
