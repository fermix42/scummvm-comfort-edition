/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_STARTUP_PRESENTATION_H
#define SNATCHER_STARTUP_PRESENTATION_H
#include "scene_runtime.h"
#include "mouse_input.h"
namespace Snatcher {
// These historical bytes are opaque compatibility data, never a device mode.
struct LegacyInputHeader {bool deviceFlag;uint8_t anchorX,anchorY;LegacyInputHeader():deviceFlag(false),anchorX(0),anchorY(0){}};
struct StartupSettings {
 unsigned controlMapping;bool mono;LegacyInputHeader compatibility;
 StartupSettings():controlMapping(0),mono(false){}
};
enum StartupOptionsAction {kStartupOptionsNone,kStartupOptionsExit};
struct StartupOptionsUpdate {
 StartupOptionsAction action;std::vector<uint16_t> fmCommands,pcmCommands;
 StartupOptionsUpdate():action(kStartupOptionsNone){}
};
// PC options retain the original button mapping and sound owners.
// Input bits are the game's pressed
// mask (UDLR BCAS), not host keys. Settings are known only after fresh setup or
// independently verified save restoration. No original routine is executed.
class StartupOptions {
public:
 StartupOptions();
 void establishFresh();
 bool restoreVerified(const StartupSettings& settings,std::string& error);
 bool settings(StartupSettings& out,std::string& error)const;
 bool select(unsigned row,std::string& error);
 bool mouseActionReady(uint8_t mask)const{return _known&&(!mask||mask!=_previous);}
 unsigned row()const{return _row;}
 bool poll(uint8_t pressed,StartupOptionsUpdate& update,std::string& error);
 bool applyActorSelection(SceneRuntime& scene,std::string& error)const;
private:
 friend class EngineStateCodec;StartupSettings _settings;bool _known;unsigned _row;uint8_t _previous;
};
struct StartupLicenseInput {
 bool startPressed,discReady,soundDriverReady,moduleReady,alternateVideo;uint64_t sourceFrame;
 StartupLicenseInput():startPressed(false),discReady(false),soundDriverReady(false),moduleReady(false),alternateVideo(false),sourceFrame(0){}
};
// Game-owned Main preparation and Konami/legal presentation. Owns original
// compressed patterns/maps, palette pages and the logo sprite; no BIOS assets.
class StartupLicense {
public:
 StartupLicense();
 bool start(const std::vector<uint8_t>& mainIp,bool previouslyPresented,std::string& error);
 bool tick(const StartupLicenseInput& input,std::string& error);
 bool render(SceneColorEncoding encoding,SceneImage& image,std::string& error);
 bool finished()const{return _finished;}
 unsigned phase()const{return _phase;}
 std::vector<uint16_t> takeMusicCommands(){std::vector<uint16_t> out;out.swap(_music);return out;}
private:
 friend class EngineStateCodec;
 std::vector<uint8_t> _main;SceneRenderer _renderer;SceneLayout _layout;std::vector<uint16_t> _colors,_visible,_music;
 unsigned _prepare,_phase,_counter,_gradient,_offset,_spriteTile,_visibleSpriteTile;int _scrollX,_scrollY,_spriteX,_visibleScrollX,_visibleScrollY,_visibleSpriteX;bool _visibleSprite;bool _started,_finished,_previouslyPresented,_display,_sprite;
 bool asset(size_t source,uint16_t destination,std::string& error);
 bool pages(unsigned a,unsigned b,unsigned c,unsigned d,std::string& error);
 bool cells(unsigned allocation,unsigned first,unsigned count,uint16_t attributes,std::string& error);
 bool finish(std::string& error);
};
enum StartupCinematicAction {kStartupCinematicNone,kStartupCinematicTrack,kStartupCinematicScene,kStartupCinematicPalette,kStartupCinematicVisual,kStartupCinematicDisplayFlag,kStartupCinematicActorFlags,kStartupCinematicComplete,kStartupCinematicSkip};
struct StartupCinematicEvent {
 StartupCinematicAction action;unsigned value;uint16_t resource;unsigned auxiliary;size_t paletteOffset;uint64_t id;
 StartupCinematicEvent():action(kStartupCinematicNone),value(0),resource(0),auxiliary(0),paletteOffset(0),id(0){}
};
// Original resource tables carry packed relative CD times. Events require an
// acknowledgement by the native graphics/audio owner before advancing. The
// clock must belong to the acknowledged track and originate from consumed CD
// samples; host elapsed time and queued audio are not completion evidence.
class StartupCinematic {
public:
 StartupCinematic();
 bool start(uint16_t resource,const std::vector<uint8_t>& data,std::string& error);
 bool poll(bool clockKnown,unsigned track,uint32_t relativeClock,bool startPressed,StartupCinematicEvent& event,std::string& error);
 bool complete(const StartupCinematicEvent& event,std::string& error);
 bool finished()const{return _finished;}
 unsigned index()const{return _index;}
private:
 friend class EngineStateCodec;
 struct Entry {uint32_t clock;StartupCinematicEvent event;};
 std::vector<Entry> _entries;unsigned _index,_track;uint64_t _nextId;bool _started,_trackReady,_finished;StartupCinematicEvent _pending;
};
struct StartupSavedSlot {
 bool known,present,metadataKnown;uint8_t counter,hours,minutes,progression;
 StartupSavedSlot():known(false),present(false),metadataKnown(false),counter(0),hours(0),minutes(0),progression(0){}
};
struct StartupStorage {
 bool known,available;std::array<StartupSavedSlot,4> slots;
 StartupStorage():known(false),available(false){}
};
enum StartupSlotAction {kStartupSlotNone,kStartupSlotRead,kStartupSlotCancel};
class StartupSaveSelector {
public:
 StartupSaveSelector():_known(false),_selected(0),_previous(0){}
 bool start(const StartupStorage& storage,std::string& error);
 bool poll(uint8_t pressed,StartupSlotAction& action,std::string& error);
 bool select(unsigned slot,std::string& error);
 bool mouseActionReady(uint8_t mask)const{return _known&&(!mask||mask!=_previous);}
 unsigned selectedSlot()const{return _selected+1;}
private:
 friend class EngineStateCodec;StartupStorage _storage;bool _known;unsigned _selected;uint8_t _previous;
};
bool validateStartupStorage(const StartupStorage& storage,std::string& error);
bool decorateStartupSlotMap(std::vector<uint8_t>& map,const StartupStorage& storage,std::string& error);
struct StartupFrame {
 uint8_t pressed;bool cdKnown,soundDriverReady;unsigned cdTrack;uint32_t cdClock;uint64_t sourceFrame;
 StartupFrame():pressed(0),cdKnown(false),soundDriverReady(false),cdTrack(0),cdClock(0),sourceFrame(0){}
};
enum StartupNativePhase {kStartupNativeDormant,kStartupNativeLicense,kStartupNativeTitleMusic,kStartupNativeTitleInstall,kStartupNativeTitleDelay,kStartupNativeTitleFlashDelay,kStartupNativeTitleReveal,kStartupNativeTitleReady,kStartupNativeOptionsPaint,kStartupNativeOptionsPoll,kStartupNativeOptionsFade,kStartupNativeCinematicFirst,kStartupNativeCinematicSecond,kStartupNativeSkipFade,kStartupNativeComplete,kStartupNativeLoadPaint,kStartupNativeLoadPoll,kStartupNativeLoadPending,kStartupNativeLoadClose,kStartupNativeLoadCancelDelay,kStartupNativeLoaded};
// Native game presenter facade; the engine owns real CD/FM/PCM submission and
// SceneRuntime's actual transfers. No presentation phase is a scenario entry.
class StartupPresenter {
public:
 StartupPresenter();
 bool start(const std::vector<uint8_t>& mainIp,const std::vector<uint8_t>& title,const std::vector<uint8_t>& first,const std::vector<uint8_t>& second,const std::vector<uint8_t>& subcode,bool savedGamesAvailable,std::string& error);
 // Original semantic slot records are read before Options; actual scenario
 // reconstruction waits until loaded() after the source close/options/fade.
 bool start(const std::vector<uint8_t>& mainIp,const std::vector<uint8_t>& title,const std::vector<uint8_t>& first,const std::vector<uint8_t>& second,const std::vector<uint8_t>& subcode,const StartupStorage& storage,bool returnedFromGame,const StartupSettings* retainedSettings,std::string& error);
 unsigned pendingLoadSlot()const{return _phase==kStartupNativeLoadPending?_selector.selectedSlot():0;}
 bool acknowledgeLoad(unsigned slot,const StartupSettings& settings,std::string& error);
 bool loaded()const{return _phase==kStartupNativeLoaded;}
 unsigned loadedSlot()const{return _loadedSlot;}
 // Pointer intent survives setup/coordinator frames until this intro owns a
 // normal Start poll. It never carries into another presentation owner.
 void queueMouseSkip(){if(_phase==kStartupNativeLicense||_phase==kStartupNativeCinematicFirst||_phase==kStartupNativeCinematicSecond)_mouseSkipRequested=true;}
 void cancelMouseSkip(){_mouseSkipRequested=false;}
 bool mouseActionReady(uint8_t mask)const;
 std::vector<MouseTarget> mouseTargets(const SceneRuntime& scene,std::string& error)const;
 bool mouseSelect(unsigned target,SceneRuntime& scene,std::string& error);
 unsigned optionRow()const{return _options.row();}
 bool continueAvailable()const{return _savedGames;}
 const StartupStorage& storage()const{return _storage;}
 bool continueSelected()const{return _continueSelected;}
 unsigned selectedSlot()const{return _selector.selectedSlot();}
 bool tick(const StartupFrame& frame,SceneRuntime& scene,std::string& error);
 bool render(SceneRuntime& scene,SceneColorEncoding encoding,SceneImage& image,std::string& error);
 StartupNativePhase phase()const{return _phase;}
 bool finished()const{return _phase==kStartupNativeComplete;}
 bool soundDriverSuspended()const{return _soundDriverSuspended;}
 bool settings(StartupSettings& out,std::string& error)const{return _options.settings(out,error);}
 std::vector<unsigned> takeTracks(){std::vector<unsigned> out;out.swap(_tracks);return out;}
 std::vector<uint16_t> takeFm(){std::vector<uint16_t> out;out.swap(_fm);return out;}
 std::vector<uint16_t> takePcm(){std::vector<uint16_t> out;out.swap(_pcm);return out;}
private:
 friend class EngineStateCodec;
 bool _mouseSkipRequested=false;
 StartupLicense _license;StartupOptions _options;StartupSaveSelector _selector;StartupStorage _storage;StartupCinematic _cinematic;OriginalFont _pcOptionsFont;StartupNativePhase _phase;
 std::vector<uint8_t> _main,_title,_first,_second,_subcode;std::vector<unsigned> _tracks;std::vector<uint16_t> _fm,_pcm;unsigned _delay,_paint;bool _savedGames,_trackPosted,_soundDriverSuspended,_continueSelected,_cancelLoad;unsigned _loadedSlot;uint8_t _previousTitle,_previousModule;
 bool renderPcOptions(SceneColorEncoding encoding,SceneImage& image,std::string& error)const;
 bool selectTitle(SceneRuntime& scene,std::string& error);
 bool applySlotSelection(SceneRuntime& scene,bool visible,std::string& error);
 bool palette(const std::vector<uint8_t>& bytes,size_t offset,SceneRuntime& scene,std::string& error);
 bool install(SceneRuntime& scene,uint16_t resource,const std::vector<uint8_t>& bytes,unsigned index,std::string& error);
};
enum StartupPaintKind {kStartupPaintOptionsOpen,kStartupPaintLoadOpen,kStartupPaintClose};
// Decodes original compressed/raw copy tasks through the shared resource
// format reader. Cache reuse and queue completion retain their real owners.
bool readStartupPaint(const std::vector<uint8_t>& resource,StartupPaintKind kind,unsigned step,std::vector<SceneGraphicsTask>& tasks,std::string& error);
unsigned startupPaintSteps(StartupPaintKind kind);
}
#endif
