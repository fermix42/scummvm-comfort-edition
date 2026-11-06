/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_VALIDATION_H
#define NOX_NATIVE_WORLD_VALIDATION_H

#include "nox/native_world.h"
#include "nox/native_world_open.h"
#include "common/savefile.h"

namespace Nox {
inline bool validateNativeOrdinaryOpen(HdvArchive &archive,Common::SaveFileManager &files,bool cold,Common::String &report,Common::String &error) {
    const char *filename="nox-native-ordinary-open.bin";
    Native::World world(archive);Native::ActorPaintRules paint;Native::WorldOpen command;
    Native::PresentationState presentation;Native::WorldScene scene;byte visibility[187]={};
    if (!paint.load(archive,8) || !command.load(archive,8)) { error="Native ordinary Open rules unavailable";return false; }
    if (cold) {
        Common::InSaveFile *in=files.openForLoading(filename);const bool ok=in && world.load(*in);delete in;
        if (!ok || world.x()!=25 || world.y()!=38 || (*world.currentTables())[1027]!=40) { error="Ordinary Open cold state rejected";return false; }
        // Reconstruct lookup inputs on an inspection clone. Restoring the
        // logical world must not settle the saved door a second time.
        Native::World inspection(world);
        if (!Native::WorldPresentation::run(inspection,paint,visibility,187,presentation,scene)) { error="Ordinary Open cold scene inspection failed";return false; }
        Native::WorldOpenEvent event;
        if (command.run(world,scene,Native::kNorth,event)!=Native::kWorldOpenRefused || event.message!=23 || !event.sound || (*world.currentTables())[1027]!=40) { error="Ordinary Open cold continuation differs";return false; }
        report+="native-ordinary-open\tphase=cold\tactual_open_retained=1\trepeat_refusal=1\tcpu_execution=0\n";return true;
    }
    byte fields[256]={};
    if (!world.initialize(8,7,25,38,fields,256) || !Native::WorldPresentation::run(world,paint,visibility,187,presentation,scene)) { error="Ordinary Open controlled Keep setup failed";return false; }
    Common::Array<byte> notice;
    if (!world.notice(0,notice) || notice.empty()) { error="Authored Keep notice unavailable";return false; }
    Common::OutSaveFile *noticeOut=files.openForSaving("nox-native-notice0.bin",false);if (!noticeOut) return false;
    noticeOut->write(notice.data(),notice.size());noticeOut->finalize();const bool noticeOk=!noticeOut->err();delete noticeOut;
    if (!noticeOk || world.notice(127,notice)) { error="Authored notice export or extent refusal failed";return false; }
    Native::WorldOpenEvent event;
    if ((*world.currentTables())[1026]!=67 || (*world.currentTables())[1027]!=16 ||
        command.run(world,scene,Native::kNorth,event)!=Native::kWorldOpenChanged || event.message!=2 || event.sound ||
        (*world.currentTables())[1026]!=67 || (*world.currentTables())[1027]!=41) { error="Ordinary Open16 action differs from original176";return false; }
    Common::MemoryWriteStreamDynamic before(DisposeAfterUse::YES),after(DisposeAfterUse::YES);
    if (!world.save(before) || command.run(world,scene,Native::kNorth,event)!=Native::kWorldOpenRefused || event.message!=23 || !event.sound ||
        !world.save(after) || before.size()!=after.size() || memcmp(before.getData(),after.getData(),before.size())) { error="Repeated ordinary Open changed state or refusal";return false; }
    if (!Native::WorldPresentation::run(world,paint,visibility,187,presentation,scene) || (*world.currentTables())[1027]!=40) { error="Ordinary Open presentation did not settle41 to40";return false; }
    Common::OutSaveFile *out=files.openForSaving(filename,false);bool saved=out && world.save(*out);
    if (out) { out->finalize();saved=saved&&!out->err(); }delete out;
    if (!saved) { error="Ordinary Open cold checkpoint failed";return false; }
    report+="native-ordinary-open\tphase=warm\tcontrolled_position=25/38\topen16_to41=1\ttile67_retained=1\trepeat_refusal=1\tpresentation40=1\tcpu_execution=0\n";return true;
}
inline bool validateNativeWorld(HdvArchive &archive, Common::SaveFileManager &files,
		bool cold, Common::String &report, Common::String &error) {
	if (!validateNativeOrdinaryOpen(archive,files,cold,report,error)) return false;
	const char *filename = "nox-native-world-state.bin";
	byte fields[256] = {};
	if (cold) {
		Common::InSaveFile *input = files.openForLoading(filename);
		Native::World restored(archive);
		const bool loaded = input && restored.load(*input); delete input;
		const Common::Array<byte> *tables = restored.currentTables();
		if (!loaded || restored.mapType() != 7 || restored.mapId() != 54 || restored.x() != 38 || restored.y() != 15 || restored.field(24) != 4 ||
			!tables || (*tables)[1058] != 58 || (*tables)[1059] != 41) {
			error = "Cold native world did not retain actual Open operation"; return false;
		}
		report += "native-world\tphase=cold\tmap=54\tnative_open_retained=1\tcpu_execution=0\n";
		return true;
	}
	Native::World refused(archive);
	if (!refused.initialize(7, 54, 38, 15, fields, sizeof(fields))) { error = "Native world profile load failed"; return false; }
	Common::MemoryWriteStreamDynamic before(DisposeAfterUse::YES), after(DisposeAfterUse::YES);
	if (!refused.save(before) || refused.open(38, 14) != Native::kGateLocked || !refused.save(after) || before.size() != after.size() || memcmp(before.getData(), after.getData(), before.size())) {
		error = "Native key refusal changed world state"; return false;
	}
	fields[24] = 4;
	Native::World opened(archive);
	if (!opened.initialize(7, 54, 38, 15, fields, sizeof(fields)) || opened.open(38, 14) != Native::kGateAllowed || opened.field(24) != 4) {
		error = "Native retained-Moat-key Open failed"; return false;
	}
	const Common::Array<byte> *tables = opened.currentTables();
	if (!tables || (*tables)[1058] != 58 || (*tables)[1059] != 41 || opened.x() != 38 || opened.y() != 15) {
		error = "Native Open mutation differs from original operation owner"; return false;
	}
	Common::OutSaveFile *output = files.openForSaving(filename, false);
	bool saved = output && opened.save(*output);
	if (output) { output->finalize(); saved = saved && !output->err(); } delete output;
	if (!saved) { error = "Native world save failed"; return false; }
	Common::MemoryWriteStreamDynamic encoded(DisposeAfterUse::YES);
	if (!opened.save(encoded)) { error = "Native world model encode failed"; return false; }
	Common::MemoryReadStream truncated(encoded.getData(), encoded.size() - 1);
	if (refused.load(truncated)) { error = "Native world accepted truncated candidate"; return false; }
	Common::MemoryWriteStreamDynamic retained(DisposeAfterUse::YES);
	if (!refused.save(retained) || retained.size() != before.size() || memcmp(retained.getData(), before.getData(), before.size())) {
		error = "Rejected native world candidate replaced live state"; return false;
	}
	memset(fields, 0, sizeof(fields));
	Native::World controlled(archive);
	if (!controlled.initialize(7, 78, 10, 8, fields, sizeof(fields)) || controlled.open(9, 8) != Native::kGateAllowed) {
		error = "Native source paired control failed"; return false;
	}
	tables = controlled.currentTables();
	if (!tables || (*tables)[1027] != 254 || (*tables)[1030] != 150 || (*tables)[1031] != 48 ||
		controlled.open(9, 8) != Native::kGateAllowed || (*controlled.currentTables())[1027] != 50 ||
		(*controlled.currentTables())[1030] != 151 || (*controlled.currentTables())[1031] != 49) {
		error = "Native source control reversal mismatched retained records"; return false;
	}
	fields[60] = 4;
	Native::World admitted(archive);
	if (!admitted.initialize(7, 54, 36, 11, fields, sizeof(fields)) || admitted.enterPortal() != Native::kGateAllowed ||
		admitted.mapId() != 57 || admitted.mapType() != 7 || admitted.x() != 38 || admitted.y() != 44 || admitted.field(60) != 4) {
		error = "Native Order portal did not load actual destination"; return false;
	}
	Common::MemoryWriteStreamDynamic portalState(DisposeAfterUse::YES);
	Native::World portalRestored(archive);
	if (!admitted.save(portalState)) { error = "Native multi-map save failed"; return false; }
	Common::MemoryReadStream portalInput(portalState.getData(), portalState.size());
	if (!portalRestored.load(portalInput) || portalRestored.mapId() != 57 || portalRestored.x() != 38 || portalRestored.y() != 44) {
		error = "Native portal destination/multi-map state did not restore"; return false;
	}
	report += "native-world\tphase=warm\tkey_refusal=1\tactual_open=1\tcontrol_reversal=1\tportal=1\tmulti_map_restore=1\tatomic_rejection=1\tcpu_execution=0\n";
	return true;
}
} // namespace Nox
#endif
