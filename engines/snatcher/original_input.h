/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_ORIGINAL_INPUT_H
#define SNATCHER_ORIGINAL_INPUT_H
#include "startup_presentation.h"
#include "game_service.h"
#include "native_archive.h"
namespace Snatcher {
// Original header anchor bytes survive independently of the selected device.
inline StartupSettings originalInputSettings(const GameSaveRecord& record){
 StartupSettings settings;settings.controlMapping=record.controllerMapping;settings.compatibility.deviceFlag=record.sourcePolicy40;settings.mono=record.sourcePolicy80;settings.compatibility.anchorX=record.pointerAnchorX;settings.compatibility.anchorY=record.pointerAnchorY;return settings;
}
inline void captureOriginalInputSettings(const StartupSettings& settings,GameSaveRecord& record){
 record.controllerMapping=uint8_t(settings.controlMapping);record.sourcePolicy40=settings.compatibility.deviceFlag;record.sourcePolicy80=settings.mono;record.pointerAnchorX=settings.compatibility.anchorX;record.pointerAnchorY=settings.compatibility.anchorY;
}
// Compatibility fields never select a hardware input path or gate PC resume.
inline bool originalInputResumeSupported(const StartupSettings& settings,std::string& error){
 error.clear();if(settings.controlMapping>=6){error="Original button mapping outside six native alternatives";return false;}return true;
}
inline bool neutralLegacyInputHeader(const LegacyInputHeader& h){return !h.deviceFlag&&!h.anchorX&&!h.anchorY;}
// Input1..9 could only be written with neutral headers (Phase19 save guard).
// Input10 preserves opaque bytes; none are interpreted as aim/calibration.
inline bool archiveLegacyInputHeader(NativeArchive& archive,LegacyInputHeader& header,unsigned version){
 if(version<1||version>10){archive.ok=false;return false;}
 LegacyInputHeader staged=header;
 if(version<10){if(archive.reading)staged=LegacyInputHeader();else if(!neutralLegacyInputHeader(staged))archive.ok=false;}
 else{archive.flag(staged.deviceFlag);archive.field(staged.anchorX);archive.field(staged.anchorY);}
 if(archive.ok&&archive.reading)header=staged;return archive.ok;
}
}
#endif
