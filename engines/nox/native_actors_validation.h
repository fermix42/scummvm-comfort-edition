/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ACTORS_VALIDATION_H
#define NOX_NATIVE_ACTORS_VALIDATION_H
#include "nox/native_actors.h"
namespace Nox {
inline bool validateNativeActors(Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','A','A','D') || input.readUint32LE() != 1 || input.readUint32LE() != 12348) return false;
    uint dispositions[3] = {};
    for (uint i = 0; i < 12348; ++i) {
        const byte bank = input.readByte(), mode = input.readByte(); byte fields[256], tables[1792], expected[1792], view[8];
        if (mode > 1 || input.read(fields, sizeof(fields)) != sizeof(fields) || input.read(tables, sizeof(tables)) != sizeof(tables)) return false;
        const byte disposition = input.readByte();
        if (disposition > 2 || input.read(expected, sizeof(expected)) != sizeof(expected) || input.read(view, sizeof(view)) != sizeof(view)) return false;
        Native::WorldActor actor;
        const Native::ActorAdmission result = Native::admitWorldActor(tables, sizeof(tables), fields, sizeof(fields), bank, 0, mode != 0, actor);
        if (uint(result) != disposition || memcmp(tables, expected, sizeof(tables))) {
            error = Common::String::format("Native actor admission/state differs at case%u", i); return false;
        }
        if (result == Native::kActorPresent && (actor.x != view[0] || actor.y != view[1] || actor.tile != view[2] || actor.state != view[3] || actor.capability != view[4] || actor.flags != view[5] || actor.kind != view[6] || actor.large != (view[7] != 0))) {
            error = Common::String::format("Native admitted actor view differs at case%u", i); return false;
        }
        ++dispositions[disposition];
    }
    if (dispositions[0] != 7716 || dispositions[1] != 4238 || dispositions[2] != 394 || input.err() || input.pos() != input.size()) return false;
    report += "native-actors\tcases=12348\tabsent=7716\tpresent=4238\tquest_removed=394\tfull_map_state=1792\tcomplete_ai=0\tcpu_execution=0\n"; return true;
}
}
#endif
