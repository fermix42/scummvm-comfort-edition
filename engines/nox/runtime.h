/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_RUNTIME_H
#define NOX_RUNTIME_H
#include "nox/resources.h"
#include "nox/speaker_audio.h"
#include "nox/mockingboard.h"
#include "common/queue.h"
#include "graphics/surface.h"
#include "common/serializer.h"
#include "nox/enhancements.h"

namespace Nox {
// Temporary M1 execution bridge adapted from banked_runtime.c/live_port.c.
// Per-instance state, no Windows calls, external executable, ROM or game code.
// Generic CPU execution remains a required dependency until M4.
class Runtime {
public:
	Runtime();
	bool initialize(HdvArchive &resources);
	bool advance(uint32 clocks, bool stopAtBoundary = false, bool worldBoundaryOnly = false);
	void key(byte value) { if (keys.size() < 256) keys.push(value | 128); }
	void render(Graphics::Surface &surface, uint32 hostMillis);
	const Common::String &error() const { return failure; }
	bool atWorldInput() const { return pc == 0x71dc && !mainram[0x6052] && !alt && !read_aux && !write_aux; }
	bool inWorldScene() const { return !mainram[0x6052] && (positionX() || positionY()); }
	bool atEndingInput() const;
	bool canSnapshot() const { return (worldWaiting||atEndingInput()) && keys.empty() && !key_latch && !_conversationPending && !_conversationQuery; }
	bool saveState(Common::WriteStream &stream);
	bool loadState(Common::SeekableReadStream &stream);
	bool validateOwnerContracts(HdvArchive &resources, Common::String &report);
	enum InterfaceRequest { kNoInterfaceRequest, kSaveInterfaceRequest, kLoadInterfaceRequest };
	InterfaceRequest takeInterfaceRequest() { InterfaceRequest value=interfaceRequest;interfaceRequest=kNoInterfaceRequest;return value; }
	Common::String diskFingerprint() const {
		Common::MemoryReadStream image(disk.data(),disk.size());return Common::computeStreamMD5AsString(image);
	}
	void enableCallerObservation(bool enabled) { _observeCallers=enabled; }
	Common::String callerObservation() const { return Common::String::format("legacy_save_entries=%u\tother_9600_entries=%u\tother_fingerprint=%u\tother_return=%04X\tsave_callers=%s",_saveEntries,_otherEntries,_otherFingerprint,_otherReturn,_saveCallerTrace.c_str()); }
	uint16 address() const { return pc; }
	Common::String diagnostic() const {
		return Common::String::format("alt=%d read=%d write=%d bank=%d mode=%u sp=%u ret=%04X",alt,read_aux,write_aux,bank,mainram[0x6052],sp,
			(alt?auxram:mainram)[0x100+byte(sp+1)]|uint16((alt?auxram:mainram)[0x100+byte(sp+2)])<<8);
	}
	byte positionX() const { return mainram[0x6cec]; }
	byte positionY() const { return mainram[0x6ced]; }
	uint64 nativeMovementCalls() const { return nativeCalls; }
	uint64 cpuClocks() const { return cpu_cycles; }
	Common::Array<int16> &audio() { return samples; }
	Enhancements &enhancements() { return _enhancements; }
	const Enhancements &enhancements() const { return _enhancements; }
	const Common::String &activeMapResource() const { return _activeMapResource; }
	void suppressEnhancementCaptureForFixture(bool suppressed) { _captureEnhancements = !suppressed; }
	// Explicitly controlled ABI setup adapted from the standalone NPC test.
	bool campaignFixtureNoon();
    bool campaignFixtureFloor(byte map,byte column,byte row);
    bool campaignFixtureShrineApproach();
    byte campaignTransportFlags() const { return mainram[0x43a]; }
    uint campaignPortalCount() const {const byte record[]={23,20,138,56,0,8,0,81,168,10,0,1};uint count=0;for(uint at=0x5700;at+12<=0x5800;++at)if(!memcmp(auxram+at,record,12))++count;return count;}
	bool campaignFixtureSevenPages();
	bool campaignFixtureActor(byte map, byte actor, byte &direction,byte preferredDirection=0);
	uint campaignPartyHealth() const { return auxram[0x1602]|uint(auxram[0x1603])<<8; }
	byte campaignResearchFlags() const { return mainram[0x43d]; }
	byte campaignMapId() const { return mainram[0x2af9]; }
	Common::String campaignActorDiagnostic(byte actor) const { return Common::String::format("map=%u type=%u actor=%u xy=%u,%u data=%02X,%02X,%02X,%02X,%02X,%02X clock=%02X:%02X",mainram[0x2af9],mainram[0x267d],actor,mainram[0xbf00+actor],mainram[0xbf01+actor],mainram[0xbf02+actor],mainram[0xbf03+actor],mainram[0xbf04+actor],mainram[0xbf05+actor],mainram[0xbf06+actor],mainram[0xbf07+actor],mainram[0x3878],mainram[0x3879]); }
	bool campaignFixtureShelf();
	uint campaignLogCount(byte item=150);
	byte campaignPageDirection(byte mask) const { return (mask==4||mask==16||mask==64)?8:mask==8?10:mask==128?21:11; }
	byte campaignPageFlags() const { return mainram[0x43e]; }
	uint campaignPagePickAttempts() const { return _pagePickAttempts; }
	Common::String campaignMachineDiagnostic() const { return Common::String::format("a=%02X x=%02X y=%02X p=%02X sp=%02X delay=%02X polls=%llu clocks=%llu %s",a,x,y,p,sp,mainram[0x29fe],(unsigned long long)_emptyKeyboardPolls,(unsigned long long)cpu_cycles,diagnostic().c_str()); }
	bool campaignFixturePage(byte map,byte column,byte row,byte mask);
	bool campaignFixturePageTools(bool missing);
	bool campaignFixturePageSearchSeed(uint32 seed);
	bool campaignFixtureSearchObject(byte map,byte type,byte column,byte row,byte direction,byte item,uint16 field,byte mask);
    bool campaignFixtureCultSwitchAdjacent(byte column,byte row,byte direction);
	uint campaignKeyDoorState(byte column,byte row,uint16 field,byte mask,byte closedTile=59) const;
	bool campaignFixtureKeyDoor(byte map,byte type,byte column,byte row,byte direction,uint16 field,byte mask,byte closedTile=59);
	bool campaignFixtureAmbientCheckpoint(Common::String &evidence);
    bool campaignFixtureDratheAmbientCheckpoint(Common::String &evidence);
	bool campaignFixtureRustyGuardDefeated();
	bool campaignFixtureClock(byte hour,byte minute);
	bool campaignFixtureWorldEntrance(byte entranceMap,byte entranceType,byte direction,byte entranceColumn=0,byte entranceRow=0);
	bool campaignSafeFloor() const;
	bool campaignFixtureQuestFinalBlow(byte worldMap,byte actor);
    bool campaignCultistsFullRoster() const {return campaignEnemyCount()==2&&READ_LE_UINT16(auxram+0x1902)==267&&READ_LE_UINT16(auxram+0x1922)==267&&auxram[0x1910]==169&&auxram[0x1930]==169;}
    bool campaignCultistsSource() const;
    bool campaignCultistsApproach() const;
    bool campaignTempleMonksSource() const;
    bool campaignTempleMonksApproach() const;
    bool campaignTempleMonksDead() const {return atWorldInput()&&campaignMapId()==99&&mainram[0x267d]==2&&mainram[0xbd0a]==137&&mainram[0xbd0b]==255;}
	bool campaignPatchPrivateResource(const HdvArchive &archive,const Common::String &name,uint offset,const byte *data,uint count);
	bool campaignWorldActorAbsent(byte actor) const {return !(actor&7)&&mainram[0xbf02+actor]==0;}
	byte campaignLogFlags() const { return mainram[0x441]; }
	bool dialogueFixtureExit(byte id, byte type);
	bool dialogueFixtureAdjacentActor(byte actor);
	bool campaignFixtureMage();
    bool campaignFixtureFinalApproach(bool small);
    bool campaignFixtureStaff();
    bool campaignRetainedStaffValid();
    bool campaignFixtureWoundedShadow(bool wound,bool melee=false);
    bool vortexDirectionPending() const { return _vortexDirectionPending; }
    byte battleDirectionKey() const;
    byte battleAlignKey() const {const int partyX=mainram[0xbd00+mainram[0x620]],partyY=mainram[0xbd01+mainram[0x620]],enemyX=mainram[0xbf00],enemyY=mainram[0xbf01];if(partyX==enemyX||partyY==enemyY)return 0;return ABS(enemyX-partyX)<ABS(enemyY-partyY)?(enemyX>partyX?21:8):(enemyY>partyY?10:11);}


    void enableBattleValidation(bool enabled=true) { _battleValidation=enabled; }
    bool battleInputReady() const;
    bool inBattle() const { return mainram[0x267d]==255; }
    bool battlePlayerTurn() const { return mainram[0x33f9]==0; }
    bool battleTargeting() const { return _battleTargeting; }
    bool battleAdmissionSeen() const { return _battleAdmission; }
    void enableBattleControlObservation() { _battleControlObservation=true; }
    const Common::String &battleControlObservation() const { return _battleControlTrace; }
    void enableEncounterReference() { _encounterReference=true; }
    void enableFormationReference(bool after=false) { _formationReference=true;_formationReferenceAfter=after; }
    bool atFormationReference() const {
        if (_formationReferenceAfter) return _formationReference && pc==0x9b1f && peek(pc)==0xa9 && peek(pc+1)==0 && peek(pc+2)==0x8d && peek(pc+3)==0x1e && peek(pc+4)==6;
        return _formationReference && pc==0x98b5 &&
            peek(pc)==0xa2 && peek(pc+1)==1 && peek(pc+2)==0xa0 && peek(pc+3)==0;
    }
    Common::String formationReferenceDiagnostic() const {
        return Common::String::format("formation_keys=%u formation_latch=%u reference_not_player_save=1",keys.size(),uint(key_latch));
    }
    bool atEncounterReference() const {
        return _encounterReference && pc==0xe43e && !read_aux && !write_aux && mainram[0x267d]==8 &&
            peek(pc)==0xad && peek(pc+1)==0x7e && peek(pc+2)==0x36 && peek(pc+3)==0xc9 && peek(pc+4)==4;
    }
    bool controlledRatPresent() const { return mainram[0x2af9]==7&&mainram[0x267d]==8&&mainram[0xbd04]==26&&mainram[0xbd05]==35&&mainram[0xbd06]==164&&mainram[0xbd07]!=255; }
    bool campaignFixtureDevilsCave();
    bool campaignFixtureDevilsApproach();
    bool campaignFixtureEssenceApproach();
    bool campaignFixtureHairnetApproach();
    bool campaignFixtureGuildPrerequisites();
    bool campaignFixtureTombTime(bool valid,bool removeRescue=false);
    bool campaignFixtureTombApproach(bool unlocked);
    bool campaignFixtureWaterObjectApproach();
    bool campaignFixtureGuardianRecovery();
    bool campaignFixtureRingApproach();
    bool campaignRingUnlocked() const {for(uint at=0;at<256;at+=4)if(mainram[0xbe00+at]==9&&mainram[0xbe01+at]==10){const uint state=mainram[0xbe03+at];return state>=33&&state<=41;}return false;}
    bool campaignRingFound() const {return (mainram[0x43a]&128)!=0;}
    bool campaignFixtureRingBattleApproach();
    bool campaignRingDefenseValid();
    bool campaignFixtureDratheFinalBlow();
    bool campaignFixtureCultistFinalBlow();
    uint campaignCultistTargetHealth() const {for(uint i=0;i<mainram[0x2efb]&&i<16;++i)if(mainram[0xbf02+i*8]==170)return auxram[0x1902+i*32]|uint(auxram[0x1903+i*32])<<8;return 0xffff;}
    bool campaignCultistDefeated() const {return (mainram[0x438]&64)!=0;}
    bool campaignJournalTranslated() const {return (mainram[0x43b]&2)!=0;}
    uint campaignFlagByte(uint field) const {return field>=0x400&&field<0x448?mainram[field]:0x100;}
    uint campaignWorldObjectState(byte column,byte row) const {for(uint at=0;at<256;at+=4)if(mainram[0xbe00+at]==column&&mainram[0xbe01+at]==row)return mainram[0xbe02+at]|uint(mainram[0xbe03+at])<<8;return 0xffffffffu;}
    bool campaignFixtureShadowCheckpointApproach(const Common::String &sectorA9,const Common::String &sectorB9,Common::String &why);
    Common::String campaignConversationDiagnostic() const {return Common::String::format("pending=%u query=%u return=%04X expectedSP=%02X worldWaiting=%u",_conversationPending,_conversationQuery,_conversationReturn,_conversationSP,worldWaiting);}
    bool campaignFixtureDarkKeyGate();
    bool campaignDarkKeyDoorClosed() const {for(uint at=0;at<256;at+=4)if(mainram[0xbe00+at]==34&&mainram[0xbe01+at]==8)return mainram[0xbe02+at]==18&&mainram[0xbe03+at]==18;return false;}
    bool campaignDarkKeyFlag() const {return (mainram[0x40c]&8)!=0;}
    uint campaignEnemyCount() const {return mainram[0x2efb];}
    byte campaignBattlePrimarySprite() const {return mainram[0xbf02];}
    bool campaignEnemyInvulnerable() const {return (auxram[0x1900]&128)!=0;}
    uint campaignEnemyMana() const {return auxram[0x1904];}
    uint campaignBattleSpell() const {return mainram[0x32fd];}
    bool campaignAtDefeatInput() const {if(pc!=0x1324||!alt||keys.size()||key_latch||campaignPartyHealth()||!inBattle())return false;return (auxram[0x100+byte(sp+1)]|uint(auxram[0x100+byte(sp+2)])<<8)==0xe6f2&&peek(0xe6f0)==0x20&&peek(0xe6f1)==0x24&&peek(0xe6f2)==0x13;}
    bool campaignSourceShadowPresent(bool finalPlane = false);
    bool campaignFixtureShadowAttackSeed(uint32 seed);
    bool campaignFixtureRingDefenseInventory();
    int campaignRingDefenseInventoryRow() const {for(uint row=0;row<=mainram[0x2d1]&&row<6;++row){uint at=mainram[0x2b8+4*row]|uint(mainram[0x2b9+4*row])<<8;if(at>=0xba00&&at<=0xbffa&&(mainram[at]&15)==1&&mainram[at+1]==118)return row;}return -1;}
    bool campaignRingDefenseInventoryCacheReady() const {return mainram[0x9a64]==1&&mainram[0x9a65]==118;}
    void enableRingEffectObservation() {_ringEffectValidation=true;_ringDeathSpells=0;_ringProtectedSpells=0;_ringSurvivedSpellHP=0;_ringHealthTrace.clear();_battleValidation=true;}
    uint campaignRingDeathSpells() const {return _ringDeathSpells;}
    uint campaignRingProtectedSpells() const {return _ringProtectedSpells;}
    uint campaignRingSurvivedSpellHP() const {return _ringSurvivedSpellHP;}
    const Common::String &campaignRingHealthTrace() const {return _ringHealthTrace;}
    bool campaignConversationInputReady() const;
    bool campaignFixtureLichLore(byte book);
    bool campaignFixtureTempleOutside();
    bool campaignFixtureOmitFireJewel();
    bool campaignMargraveKeyFlag() const {return (mainram[0x432]&16)!=0;}
    bool campaignChildrenRescued() const {return (mainram[0x435]&2)!=0;}
    bool campaignTombUnlocked() const {return (mainram[0x42a]&1)!=0;}
    bool campaignTombReconciled() const {return (mainram[0x435]&8)!=0;}
    bool campaignScrollFound() const {return (mainram[0x41c]&1)!=0;}
    bool campaignBookInputReady() const;
    Common::String campaignBookText() const {Common::String text;for(uint i=0xb00;i<0x2000&&auxram[i];++i)text+=char(auxram[i]&127);return text;}
    bool campaignLichLorePresent() const {Common::String text=campaignBookText();text.toLowercase();return text.size()>=32&&text.contains("lich");}
    bool campaignScrollCluePresent() const {Common::String text=campaignBookText();text.toLowercase();return text.contains("sword")&&text.contains("head east")&&text.contains("toad")&&text.contains("smooth as stone");}
    bool campaignWaterCondition() const {return (mainram[0x40c]&4)!=0;}
    uint campaignTombDoor() const {for(uint at=0;at<256;at+=8)if(mainram[0xbe00+at]==10&&mainram[0xbe01+at]==10)return mainram[0xbe02+at]|uint(mainram[0xbe03+at])<<8;return 0;}
    bool campaignTombDoorOpen() const {uint v=campaignTombDoor();return (v&255)==58&&(v>>8)>=33&&(v>>8)<=41;}
    int campaignOwnedMiscRow(byte item) const {for(uint row=0;row<=mainram[0x2d1]&&row<6;++row){const uint at=mainram[0x2b8+4*row]|uint(mainram[0x2b9+4*row])<<8;if(at>=0xba00&&at<=0xbffa&&(mainram[at]&15)==2&&mainram[at+1]==item)return row;}return -1;}
    bool campaignFixtureGuardianApproach(uint guardian);
    uint campaignInvulnerability() const {return mainram[0x60d4];}
    bool campaignVortexTargetSeen() const {return _vortexTargetTrace.contains("completed-list=3");}
    byte campaignHairnetFlags() const {return mainram[0x43f];}
    bool campaignFooleryKeyFlag() const {return (mainram[0x41a]&32)!=0;}
    uint campaignHairnetDoor() const {for(uint at=0;at<256;at+=8)if(mainram[0xbe00+at]==12&&mainram[0xbe01+at]==37)return mainram[0xbe02+at]|uint(mainram[0xbe03+at])<<8;return 0;}
    bool campaignHairnetDoorOpen() const {const uint value=campaignHairnetDoor();return (value&255)==58&&(value>>8)>=33&&(value>>8)<=41;}
    uint campaignDevilsDoor() const {for(uint at=0;at<256;at+=8)if(mainram[0xbe00+at]==69&&mainram[0xbe01+at]==90)return mainram[0xbe02+at]|uint(mainram[0xbe03+at])<<8;return 0xffffffffU;}
    bool campaignDevilsDoorOpen() const {const uint door=campaignDevilsDoor();return (door&255)==64&&(door>>8)>=33&&(door>>8)<=41;}
    byte campaignDevilsKeyFlags() const {return (mainram[0x40f]&16?1:0)|(mainram[0x410]&64?2:0);}
    uint campaignFinalEnemyHealth() const {return READ_LE_UINT16(auxram+0x1902);}
    uint vortexEffects() const { return _vortexEffects; }
    uint vortexDamaged() const { uint n=0;for(uint i=0;i<16;++i)if(_vortexEnemyHP[i]&&(auxram[0x1902+i*32]|uint(auxram[0x1903+i*32])<<8)<_vortexEnemyHP[i])++n;return n; }
    byte battleTargetKey() const {
        uint best=65535,tx=mainram[0x644],ty=mainram[0x645];const int px=mainram[0xbd00+mainram[0x620]],py=mainram[0xbd01+mainram[0x620]];
        for(uint i=0;i<128;i+=8)if(mainram[0xbf02+i]&&mainram[0xbf03+i]!=255&&mainram[0xbf07+i]<2){uint d=ABS(int(mainram[0xbf00+i])-px)+ABS(int(mainram[0xbf01+i])-py);if(d<best){best=d;tx=mainram[0xbf00+i];ty=mainram[0xbf01+i];}}
        return mainram[0x644]!=tx?(mainram[0x644]<tx?21:8):mainram[0x645]!=ty?(mainram[0x645]<ty?10:11):13;
    }
    bool campaignDiagnosticSnapshot(Common::WriteStream &out);
    bool campaignBattleReference(Common::WriteStream &out) const;
    void campaignDiagnosticMemory(Common::WriteStream &out) const { out.write(mainram,sizeof(mainram));out.write(auxram,sizeof(auxram)); }
    void campaignDiagnosticBanks(Common::WriteStream &out) const { out.write(&lc[0][0][0],sizeof(lc));out.write(&high[0][0],sizeof(high)); }
    Common::String battleDiagnostic() const { Common::String text=Common::String::format("type=%u map=%u side=%u actor=%u spell=%u MP=%u HP=%u effects=%u damaged=%u target=%u,%u targeting=%d enemies=%u",mainram[0x267d],mainram[0x2af9],mainram[0x33f9],mainram[0x620],mainram[0x32fd],campaignMana(),campaignPartyHealth(),_vortexEffects,vortexDamaged(),mainram[0x644],mainram[0x645],_battleTargeting,mainram[0x2efb]);for(uint i=0;i<mainram[0x2efb]&&i<16;++i)text+=Common::String::format(" enemy%u=%u",i,auxram[0x1902+i*32]|uint(auxram[0x1903+i*32])<<8);text+=Common::String::format(" party=%u,%u enemy=%u,%u direction_pending=%d",mainram[0xbd00+mainram[0x620]],mainram[0xbd01+mainram[0x620]],mainram[0xbf00],mainram[0xbf01],_vortexDirectionPending);text+=Common::String::format(" origin=%u,%u dir=%02X targets=%u enemy_flags=%02X,%02X,%02X",mainram[0x650],mainram[0x651],mainram[0xba7],mainram[0x240],mainram[0xbf03],mainram[0xbf07],auxram[0x1900]);text+=" "+_vortexTargetTrace+" "+_weaponTrace;return text; }

	void enableSpellCastBoundary(bool enabled=true) { _spellCastBoundary=enabled; }
	void enableSpellValidationObservation() { _spellValidationObservation=true;_spellReadyRefusals=0; }
	bool campaignTestInputReady() const { return _spellValidationObservation&&keys.empty()&&!key_latch&&atKeyboardPoll()&&(pc==0xa9a1||pc==0x9966||pc==0x1324||pc==0x1371||(pc==0xd560&&_spellCastBoundary)||atWorldInput()); }
	uint campaignWorldCastRefusals() const { return _worldCastRefusals; }
	uint campaignSpellReadyRefusals() const { return _spellReadyRefusals; }
	byte campaignReadiedSpell(byte slot=1) const { return mainram[0x7043+slot]; }
	byte campaignReadiedIntelligence(byte slot=1) const { return mainram[0x704d+slot]; }
	byte campaignMana() const { return auxram[0x1604]; }
	byte campaignInventoryTab() const { return mainram[0x25a]; }
	byte campaignInventoryRow() const { return mainram[0x2d0]; }
	int campaignOwnedSpellRow() const {
		for(uint row=0;row<=mainram[0x2d1]&&row<6;++row){const uint at=mainram[0x2b8+4*row]|uint(mainram[0x2b9+4*row])<<8;
			if(at>=0xba00&&at<=0xbffa&&(mainram[at]&15)==2&&mainram[at+1]==12)return row;}
		return -1;
	}
    int campaignOwnedStaffRow() const {for(uint row=0;row<=mainram[0x2d1]&&row<6;++row){const uint at=mainram[0x2b8+4*row]|uint(mainram[0x2b9+4*row])<<8;if(at>=0xba00&&at<=0xbffa&&(mainram[at]&15)==0&&mainram[at+1]==71)return row;}return -1;}
    bool campaignStaffEquipped() const {return mainram[0x9a58]==0&&mainram[0x9a59]==71&&mainram[0x9a5a]==3;}
    bool campaignStaffRange() const {return (auxram[0x1615]>>4)==15||(auxram[0x1616]>>4)==15;}
	Common::String campaignSpellDiagnostic() const { return Common::String::format("tab=%u row=%u maxrow=%u kind=%u item=%u spell1=%u int1=%u level=%u INT=%u MP=%u refusals=%u",mainram[0x25a],mainram[0x2d0],mainram[0x2d1],mainram[0x264],mainram[0x265],mainram[0x7044],mainram[0x704e],auxram[0x1601],auxram[0x1612],auxram[0x1604],_spellReadyRefusals); }
	bool campaignActorDirection(byte actor, byte &direction) const {
		const int dx=int(mainram[0xbf00+actor])-int(mainram[0x6cec]),dy=int(mainram[0xbf01+actor])-int(mainram[0x6ced]);
		if(dx==0&&dy==-1)direction=11;else if(dx==0&&dy==1)direction=10;
		else if(dx==-1&&dy==0)direction=8;else if(dx==1&&dy==0)direction=21;else return false;
		return true;
	}
	void enableDialogueValidationPolling() { _dialogueValidationPolling = true; }
	void enableStrictCampaignWords() { _strictCampaignWords = true; _dialogueValidationPolling = true; }
	void enableCampaignSceneObservation() { _campaignSceneObservation=true; }
    bool campaignSceneInputReady() const {
        const byte wait[]={0x20,0x77,0x89,0xad,0,0xc0,0x10,0xf8,0x8d,0x10,0xc0};
        if(!_campaignSceneObservation||!keys.empty()||key_latch||inBattle()||!atKeyboardPoll())return false;
        return (pc==0x1330&&!memcmp(mainram+0x132d,wait,sizeof(wait)))||
               (pc==0x80d2&&!memcmp(mainram+0x80cf,wait,8)); // Original information-panel any-key loop.
    }
	uint64 emptyKeyboardPolls() const { return _emptyKeyboardPolls; }
	byte partySize() const { return mainram[0x70a3]; }
	bool atKeyboardPoll() const {
		return !alt && !read_aux && !write_aux && peek(pc)==0xad && (peek(uint16(pc+1)) | uint(peek(uint16(pc+2)))<<8)==0xc000;
	}
private:
	bool _campaignSceneObservation=false; // Unsaved, source keyboard-boundary observer only.
	bool _battleAdmission=false;
    bool _encounterReference=false; // Read-only unsaved original contact boundary.
    bool _formationReference=false; // Read-only unsaved original placement input.
    bool _formationReferenceAfter=false;
    bool _battleValidation=false,_battleTargeting=false,_vortexDirectionPending=false;
    bool _battleControlObservation=false;Common::String _battleControlTrace;
    bool _ringEffectValidation=false;uint _ringDeathSpells=0,_ringProtectedSpells=0,_ringSurvivedSpellHP=0;Common::String _ringHealthTrace;
    uint _vortexEffects=0;uint16 _vortexEnemyHP[16]={};Common::String _vortexTargetTrace,_weaponTrace;
	bool _spellCastBoundary=false;
	bool _spellValidationObservation=false; // Read-only test observation, never saved.
	uint _spellReadyRefusals=0,_worldCastRefusals=0;
	bool _observeCallers=false; // Test-only observation; never serialized.
	uint32 _saveEntries=0,_otherEntries=0,_otherFingerprint=0;
	uint16 _otherReturn=0;
	Common::String _saveCallerTrace;
	bool _dialogueValidationPolling = false; // Diagnostic polling only, never saved.
	bool _strictCampaignWords = false; // Opt-in test observer, never saved.
	uint _pagePickAttempts=0; // Read-only original DEC421 observation, never saved.
	uint64 _emptyKeyboardPolls = 0;
	Enhancements _enhancements;
	const HdvArchive *_resourceArchive = nullptr; // Immutable engine-owned resource view.
	Common::String _activeMapResource; // Derived from the saved world filename/family.
	bool _captureEnhancements = true; // Suppressed only for explicitly synthetic owner fixtures.
	bool _conversationActive = false, _conversationPending = false, _conversationOpening = false, _conversationQuery = false;
	uint16 _conversationReturn = 0;
	byte _conversationSP = 0, _conversationJoinCount = 0;
	Common::String _conversationInput, _conversationSelected, _conversationResponse, _conversationSegment;
	bool observeEnhancements();
	Common::String currentMapResource() const;
	Common::String observedText(uint16 address, uint max) const;
	enum { C=1, Z=2, I=4, D=8, B=16, U=32, V=64, N=128 };
	byte mainram[65536], auxram[65536], lc[2][2][4096], high[2][8192], rom[0x3000];
	byte a = 0, x = 0, y = 0, p = B|U, sp = 255, key_latch = 0;
	uint16 pc = 0x9bcf, origin = 0;
	int alt = 0, bank = 1, ram = 1, read_aux = 0, write_aux = 0;
	int video_text = 1, video_hires = 0, video_mixed = 0, video_page2 = 0, video_80col = 0, video_80store = 0, video_dhgr = 0;
	uint64 cpu_cycles = 0, total_steps = 0, nativeCalls = 0;
	Common::Array<byte> disk;
	Common::Queue<byte> keys;
	Common::Array<int16> samples;
	SpeakerAudio speaker;
	Mockingboard mockingboard;
	Common::String failure;
	byte font[64][8];
	bool music = true;
	bool worldWaiting = false;
	InterfaceRequest interfaceRequest=kNoInterfaceRequest; // Transient host command, never saved.
	bool syncState(Common::Serializer &serializer);
	bool inputBoundary() const;
	byte rd(uint16 q);
	byte peek(uint16 q) const;
	void wr(uint16 q, byte value);
	void io(uint16 q, bool writing);
	int side(uint16 q, bool writing) const;
	uint16 r16(uint16 q) { uint16 lo = rd(q); return lo | uint16(rd(q+1)) << 8; }
	uint16 z16(byte q) { uint16 lo = rd(q); return lo | uint16(rd(byte(q+1))) << 8; }
	void nz(byte v) { p = (p & ~(N|Z)) | (v & N) | (v ? 0 : Z); }
	void push(byte v) { wr(0x100 + sp--, v); }
	byte pop() { return rd(0x100 + ++sp); }
	void push16(uint16 v) { push(v >> 8); push(v); }
	uint16 pop16() { uint16 lo = pop(); return lo | uint16(pop()) << 8; }
	void cmp(byte l, byte r) { p = (p & ~C) | (l >= r ? C : 0); nz(l-r); }
	void arithmetic(byte value, int subtract);
	uint32 instructionCycles() const;
	int fallbackStep();
	bool step();
	bool nativeMovement();
	void switches(uint16 q);
	static void sample(int16 value, void *context);
	void textRow(Graphics::Surface &surface, int row, uint32 millis);
};
} // namespace Nox
#endif
