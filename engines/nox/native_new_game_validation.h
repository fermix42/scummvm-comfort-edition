/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_NEW_GAME_VALIDATION_H
#define NOX_NATIVE_NEW_GAME_VALIDATION_H
#include "nox/native_new_game.h"
#include "common/memstream.h"
namespace Nox {
inline bool validateNativeNewGame(HdvArchive &archive, Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','N','G','S')) return false;
    const uint32 version=input.readUint32LE();
    if ((version!=1 && version!=2) || input.readUint32LE()!=1) return false;
    byte expected[2510],surface[1792]; if (input.read(expected,sizeof(expected)) != sizeof(expected)) return false;
    if (version==2 && input.read(surface,sizeof(surface))!=sizeof(surface)) return false;
    if (input.err() || input.pos()!=input.size()) return false;
    Native::NewGame game(archive);
    if (!game.create("TRACE",0,0) || !game.ready()) return false;
    Common::MemoryWriteStreamDynamic party(DisposeAfterUse::YES);
    byte inventory[1536],book[20];
    if (!game.party.save(party) || party.size() != 870 || !game.inventory.exportRecords(inventory,sizeof(inventory)) || !game.book.exportState(book,sizeof(book))) return false;
    const byte location[] = {game.world.mapType(),game.world.mapId(),game.world.x(),game.world.y()};
    const byte clock[] = {game.clock.phase,game.clock.hour,game.clock.minute,game.clock.lightTicks,game.clock.lightActive,game.clock.hourlyCounter};
    Common::MemoryWriteStreamDynamic actual(DisposeAfterUse::YES);
    actual.write(game.world.questFields(),80); actual.write(party.getData() + 6,768); actual.write(party.getData() + 774,96);
    actual.write(inventory,1536); actual.write(book,20); actual.write(location,4); actual.write(clock,6);
    for (uint i = 0; i < sizeof(expected); ++i) if (actual.getData()[i] != expected[i]) {
        error = Common::String::format("Native starting data differs at byte%u: native%u original%u",i,actual.getData()[i],expected[i]); return false;
    }
    if (game.create("OTHER",0,0)) return false;
    if (version==2) {
        Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES),again(DisposeAfterUse::YES);
        if (!game.save(saved)) return false;
        Native::NewGame restored(archive); Common::MemoryReadStream cold(saved.getData(),saved.size());
        if (!restored.load(cold) || !restored.save(again) || again.size()!=saved.size() ||
            memcmp(saved.getData(),again.getData(),saved.size())) return false;
        Common::MemoryWriteStreamDynamic world(DisposeAfterUse::YES);
        if (!restored.world.save(world) || world.size()!=269+2*1794 || READ_LE_UINT16(world.getData()+267)!=2 ||
            world.getData()[269]!=6 || world.getData()[270]!=7 ||
            world.getData()[2063]!=0 || world.getData()[2064]!=0 ||
            memcmp(world.getData()+2065,surface,sizeof(surface))) {
            error="Native new-game Surface registry differs from retained original or cold state"; return false;
        }
        report += "native-new-game-surface\tcompared_bytes=1792\tcold_identity=1\tcurrent_shipwreck_preserved=1\tcpu_execution=0\n";
    }
    report += "native-new-game\tcases=1\tcompared_bytes=2510\tactual_original_first_world=1\tsource_templates=1\tsecond_create_refused=1\tinitial_actor_scene=0\tmenu=0\tcpu_execution=0\n"; return true;
}
}
#endif
