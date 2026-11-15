/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_CONVERSATION_H
#define NOX_NATIVE_WORLD_CONVERSATION_H
#include "nox/native_new_game.h"
#include "nox/native_recruitment.h"
#include "nox/native_training.h"
#include "nox/native_world_presentation.h"
#include "nox/native_text_window.h"
#include "nox/native_opening_backdrop.h"
#include "nox/native_temple_service.h"
#include "nox/native_inn_service.h"
#include "nox/native_vendor_transactions.h"
#include "nox/native_merchant_screen.h"
#include "nox/native_inn_membership.h"
#include "nox/native_party_selection.h"
#include "common/memstream.h"
#include "nox/native_sound_events.h"
#include "nox/native_praevoc.h"
namespace Nox { namespace Native {
enum WorldConversationResult { kWorldConversationHandled,kWorldConversationFinished,kWorldConversationMessage,kWorldConversationUnsupported,kWorldConversationTravel };
// Ordinary on-foot directional Talk, plain authored response/input/closing and
// exit handoff. Service callbacks and structured text are explicit boundaries.
// All phases and data references are semantic; no original instructions run.
class WorldConversation {
public:
    WorldConversation() : _stage(0),_type(255),_map(0),_slot(255),_closing(false),_endShown(false),_footerShown(false),_hasDialogue(false),_hasWindow(false),_refused(false),_memberMode(0),_waitingSlot(255),_pickupFee(0),_vendorRole(0),_merchantSale(false),_merchantIndex(0),_trainingMember(255),_worldRefresh(false),_sounds(nullptr),_effectsEnabled(true),_speechCursor(5) {}
    void setSoundEvents(SoundEvents *sounds,bool effectsEnabled=true) {_sounds=sounds;_effectsEnabled=effectsEnabled;}
    bool takeWorldRefresh() {const bool result=_worldRefresh;_worldRefresh=false;return result;}
    bool active() const { return _stage!=0; }
    byte stage() const { return _stage; }
    byte actorSlot() const {return _slot;}
    bool plainDialogue() const {return _hasDialogue;}
    const Common::String &visibleReply() const {return _conversation.reply();}
    bool takeInnDestination(WorldTravelPlan &plan,HgrFrame &frame) {
        if (_inn.phase()!=4) return false;plan=_inn.destination();restore(frame);return true;
    }
    bool matchesWorld(const World &world) const { return !active() || (world.ready() && world.mapType()==_type && world.mapId()==_map); }
    bool choose(const World &world,const HgrFrame &frame) {
        if (active() || !world.ready() || !world.onFoot()) return false;
        WorldConversation next;next._sounds=_sounds;next._effectsEnabled=_effectsEnabled;next._type=world.mapType();next._map=world.mapId();next._underlay=frame;next._stage=1;*this=next;return true;
    }
    WorldConversationResult handle(const Common::Event &event,HdvArchive &archive,NewGame &game,const WorldScene &scene,
            HgrFrame &frame,byte &message,WorldTransportState *transport=nullptr,GameplayRandom *random=nullptr,byte speechCursor=5) {
        _speechCursor=speechCursor;
        if (speechCursor>5 || !active() || !matchesWorld(game.world) || event.type!=Common::EVENT_KEYDOWN) return kWorldConversationUnsupported;
        const Common::KeyCode key=event.kbd.keycode;
        if (_stage==1) {
            if (key==Common::KEYCODE_ESCAPE) { restore(frame);return kWorldConversationFinished; }
            uint cell;
            switch (key) {
            case Common::KEYCODE_UP:cell=76;break;case Common::KEYCODE_DOWN:cell=110;break;
            case Common::KEYCODE_RIGHT:cell=94;break;case Common::KEYCODE_LEFT:cell=92;break;
            default:return kWorldConversationHandled;
            }
            Common::Array<byte> data,footer;MapAssetRange assets;
            if (!archive.read("NA/GAME",data) || !resolveMapAssets(data,_type,assets) ||
                    !archive.readRange(assets.resource,assets.offset+assets.size,72,footer)) return kWorldConversationUnsupported;
            // M:FEC6..FED9 and LWR1559: a counter permits the same
            // direction's second cell, not an arbitrary nearby NPC search.
            if (scene.actors.lookup[cell]==255 && scene.actors.terrain[cell]>=footer[30] && scene.actors.terrain[cell]<footer[31]) {
                switch (key) {
                case Common::KEYCODE_UP:cell=59;break;case Common::KEYCODE_DOWN:cell=127;break;
                case Common::KEYCODE_RIGHT:cell=95;break;case Common::KEYCODE_LEFT:cell=91;break;
                default:return kWorldConversationUnsupported;
                }
            }
            const byte slot=scene.actors.lookup[cell],kind=scene.actors.kinds[cell]&135;
            if (slot==255) return refuseStrip(20,message);
            if (!kind) return refuseStrip(19,message);
            if (kind<4) return refuseStrip(20,message);
            if (kind!=4 || (slot&7)) return kWorldConversationUnsupported;
            const byte *tables=game.world.currentTables()->data(),*actor=tables+1280+slot;
            const byte role=actor[5]&127;
            // M:FEF1-FF58: ordinary service roles dispatch their own complete
            // shop/inn/training owners. Do not treat them as generic dialogue.
            if (role<8 || role==16) {
                // M:FF01-FF13 bypasses the flag/capability gate in underworld2.
                if (_type!=2 && (!(actor[4]&64) || actor[6])) return refuseStrip(9,message);
                // choose() admits only on-foot context. FF13's mounted/swimming
                // compact-player sprite sentinels belong to transport callers;
                // they must not be read from unrelated first-NPC coordinates.
                if (role==0) {
                    MerchantCatalog merchants;MerchantOffer offer;
                    if (!merchants.load(archive) || !merchants.select(_map,slot,offer)) return kWorldConversationUnsupported;
                    _slot=slot;_query.clear();_merchantSale=false;_merchantIndex=0;
                    return merchantText(archive,offer.welcome,22,frame)?kWorldConversationHandled:kWorldConversationUnsupported;
                }
                if (role==1 || role==3 || role==4 || role==6) {
                    VendorCatalog vendors;VendorOffer offer;
                    if (!vendors.load(archive) || !vendors.select(role,game.world.mapId(),slot,offer)) return kWorldConversationUnsupported;
                    _slot=slot;_vendorRole=role;_query.clear();
                    const bool already=role==3 && game.world.field(66)!=0;
                    return vendorText(archive,already?0x4a98+0x2a6:offer.textOffset,already || role==4,frame)?kWorldConversationHandled:kWorldConversationUnsupported;
                }
                if (role==7) {
                    _slot=slot;
                    if (!_temple.begin(archive,game,slot)) return kWorldConversationUnsupported;
                    return serviceText(archive,_temple.offer(),false,frame)?kWorldConversationHandled:kWorldConversationUnsupported;
                }
                if (role==2) {
                    _slot=slot;if (!_inn.begin(archive,game,slot)) return kWorldConversationUnsupported;
                    return innText(archive,_inn.offer(),frame)?kWorldConversationHandled:kWorldConversationUnsupported;
                }
                if (role!=16) return kWorldConversationUnsupported;
            }
            _slot=slot;ConversationContext context;
            context.map=_map;context.mapType=_type;context.hour=game.clock.hour;context.minute=game.clock.minute;
            context.partyFull=game.party.memberCount()>=6;context.serviceAccess=role==17?2:3;
            // Historical API name: this is the source additional-gate byte.
            // Original new-game147 and retained pre-binding context both use1;
            // other caller lifetimes remain part of the service/UI integration.
            context.adultMode=true;
            uint refusal=0;
            if (actor[6]==255) refusal=0x12d; // hostile refusal text in immutable Talk data
            else if (scene.actors.terrain[cell]==footer[58] || scene.actors.terrain[cell]==footer[59]) refusal=0xf50;
            else if (actor[2]>=footer[64] && actor[2]<footer[65])
                refusal=_map==41?0x70:_map==76?0x27:(_map>=71 && _map<74)?1:0xb8;
            if (refusal) {
                if (!Conversation::initializeFields(game.world._fields,256,context)) return kWorldConversationUnsupported;
                Common::Array<byte> raw;if (!archive.read("NA/SRTN.NPC.TALK",raw)) return kWorldConversationUnsupported;
                Common::String text;if (!plainDialogueBody(raw,refusal,text)) return kWorldConversationUnsupported;
                _refused=true;_closing=true;_hasDialogue=false;
                return show(archive,text,frame)?kWorldConversationHandled:kWorldConversationUnsupported;
            }
            if (!_conversation.begin(archive,game.world.dialogueResource(),slot,game.world.questFields(),256,context) ||
                    _conversation.opening(random)!=kConversationReply) return kWorldConversationUnsupported;
            _hasDialogue=true;_closing=_conversation.terminalReply();
            return showDialogue(archive,game,frame,random)?kWorldConversationHandled:kWorldConversationUnsupported;
        }
        if (_stage>=27) return trainingInput(event,archive,game,frame,random);
        if (_stage>=22) return merchantInput(event,archive,game,frame);
        if (_stage>=19) return vendorInput(event,archive,game,frame,transport);
        if (_stage>=9) return innInput(event,archive,game,frame);
        if (_stage>=5) return serviceInput(event,archive,game,frame);
        if (_stage==2) {
            if (!_window.waiting() || !_window.handle(event) || !_window.paint(frame) || !settle(archive,frame,&game.discovery,random,&game) || (_stage==3 && _conversation.trainingPending() && !trainingText(archive,0x9790,27,frame))) return kWorldConversationUnsupported;
            return kWorldConversationHandled;
        }
        if (_stage==4) {
            if (_hasDialogue) {
                NewGame next(game);Conversation conversation=_conversation;ItemCatalog catalog;
                if (!catalog.load(archive)) return kWorldConversationUnsupported;
                const HandoffResult handoff=conversation.finish(catalog,next.inventory,next.gold);
                if (handoff==kHandoffNeedsParty) {
                    DialogueHandoff request;
                    if (!describeDialogueHandoff(conversation.fields(),256,request) || !conversation.finishRecruitment() || !next.world.acceptConversation(conversation) ||
                        Recruitment::apply(archive,next,_slot,request.companion)!=kRecruitApplied) return kWorldConversationUnsupported;
                } else if (handoff!=kHandoffApplied || !next.world.acceptConversation(conversation)) return kWorldConversationUnsupported;
                next.clock.hourlyCounter=next.world.questFields()[20];
                game.adoptState(next);_conversation=conversation;
            }
            restore(frame);return kWorldConversationFinished;
        }
        if (_stage!=3) return kWorldConversationUnsupported;
        if (_conversation.paymentPhase()) {
            Common::String text;const byte answer=key==Common::KEYCODE_ESCAPE?27:byte(event.kbd.ascii);
            const int result=_conversation.paymentInput(archive,answer,game.gold,text,random);
            if(result<0) return kWorldConversationUnsupported;
            if(result==2) {_query.clear();return input(archive,frame)?kWorldConversationHandled:kWorldConversationUnsupported;}
            if(result==1) {_closing=_conversation.terminalReply();return show(archive,text,frame,&game.discovery,random,&game)?kWorldConversationHandled:kWorldConversationUnsupported;}
            return kWorldConversationHandled;
        }
        if (key==Common::KEYCODE_TAB) {
            if (!_conversation.cycleVoice()) return kWorldConversationUnsupported;
            return input(archive,frame)?kWorldConversationHandled:kWorldConversationUnsupported;
        }
        if (key==Common::KEYCODE_BACKSPACE || key==Common::KEYCODE_DELETE) { if (!_query.empty()) _query.deleteLastChar(); }
        else if (key==Common::KEYCODE_RETURN || key==Common::KEYCODE_ESCAPE) {
            Common::Array<byte> raw;if (!archive.read("NA/SRTN.NPC.TALK",raw) || raw.size()!=32831) return kWorldConversationUnsupported;
            // BA3F..BA62 trims leading spaces before Return/Escape submission.
            uint first=0;while (first<_query.size() && _query[first]==' ') ++first;
            Common::String submitted(_query.c_str()+first);
            if (submitted.empty()) for (uint i=0;i<3;++i) submitted+=char(raw[0xf8c+i]&127);
            bool farewell=submitted.size()>=3;
            for (uint i=0;i<3 && farewell;++i) farewell=(byte(submitted[i])&95)==(raw[0xf8c+i]&127);
            if (_conversation.submit(submitted,random)!=kConversationReply) return kWorldConversationUnsupported;
            _closing=farewell || _conversation.terminalReply();_endShown=_footerShown=false;_query.clear();
            return showDialogue(archive,game,frame,random)?kWorldConversationHandled:kWorldConversationUnsupported;
        } else if (event.kbd.ascii>=32 && event.kbd.ascii<127 && _query.size()<28) _query+=char(event.kbd.ascii);
        return input(archive,frame)?kWorldConversationHandled:kWorldConversationUnsupported;
    }
    bool save(Common::WriteStream &out) const {
        if (_worldRefresh) {
            Common::ScopedPtr<WorldConversation> base(new WorldConversation(*this));base->_worldRefresh=false;
            Common::MemoryWriteStreamDynamic body(DisposeAfterUse::YES);if (!base->save(body)) return false;
            out.writeUint32BE(MKTAG('N','C','U','I'));out.writeUint16LE(8);out.writeUint32LE(body.size());out.write(body.getData(),body.size());return !out.err();
        }
        if (_stage>30 || _query.size()>28) return false;
        out.writeUint32BE(MKTAG('N','C','U','I'));out.writeUint16LE(_stage>=27?7:_stage>=22?6:_vendorRole?5:4);out.writeByte(_stage);
        if (!active()) return !out.err();
        out.writeByte(_type);out.writeByte(_map);out.writeByte(_slot);
        out.writeByte(byte((_closing?1:0)|(_endShown?2:0)|(_footerShown?4:0)|(_hasDialogue?8:0)|(_hasWindow?16:0)|(_refused?32:0)));
        out.writeByte(byte(_query.size()));out.write(_query.c_str(),_query.size());out.write(_underlay.data(),8192);
        if (_hasDialogue) { Common::MemoryWriteStreamDynamic part(DisposeAfterUse::YES);if (!_conversation.save(part)) return false;out.writeUint32LE(part.size());out.write(part.getData(),part.size()); }
        if (_hasWindow) { Common::MemoryWriteStreamDynamic part(DisposeAfterUse::YES);if (!_window.save(part)) return false;out.writeUint32LE(part.size());out.write(part.getData(),part.size()); }
        out.writeByte(_temple.phase()!=0);
        if (_temple.phase() && !_temple.save(out)) return false;
        out.writeByte(_inn.phase()!=0);if (_inn.phase() && !_inn.save(out)) return false;
        out.writeByte(_memberMode);out.writeByte(_waitingSlot);out.writeUint16LE(_pickupFee);
        out.writeByte(_selection.active());if (_selection.active() && !_selection.save(out)) return false;
        if (_stage>=27) out.writeByte(_trainingMember);
        else if (_stage>=22) {out.writeByte(_merchantSale);out.writeUint16LE(_merchantIndex);}
        else if (_vendorRole) out.writeByte(_vendorRole);
        return !out.err();
    }
    bool load(Common::SeekableReadStream &in,HdvArchive &archive) {
        if (in.readUint32BE()!=MKTAG('N','C','U','I')) return false;
        const uint16 version=in.readUint16LE();
        if (version==8) {
            const uint32 size=in.readUint32LE();if (!size || size>262144 || int64(size)!=in.size()-in.pos()) return false;
            Common::Array<byte> bytes;bytes.resize(size);if (in.read(bytes.data(),size)!=size || size<6 || READ_BE_UINT32(bytes.data())!=MKTAG('N','C','U','I') || READ_LE_UINT16(bytes.data()+4)>7) return false;
            Common::MemoryReadStream body(bytes.data(),size);Common::ScopedPtr<WorldConversation> next(new WorldConversation);
            if (!next->load(body,archive) || !next->active()) return false;next->_worldRefresh=true;*this=*next;return true;
        }
        if (version<1 || version>7) return false;
        WorldConversation next;next._stage=in.readByte();if (next._stage>(version==1?4:version==2?8:version==3?14:version==4?18:version==5?21:version==6?26:30)) return false;
        if (next.active()) {
            next._type=in.readByte();next._map=in.readByte();next._slot=in.readByte();const byte flags=in.readByte(),length=in.readByte();
            if (flags>63 || length>28 || (next._slot!=255 && (next._slot&7))) return false;
            next._closing=(flags&1)!=0;next._endShown=(flags&2)!=0;next._footerShown=(flags&4)!=0;
            next._hasDialogue=(flags&8)!=0;next._hasWindow=(flags&16)!=0;next._refused=(flags&32)!=0;
            if ((next._stage==1 && flags) || (next._hasDialogue && next._refused) || (next._stage==3 && !next._hasDialogue) || (next._stage==2 && !next._hasWindow)) return false;
            for (uint i=0;i<length;++i) {const byte ch=in.readByte();if (ch<32 || ch>=127) return false;next._query+=char(ch);}
            byte image[8192];if (in.read(image,8192)!=8192 || !next._underlay.importImage(image,8192)) return false;
            for (uint i=0;i<2;++i) if (i?next._hasWindow:next._hasDialogue) {
                const uint32 size=in.readUint32LE();if (!size || size>131072 || int64(size)>in.size()-in.pos()) return false;
                Common::Array<byte> bytes;bytes.resize(size);if (in.read(bytes.data(),size)!=size) return false;Common::MemoryReadStream part(bytes.data(),size);
                if (i?!next._window.load(part,archive):!next._conversation.load(part,archive)) return false;
            }
            if (next._hasDialogue && (next._conversation.context().map!=next._map || next._conversation.context().mapType!=next._type || next._conversation.packet().actor!=next._slot)) return false;
            if (version>=2) {
                const byte service=in.readByte();if (service>1) return false;
                if (service) {
                    byte bytes[16];if (in.read(bytes,16)!=16) return false;Common::MemoryReadStream part(bytes,16);
                    if (!next._temple.load(part) || !next._temple.phase() || next._stage<5 || next._hasDialogue) return false;
                } else if (next._stage>=5 && next._stage<=8) return false;
            }
            if (version>=3) {
                const byte service=in.readByte();if (service>1) return false;
                if (service) {
                    byte bytes[20];if (in.read(bytes,20)!=20) return false;Common::MemoryReadStream part(bytes,20);
                    if (!next._inn.load(part) || !next._inn.phase() || next._stage<9 || next._hasDialogue || next._temple.phase()) return false;
                } else if (next._stage>=9 && next._stage<=18) return false;
            }
            if (version>=4) {
                next._memberMode=in.readByte();next._waitingSlot=in.readByte();next._pickupFee=in.readUint16LE();const byte selected=in.readByte();
                if (next._memberMode>3 || (next._waitingSlot!=255 && next._waitingSlot>=12) || selected>1 ||
                        (next._memberMode && next._inn.phase()!=2)) return false;
                if (selected) {byte bytes[10];if (in.read(bytes,10)!=10) return false;Common::MemoryReadStream part(bytes,10);if (!next._selection.load(part) || !next._selection.active()) return false;}
            }
        }
        if (version==7 && next.active()) {
            next._trainingMember=in.readByte();
            if (next._stage<27 || !next._hasDialogue || next._temple.phase() || next._inn.phase() ||
                (next._trainingMember!=255 && next._trainingMember>=6) || next._query.size()>2 ||
                ((next._stage==27 || next._stage==30) && !next._hasWindow) || (next._stage==28 && next._hasWindow) ||
                ((next._stage==28)!=next._selection.active()) ||
                (next._stage!=30 && !next._conversation.trainingPending()) ||
                (next._stage==29 && next._trainingMember==255)) return false;
            for (uint i=0;i<next._query.size();++i) if (next._query[i]<'0' || next._query[i]>'9') return false;
        } else if (version==6 && next.active()) {
            const byte sale=in.readByte();next._merchantSale=sale!=0;next._merchantIndex=in.readUint16LE();
            if (sale>1 || next._merchantIndex>255 || next._stage<22 || next._hasDialogue || next._temple.phase() || next._inn.phase() || next._query.size()>2 ||
                ((next._stage==22 || next._stage==26)!=next._hasWindow)) return false;
            MerchantCatalog merchants;MerchantOffer offer;if (!merchants.load(archive) || !merchants.select(next._map,next._slot,offer)) return false;
            for (uint i=0;i<next._query.size();++i) if (next._query[i]<'0' || next._query[i]>'9') return false;
        } else if (version>=5 && next.active()) {
            next._vendorRole=in.readByte();
            if ((next._vendorRole!=1 && next._vendorRole!=3 && next._vendorRole!=4 && next._vendorRole!=6) || next._stage<19 || !next._hasWindow || next._hasDialogue || next._temple.phase() || next._inn.phase() || next._query.size()>2) return false;
            for (uint i=0;i<next._query.size();++i) if (next._query[i]<'0' || next._query[i]>'9') return false;
        } else if (next._stage>=19) return false;
        if (in.err() || in.pos()!=in.size()) return false;*this=next;return true;
    }
private:
    bool applyCallbacks(HdvArchive &archive,NewGame &game) {
        SoundEvents speech;
        if (_conversation.worldCallbacksPending() && (_conversation.fields()[12]&32) && _sounds && _effectsEnabled) {
            PraevocSpeechRules rules;if (!rules.load(archive) || !rules.baseCue(_speechCursor,speech)) return false;
        }
        bool refresh=false;if (!game.world.applyConversationCallbacks(_conversation,refresh)) return false;
        _worldRefresh|=refresh;
        if (_sounds) for (uint i=0;i<speech.size();++i) _sounds->push_back(speech[i]);
        return true;
    }
    bool showDialogue(HdvArchive &archive,NewGame &game,HgrFrame &frame,GameplayRandom *random=nullptr) {
        if (!applyCallbacks(archive,game)) return false;
        return show(archive,_conversation.reply(),frame,&game.discovery,random,&game) &&
            !(_stage==3 && _conversation.trainingPending() && !trainingText(archive,0x9790,27,frame));
    }
    static bool trainingBody(HdvArchive &archive,uint address,Common::String &text) {
        Common::Array<byte> raw;return archive.read("NA/SRTN.NPC.TALK",raw) && plainDialogueBody(raw,0x1561+address-0x9600,text);
    }
    bool trainingText(HdvArchive &archive,uint address,byte stage,HgrFrame &frame) {
        Common::String text;return trainingBody(archive,address,text) && merchantText(archive,text,stage,frame);
    }
    bool trainingOffer(HdvArchive &archive,const NewGame &game,HgrFrame &frame) {
        TrainingQuote offer;Common::String first,middle,last;
        if (!TrainingService::quote(archive,game,_conversation.fields()[22],_trainingMember,offer) ||
            !trainingBody(archive,0x97a0,first) || !trainingBody(archive,0x97d7,middle) || !trainingBody(archive,0x97f6,last)) return false;
        if (!merchantText(archive,first+Common::String::format("%u",offer.available)+middle+Common::String::format("%u",offer.price)+last,29,frame)) return false;
        if (_window.waiting()) return true;
        _hasWindow=false;return trainingQuantity(archive,frame);
    }
    bool trainingQuantity(HdvArchive &archive,HgrFrame &frame) const {
        GlyphFont font;if (!font.load(archive,1) || !frame.clearTextSpan(21,22,14)) return false;
        const Common::String text=Common::String("Count: ")+_query;
        for (uint i=0;i<text.size();++i) if (!frame.drawGlyph(font,byte(text[i])|128,21+i,22,0)) return false;
        return frame.drawGlyph(font,0xa0,21+text.size(),22,127);
    }
    WorldConversationResult trainingResult(HdvArchive &archive,uint address,HgrFrame &frame) {
        if (!_conversation.completeTraining()) return kWorldConversationUnsupported;
        _query.clear();return trainingText(archive,address,30,frame)?kWorldConversationHandled:kWorldConversationUnsupported;
    }
    WorldConversationResult trainingInput(const Common::Event &event,HdvArchive &archive,NewGame &game,HgrFrame &frame,GameplayRandom *random=nullptr) {
        const Common::KeyCode key=event.kbd.keycode;
        if (_stage==27 || _stage==30) {
            if (_window.waiting()) {
                if (!_window.handle(event) || !_window.paint(frame)) return kWorldConversationUnsupported;
                if (_window.waiting()) return kWorldConversationHandled;
            }
            _hasWindow=false;
            if (_stage==30) {
                _stage=3;return (_closing?settle(archive,frame,&game.discovery,random,&game):input(archive,frame))?kWorldConversationHandled:kWorldConversationUnsupported;
            }
            if (!_selection.begin(game.party,false) || !_selection.paint(archive,game.party,frame)) return kWorldConversationUnsupported;
            _stage=28;return kWorldConversationHandled;
        }
        if (_stage==28) {
            const PartySelectionResult selected=_selection.handle(event,game.party);
            if (selected==kPartySelectionInvalid) return kWorldConversationUnsupported;
            if (selected==kPartySelectionPending) return _selection.paint(archive,game.party,frame)?kWorldConversationHandled:kWorldConversationUnsupported;
            if (selected==kPartySelectionCancelled) return trainingResult(archive,0x98c1,frame);
            _trainingMember=_selection.selected();TrainingQuote offer;
            if (!TrainingService::quote(archive,game,_conversation.fields()[22],_trainingMember,offer)) return kWorldConversationUnsupported;
            if (!offer.available) return trainingResult(archive,0x988b,frame);
            _query.clear();_stage=29;return trainingOffer(archive,game,frame)?kWorldConversationHandled:kWorldConversationUnsupported;
        }
        if (_stage!=29) return kWorldConversationUnsupported;
        if (_hasWindow) {
            if (!_window.handle(event) || !_window.paint(frame)) return kWorldConversationUnsupported;
            if (_window.waiting()) return kWorldConversationHandled;
            _hasWindow=false;return trainingQuantity(archive,frame)?kWorldConversationHandled:kWorldConversationUnsupported;
        }
        if (key==Common::KEYCODE_ESCAPE) return trainingResult(archive,0x98c1,frame);
        if (key==Common::KEYCODE_BACKSPACE) {if (!_query.empty()) _query.deleteLastChar();}
        else if (event.kbd.ascii>='0' && event.kbd.ascii<='9' && _query.size()<2) _query+=char(event.kbd.ascii);
        else if (key==Common::KEYCODE_RETURN) {
            byte quantity=0;for (uint i=0;i<_query.size();++i) quantity=byte(quantity*10+_query[i]-'0');
            const TrainingResult result=TrainingService::apply(archive,game,_conversation.fields()[22],_trainingMember,quantity);
            if (result==kTrainingInvalid) {_query.clear();return trainingQuantity(archive,frame)?kWorldConversationHandled:kWorldConversationUnsupported;}
            return trainingResult(archive,result==kTrainingApplied?0x9818:result==kTrainingPoor?0x985a:result==kTrainingNoPractice?0x988b:0x98c1,frame);
        }
        return trainingQuantity(archive,frame)?kWorldConversationHandled:kWorldConversationUnsupported;
    }
    bool merchantText(HdvArchive &archive,const Common::String &text,byte stage,HgrFrame &frame) {
        Common::Array<byte> bytes;encoded(text,bytes);HgrFrame background=frame;
        if (!OpeningBackdrop::renderTempleFrame(background)) return false;
        TextWindow window;if (!window.begin(archive,bytes.data(),bytes.size(),21,18,14,22,8,&background) || !window.paint(background)) return false;
        _window=window;_hasWindow=true;_stage=stage;frame=background;return true;
    }
    WorldConversationResult merchantInput(const Common::Event &event,HdvArchive &archive,NewGame &game,HgrFrame &frame) {
        MerchantCatalog merchants;ItemCatalog items;MerchantOffer offer;Common::Array<MerchantSelection> list;
        if (!merchants.load(archive) || !items.load(archive) || !merchants.select(_map,_slot,offer) ||
                !MerchantScreen::selections(archive,game,offer,_merchantSale,list)) return kWorldConversationUnsupported;
        const Common::KeyCode key=event.kbd.keycode;const byte ascii=byte(event.kbd.ascii)&95;
        if (_stage==22 || _stage==26) {
            if (_window.waiting()) {
                if (!_window.handle(event) || !_window.paint(frame)) return kWorldConversationUnsupported;
                return kWorldConversationHandled;
            }
            _hasWindow=false;_stage=23;_query.clear();
            if (_merchantIndex>=list.size()) _merchantIndex=list.empty()?0:uint16(list.size()-1);
        } else if (_stage==23) {
            if (key==Common::KEYCODE_ESCAPE) {restore(frame);return kWorldConversationFinished;}
            if (ascii=='B' || ascii=='S') {_merchantSale=ascii=='S';_merchantIndex=0;}
            else if (key==Common::KEYCODE_DOWN && uint(_merchantIndex)+1<list.size()) ++_merchantIndex;
            else if (key==Common::KEYCODE_UP && _merchantIndex) --_merchantIndex;
            else if (key==Common::KEYCODE_RIGHT && uint(_merchantIndex)+6<list.size()) _merchantIndex+=6;
            else if (key==Common::KEYCODE_LEFT) _merchantIndex=_merchantIndex>=6?_merchantIndex-6:0;
            else if (key==Common::KEYCODE_RETURN && !list.empty()) {
                const MerchantSelection &entry=list[_merchantIndex];_query.clear();
                game.commerceCounter.kind=entry.kind;game.commerceCounter.kindKnown=true;
                if (_merchantSale) {game.commerceCounter.quantity=0;game.commerceCounter.quantityKnown=true;}
                _stage=_merchantSale?25:24;
                if (!_merchantSale && entry.kind==0 && entry.id<64) _query="1";
                if (_merchantSale || _query.empty()) return MerchantScreen::paint(archive,game,offer,_merchantSale,_merchantIndex,_stage,_query,frame)?kWorldConversationHandled:kWorldConversationUnsupported;
            }
        } else if (_stage==24) {
            if (key==Common::KEYCODE_BACKSPACE) {if (!_query.empty()) _query.deleteLastChar();}
            else if (event.kbd.ascii>='0' && event.kbd.ascii<='9' && _query.size()<2) _query+=char(event.kbd.ascii);
            else if (key!=Common::KEYCODE_RETURN && key!=Common::KEYCODE_ESCAPE)
                return kWorldConversationHandled;
        }
        const bool immediate=_stage==24 && !_merchantSale && _merchantIndex<list.size() && list[_merchantIndex].kind==0 && list[_merchantIndex].id<64;
        if (immediate || (_stage==24 && (key==Common::KEYCODE_RETURN || key==Common::KEYCODE_ESCAPE)) || _stage==25) {
            if (_merchantIndex>=list.size()) return kWorldConversationUnsupported;
            const MerchantSelection entry=list[_merchantIndex];NewGame next(game);MerchantTransaction transaction;bool ok;
            if (_merchantSale) {
                byte choice=ascii=='Y'?1:ascii=='A'?2:0;
                if (!choice && key!=Common::KEYCODE_ESCAPE && key!=Common::KEYCODE_RETURN) return kWorldConversationHandled;
                ItemDefinition definition;if (!items.definition(entry.kind,entry.id,definition)) return kWorldConversationUnsupported;
                bool ready=false;for (uint i=0;i<10;++i) ready|=next.book.spell(i)==definition.bytes[6];
                ok=MerchantTransactions::sell(merchants,items,offer,entry.slot,entry.available,entry.kind==2 && entry.id<64,ready,choice,
                    next.inventory,next.world._fields,256,next.gold,transaction);
                if (ok && transaction.result!=kMerchantNoValue && choice) {
                    const uint16 originalGold=game.gold;
                    byte retained=choice==1?1:byte(MIN<uint16>((9999-originalGold)/transaction.unitPrice,entry.available));
                    if (entry.kind==2 && entry.id<64 && ready && entry.available>1 && retained!=1 && retained) --retained;
                    next.commerceCounter.quantity=retained;next.commerceCounter.quantityKnown=true;
                }
            } else {
                byte quantity=0;for (uint i=0;i<_query.size();++i) quantity=byte(quantity*10+_query[i]-'0');
                if (key==Common::KEYCODE_ESCAPE && !immediate) quantity=0;
                ok=MerchantTransactions::buy(merchants,items,offer,entry.kind,entry.id,quantity,next.inventory,next.world._fields,256,next.gold,transaction);
            }
            if (!ok) return kWorldConversationUnsupported;
            if (!_merchantSale && (immediate || (key!=Common::KEYCODE_ESCAPE && !_query.empty() && _query!="0" && _query!="00"))) {
                byte requested=0;for (uint i=0;i<_query.size();++i) requested=byte(requested*10+_query[i]-'0');
                next.commerceCounter.quantity=requested;next.commerceCounter.quantityKnown=true;
            }
            uint message=transaction.result==kMerchantApplied?(_merchantSale?0x9c4f:0x9b79):
                transaction.result==kMerchantPoor?0x9b37:transaction.result==kMerchantFull?0x9bb0:
                transaction.result==kMerchantOverflow?0x9b93:transaction.result==kMerchantCancelled?0x9bc2:
                transaction.result==kMerchantGoldFull?0x9c6a:transaction.result==kMerchantNoValue?0x9caa:0x9c87;
            Common::Array<byte> raw;Common::String text;
            if (!archive.read("NA/SRTN.INVENTORY",raw) || !plainDialogueBody(raw,0x7996+message-0x9b06,text) ||
                !merchantText(archive,text,26,frame)) return kWorldConversationUnsupported;
            game.adoptState(next);return kWorldConversationHandled;
        }
        return MerchantScreen::paint(archive,game,offer,_merchantSale,_merchantIndex,_stage,_query,frame)?kWorldConversationHandled:kWorldConversationUnsupported;
    }
    bool vendorText(HdvArchive &archive,uint offset,bool result,HgrFrame &frame) {
        Common::Array<byte> raw,bytes;Common::String text;
        if (!archive.read("NA/SRTN.NPC.TALK",raw) || !plainDialogueBody(raw,offset,text)) return false;
        HgrFrame background=frame;if (!OpeningBackdrop::renderTempleFrame(background)) return false;
        encoded(text,bytes);TextWindow window;
        if (!window.begin(archive,bytes.data(),bytes.size(),21,18,14,22,8,&background) || !window.paint(background)) return false;
        _window=window;_hasWindow=true;_stage=result?21:window.waiting()?19:20;frame=background;return true;
    }
    bool vendorNumber(HdvArchive &archive,HgrFrame &frame) const {
        GlyphFont font;if (!font.load(archive,1)) return false;
        for (uint i=0;i<3;++i) if (!frame.drawGlyph(font,i<_query.size()?byte(_query[i])|128:0xa0,34+i,22,0)) return false;
        return true;
    }
    WorldConversationResult vendorInput(const Common::Event &event,HdvArchive &archive,NewGame &game,HgrFrame &frame,WorldTransportState *transport) {
        if (_vendorRole!=1 && _vendorRole!=3 && _vendorRole!=4 && _vendorRole!=6) return kWorldConversationUnsupported;
        if (_stage==19 || _stage==21) {
            if (_window.waiting()) {
                if (!_window.handle(event) || !_window.paint(frame)) return kWorldConversationUnsupported;
                if (!_window.waiting() && _stage==19) _stage=20;
                return kWorldConversationHandled;
            }
            if (_stage==21) {restore(frame);return kWorldConversationFinished;}
            _stage=20;return kWorldConversationHandled;
        }
        if (_stage!=20) return kWorldConversationUnsupported;
        const Common::KeyCode key=event.kbd.keycode;
        if (_vendorRole!=1) {
            VendorCatalog vendors;VendorOffer offer;VendorResult result;NewGame next(game);WorldTransportState controls;
            if (!vendors.load(archive) || !vendors.select(_vendorRole,_map,_slot,offer)) return kWorldConversationUnsupported;
            uint offset;
            if (_vendorRole==3) {
                byte choice=0;if (key!=Common::KEYCODE_ESCAPE) {
                    if (event.kbd.ascii<'1' || event.kbd.ascii>'3') return kWorldConversationHandled;
                    choice=byte(event.kbd.ascii-'0');
                }
                byte ownership=next.world.field(66);
                if (!VendorTransactions::boat(offer,choice,next.gold,ownership,result)) return kWorldConversationUnsupported;
                next.world._fields[66]=ownership;
                offset=0x4a98+(result==kVendorBought?0x251:result==kVendorPoor?0x224:result==kVendorAlreadyOwned?0x2a6:0x288);
            } else {
                if (!transport) return kWorldConversationUnsupported;controls=*transport;
                if (!controls.used) controls.appearance=controls.baseAppearance=controls.statusAppearance=game.appearance;
                byte *objects=next.world._states[next.world._current].tables.data()+1024;
                if (!VendorTransactions::mount(offer,(byte(event.kbd.ascii)&95)=='Y',next.gold,next.world.x(),next.world.y(),objects,256,controls,result)) return kWorldConversationUnsupported;
                if (result==kVendorBought) {next.world._onFoot=false;next.appearance=controls.appearance;}
                offset=0x656f+(result==kVendorBought?0x274:result==kVendorPoor?0x216:0x264);
            }
            if (!vendorText(archive,offset,true,frame)) return kWorldConversationUnsupported;
            game.adoptState(next);if (_vendorRole==6) *transport=controls;return kWorldConversationHandled;
        }
        if (key==Common::KEYCODE_BACKSPACE) {if (!_query.empty()) _query.deleteLastChar();}
        else if (key==Common::KEYCODE_RETURN || key==Common::KEYCODE_ESCAPE) {
            uint requested=0;if (key!=Common::KEYCODE_ESCAPE) for (uint i=0;i<_query.size();++i) requested=requested*10+_query[i]-'0';
            VendorCatalog vendors;VendorOffer offer;FoodPurchase purchase;
            if (!vendors.load(archive) || !vendors.select(1,_map,_slot,offer) || !VendorTransactions::food(byte(game.party.memberCount()),game.food,game.gold,offer.price,byte(requested),purchase)) return kWorldConversationUnsupported;
            uint relative=0x39b;
            if (purchase.result==kVendorBought) relative=purchase.clipped?0x3de:0x376;
            else if (purchase.result==kVendorPoor) relative=0x300;
            else if (purchase.result==kVendorFull) relative=0x333;
            if (!vendorText(archive,0x186f+relative,true,frame)) return kWorldConversationUnsupported;
            if (purchase.result==kVendorBought) {game.food=purchase.food;game.gold=purchase.gold;}
            _query.clear();return kWorldConversationHandled;
        } else if (event.kbd.ascii>='0' && event.kbd.ascii<='9' && _query.size()<2) _query+=char(event.kbd.ascii);
        return vendorNumber(archive,frame)?kWorldConversationHandled:kWorldConversationUnsupported;
    }
    WorldConversationResult innInput(const Common::Event &event,HdvArchive &archive,NewGame &game,HgrFrame &frame) {
        if (_stage>=15) return memberInput(event,archive,game,frame);
        if (_stage==13) {
            if (!_window.waiting() || !_window.handle(event) || !_window.paint(frame)) return kWorldConversationUnsupported;
            if (!_window.waiting()) _stage=_inn.phase()==5?12:byte(_inn.phase()+8);
            return kWorldConversationHandled;
        }
        if (_stage==12) {restore(frame);return kWorldConversationFinished;}
        const Common::KeyCode key=event.kbd.keycode;const byte ch=byte(event.kbd.ascii)&95;
        if (_stage==9) {
            if (!_inn.welcome(ch=='Y')) return kWorldConversationUnsupported;
        } else if (_stage==10) {
            if (ch=='R') {if (!_inn.requestRest()) return kWorldConversationUnsupported;}
            else if (ch=='P') return pickupOffer(archive,game,frame,0);
            else if (ch=='L') {
                if (game.party.memberCount()<2) {if (!_inn.requestRest()) return kWorldConversationUnsupported;}
                else {
                    Common::Array<byte> raw;const uint at=_inn.offer()-12;
                    if (!archive.read("NA/SRTN.NPC.TALK",raw) || at+12>=raw.size()) return kWorldConversationUnsupported;
                    const uint size=READ_LE_UINT16(raw.data()+at);if (size<12 || size>raw.size()-at) return kWorldConversationUnsupported;
                    const uint relative=READ_LE_UINT16(raw.data()+at+size-2);if (relative>=size) return kWorldConversationUnsupported;
                    _memberMode=2;Common::String body;if (!plainDialogueBody(raw,at+relative,body)) return kWorldConversationUnsupported;
                    return memberBody(archive,body,frame)?kWorldConversationHandled:kWorldConversationUnsupported;
                }
            }
            else if (key==Common::KEYCODE_ESCAPE) {if (!_inn.requestRest() || !_inn.confirm(false,game)) return kWorldConversationUnsupported;}
            else return kWorldConversationHandled;
        } else if (_stage==11) {
            if (!_inn.confirm(ch=='Y',game)) return kWorldConversationUnsupported;
            if (_inn.phase()==4) return kWorldConversationTravel;
        } else return kWorldConversationUnsupported;
        const uint offset=_inn.phase()==2?0x2b44+0x4bb:_inn.phase()==3?_inn.quote():
            _inn.result()==kInnPoor?0x2b44+0x449:0x2b44+0x4a0;
        return innText(archive,offset,frame)?kWorldConversationHandled:kWorldConversationUnsupported;
    }
    bool innText(HdvArchive &archive,uint offset,HgrFrame &frame) {
        Common::Array<byte> raw,bytes;Common::String text;
        if (!archive.read("NA/SRTN.NPC.TALK",raw) || !plainDialogueBody(raw,offset,text)) return false;
        return innBody(archive,text,frame);
    }
    bool innBody(HdvArchive &archive,const Common::String &text,HgrFrame &frame) {
        Common::Array<byte> bytes;
        HgrFrame background=frame;if (!OpeningBackdrop::renderTempleFrame(background)) return false;
        encoded(text,bytes);TextWindow window;
        if (!window.begin(archive,bytes.data(),bytes.size(),21,18,14,22,8,&background) || !window.paint(background)) return false;
        _window=window;_hasWindow=true;_stage=window.waiting()?13:_inn.phase()==5?12:byte(_inn.phase()+8);frame=background;return true;
    }
    bool memberBody(HdvArchive &archive,const Common::String &text,HgrFrame &frame) {
        if (!innBody(archive,text,frame)) return false;_stage=_window.waiting()?18:_memberMode==1?15:16;return true;
    }
    Common::String waitingName(const NewGame &game) const {
        Common::String name;const byte *member=game.inn.member(_waitingSlot);if (!member) return name;
        for (uint i=0;i<15 && member[75+i];++i) name+=char(member[75+i]&127);return name;
    }
    WorldConversationResult pickupOffer(HdvArchive &archive,NewGame &game,HgrFrame &frame,uint first) {
        _waitingSlot=255;
        for (uint i=first;i<12;++i) if (game.inn.data()[i*8]==game.world.mapId()) {_waitingSlot=byte(i);break;}
        if (_waitingSlot==255) {
            const bool next=first!=0;_memberMode=0;
            if (next?!_inn.closeService():!_inn.resumeWelcome()) return kWorldConversationUnsupported;
            return innText(archive,0x2b44+(next?0x4a0:0x512),frame)?kWorldConversationHandled:kWorldConversationUnsupported;
        }
        Common::Array<byte> raw;Common::String body;if (!archive.read("NA/SRTN.NPC.TALK",raw) || _inn.offer()<6 ||
                !plainDialogueBody(raw,0x2b44+0x559,body)) return kWorldConversationUnsupported;
        _pickupFee=READ_LE_UINT16(raw.data()+_inn.offer()-6);body+=waitingName(game);body+='?';_memberMode=1;
        return memberBody(archive,body,frame)?kWorldConversationHandled:kWorldConversationUnsupported;
    }
    WorldConversationResult memberInput(const Common::Event &event,HdvArchive &archive,NewGame &game,HgrFrame &frame) {
        if (_stage==18) {
            if (!_window.waiting() || !_window.handle(event) || !_window.paint(frame)) return kWorldConversationUnsupported;
            if (!_window.waiting()) _stage=_memberMode==1?15:16;return kWorldConversationHandled;
        }
        if (_stage==17) {
            const PartySelectionResult selected=_selection.handle(event,game.party);
            if (selected==kPartySelectionInvalid) return kWorldConversationUnsupported;
            if (selected==kPartySelectionPending) return _selection.paint(archive,game.party,frame)?kWorldConversationHandled:kWorldConversationUnsupported;
            if (selected==kPartySelectionAccepted) {
                if (InnMembership::leave(archive,game,_selection.selected())!=kInnMemberApplied) return kWorldConversationUnsupported;
                restore(frame);return kWorldConversationFinished;
            }
            _memberMode=0;if (!_inn.closeService()) return kWorldConversationUnsupported;
            return innText(archive,0x2b44+0x4a0,frame)?kWorldConversationHandled:kWorldConversationUnsupported;
        }
        const bool yes=(byte(event.kbd.ascii)&95)=='Y';
        if (_memberMode==1) {
            if (!yes) return pickupOffer(archive,game,frame,uint(_waitingSlot)+1);
            const Common::String name=waitingName(game);const InnMembershipResult result=InnMembership::pickup(game,_pickupFee,_waitingSlot);_memberMode=0;
            if (result==kInnMemberFullParty) {
                Common::Array<byte> raw;Common::String body;if (!_inn.resumeWelcome() || !archive.read("NA/SRTN.NPC.TALK",raw) || !plainDialogueBody(raw,0x2b44+0x573,body)) return kWorldConversationUnsupported;
                return innBody(archive,name+body,frame)?kWorldConversationHandled:kWorldConversationUnsupported;
            }
            if (result==kInnMemberPoor) {
                if (!_inn.closeService()) return kWorldConversationUnsupported;
                return innText(archive,0x2b44+0x421,frame)?kWorldConversationHandled:kWorldConversationUnsupported;
            }
            if (result!=kInnMemberApplied || !_inn.resumeWelcome()) return kWorldConversationUnsupported;
            Common::Array<byte> raw;Common::String bill,again;if (!archive.read("NA/SRTN.NPC.TALK",raw) || !plainDialogueBody(raw,0x2b44+0x5d1,bill) ||
                    !plainDialogueBody(raw,0x2b44+0x5e1,again)) return kWorldConversationUnsupported;
            return innBody(archive,bill+Common::String::format("%u",_pickupFee)+again,frame)?kWorldConversationHandled:kWorldConversationUnsupported;
        }
        if (_memberMode!=2) return kWorldConversationUnsupported;
        if (!yes || game.inn.firstFree()<0) {
            _memberMode=0;if (!_inn.closeService()) return kWorldConversationUnsupported;
            return innText(archive,0x2b44+(yes?0x615:0x4a0),frame)?kWorldConversationHandled:kWorldConversationUnsupported;
        }
        if (!_selection.begin(game.party,false) || !_selection.paint(archive,game.party,frame)) return kWorldConversationUnsupported;
        _memberMode=3;_stage=17;return kWorldConversationHandled;
    }
    WorldConversationResult serviceInput(const Common::Event &event,HdvArchive &archive,NewGame &game,HgrFrame &frame) {
        if (_stage==8) {
            if (!_window.waiting() || !_window.handle(event) || !_window.paint(frame)) return kWorldConversationUnsupported;
            if (!_window.waiting()) {
                _stage=byte(_temple.phase()+4);
                if (_temple.phase()==1 && !servicePrices(archive,frame)) return kWorldConversationUnsupported;
                if (_temple.phase()==2 && ((!_selection.active() && !_selection.begin(game.party,true)) || !_selection.paint(archive,game.party,frame))) return kWorldConversationUnsupported;
            }
            return kWorldConversationHandled;
        }
        if (_stage==7) {restore(frame);return kWorldConversationFinished;}
        const Common::KeyCode key=event.kbd.keycode;
        if (_stage==5) {
            byte choice;
            if (key==Common::KEYCODE_ESCAPE) choice=0;
            else if (event.kbd.ascii=='1' || event.kbd.ascii=='2') choice=byte(event.kbd.ascii-'0');
            else return kWorldConversationHandled;
            if (!_temple.choose(choice,game)) return kWorldConversationUnsupported;
        } else if (_stage==6) {
            if (!_selection.active() && !_selection.begin(game.party,true)) return kWorldConversationUnsupported;
            const PartySelectionResult result=_selection.handle(event,game.party);
            if (result==kPartySelectionInvalid) return kWorldConversationUnsupported;
            if (result==kPartySelectionPending) return _selection.paint(archive,game.party,frame)?kWorldConversationHandled:kWorldConversationUnsupported;
            if (!_temple.target(result==kPartySelectionCancelled?255:_selection.selected(),game)) return kWorldConversationUnsupported;
        } else return kWorldConversationUnsupported;
        if (_temple.phase()==2) {
            if (!_selection.begin(game.party,true) || !serviceText(archive,0x8394+0x13d,true,frame) ||
                    (_stage==6 && !_selection.paint(archive,game.party,frame))) return kWorldConversationUnsupported;
            return kWorldConversationHandled;
        }
        uint offset=0;bool combat=false;
        switch (_temple.result()) {
        case kTemplePoor:offset=0x71d2+0x32a;break;
        case kTempleCancelled:offset=0x71d2+0x34f;break;
        case kTempleNotDead:offset=0x8394+0x14e;combat=true;break;
        case kTempleResurrected:offset=0x8394+0x165;combat=true;break;
        default:break;
        }
        if (!serviceText(archive,offset,combat,frame)) return kWorldConversationUnsupported;
        if (_sounds && (_temple.result()==kTempleHealed || _temple.result()==kTempleResurrected)) _sounds->push_back(kSpeakerSelection);
        return kWorldConversationHandled;
    }
    bool serviceText(HdvArchive &archive,uint offset,bool combat,HgrFrame &frame) {
        Common::String text;
        if (offset) {
            Common::Array<byte> raw;
            if (!archive.read(combat?"NA/SRTN.COMBAT.S":"NA/SRTN.NPC.TALK",raw) || !plainDialogueBody(raw,offset,text)) return false;
        }
        HgrFrame background=frame;if (!OpeningBackdrop::renderTempleFrame(background)) return false;
        Common::Array<byte> bytes;encoded(text,bytes);TextWindow window;
        if (!window.begin(archive,bytes.data(),bytes.size(),21,18,14,22,8,&background) || !window.paint(background)) return false;
        _window=window;_hasWindow=true;_stage=window.waiting()?8:byte(_temple.phase()+4);frame=background;
        return _stage!=5 || servicePrices(archive,frame);
    }
    bool servicePrices(HdvArchive &archive,HgrFrame &frame) const {
        GlyphFont font;if (!font.load(archive,1)) return false;
        const uint16 prices[2]={_temple.healingPrice(),_temple.resurrectionPrice()};
        for (uint line=0;line<2;++line) {
            const Common::String text=Common::String::format("%5u",prices[line]);
            for (uint i=0;i<text.size();++i) if (!frame.drawGlyph(font,byte(text[i])|128,22+i,19+line*2,0)) return false;
        }
        return true;
    }
    WorldConversationResult refuseStrip(byte id,byte &message) { _stage=4;_refused=true;message=id;return kWorldConversationMessage; }
    void restore(HgrFrame &frame) {const bool refresh=_worldRefresh;frame=_underlay;frame.clearTextSpan(7,23,25);*this=WorldConversation();_worldRefresh=refresh;}
    static void encoded(const Common::String &text,Common::Array<byte> &bytes) {
        bytes.clear();for (uint i=0;i<text.size();++i) bytes.push_back(text[i]=='\n'?0x8d:byte(text[i])|128);
    }
    bool show(HdvArchive &archive,const Common::String &text,HgrFrame &frame,Enhancements *events=nullptr,GameplayRandom *random=nullptr,NewGame *game=nullptr) {
        HgrFrame background=frame;if (!OpeningBackdrop::renderConversationFrame(background)) return false;
        Common::Array<byte> bytes;encoded(text,bytes);TextWindow window;
        if (!window.begin(archive,bytes.data(),bytes.size(),25,13,1,12,11,&background) || !window.paint(background)) return false;
        _window=window;_hasWindow=true;_stage=2;frame=background;return settle(archive,frame,events,random,game);
    }
    bool settle(HdvArchive &archive,HgrFrame &frame,Enhancements *events=nullptr,GameplayRandom *random=nullptr,NewGame *game=nullptr) {
        if (_hasDialogue && _conversation.worldCallbacksPending()) {if (!game || !applyCallbacks(archive,*game)) return false;}
        if (_window.waiting()) {_stage=2;return true;}
        if (!_window.ended()) return false;
        if (_hasDialogue && events && !_conversation.recordSpokenReply(*events)) return false;
        if (_hasDialogue && _conversation.paymentPhase()) {
            if(_conversation.paymentPresentationPending()) {Common::String text;if(!_conversation.paymentPresentation(archive,text)) return false;return show(archive,text,frame,events,random,game);}
            _stage=3;return input(archive,frame);
        }
        if (_hasDialogue && _conversation.trainingPending()) {_stage=3;return true;}
        if (!_closing) {_stage=3;return input(archive,frame);}
        if (!_endShown) {
            _endShown=true;
            if (_hasDialogue) {
                const ConversationResult result=_conversation.closing(random);
                if (_conversation.worldCallbacksPending()) {if (!game || !applyCallbacks(archive,*game)) return false;}
                if (result!=kConversationReply && result!=kConversationNoReply) return false;
                Common::Array<byte> bytes;encoded(_conversation.reply(),bytes);
                if (!_window.append(bytes.data(),bytes.size()) || !_window.paint(frame)) return false;
                if (_window.waiting()) {_stage=2;return true;}
            }
        }
        if (_hasDialogue && events && !_conversation.recordSpokenReply(*events)) return false;
        if (!_footerShown) {
            Common::Array<byte> raw,footer;if (!archive.read("NA/SRTN.NPC.TALK",raw) || raw.size()!=32831) return false;
            uint i=0xf9a;while (i<raw.size() && raw[i]) footer.push_back(raw[i++]);if (i==raw.size()) return false;
            _footerShown=true;if (!_window.append(footer.data(),footer.size()) || !_window.paint(frame)) return false;
            if (_window.waiting()) {_stage=2;return true;}
        }
        _stage=4;return true;
    }
    bool input(HdvArchive &archive,HgrFrame &frame) const {
        GlyphFont font;if (!font.load(archive,1)) return false;
        for (uint row=19;row<22;++row) if (!frame.clearTextSpan(25,row,13)) return false;
        for (uint i=0;i<_query.size();++i) if (!frame.drawGlyph(font,byte(_query[i])|128,25+i%13,19+i/13,0)) return false;
        if (!frame.clearTextSpan(25,22,13)) return false;
        if (_hasDialogue && !_conversation.paymentPhase() && !_conversation.trainingPending()) {
            const char *labels[]={"Tab: normal","Tab: yell","Tab: whisper"};const char *mode=labels[_conversation.context().voice];
            for (uint i=0;mode[i];++i) if (!frame.drawGlyph(font,byte(mode[i])|128,25+i,22,0)) return false;
        }
        return frame.drawGlyph(font,0xa0,25+_query.size()%13,19+_query.size()/13,127);
    }
    byte _stage,_type,_map,_slot;
    bool _closing,_endShown,_footerShown,_hasDialogue,_hasWindow,_refused;
    Common::String _query;Conversation _conversation;TextWindow _window;HgrFrame _underlay;TempleService _temple;InnService _inn;
    byte _memberMode,_waitingSlot;uint16 _pickupFee;PartySelection _selection;byte _vendorRole;bool _merchantSale;uint16 _merchantIndex;byte _trainingMember;bool _worldRefresh;SoundEvents *_sounds;bool _effectsEnabled;byte _speechCursor;
};
} }
#endif
