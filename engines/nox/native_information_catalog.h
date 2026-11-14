/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_INFORMATION_CATALOG_H
#define NOX_NATIVE_INFORMATION_CATALOG_H
#include "nox/native_world_presentation.h"
#include "nox/native_lz.h"
namespace Nox { namespace Native {
struct InformationText {
    Common::String text;bool window;
    InformationText() : window(false) {}
};
// Authored text/condition data only. Decoded quest instruction bytes are never
// retained or executed: only its51 descriptions and field/bit quadruples survive.
class InformationCatalog {
public:
    InformationCatalog() : _ready(false) {memset(_conditions,0,sizeof(_conditions));memset(_regions,0,sizeof(_regions));memset(_regionText,0,sizeof(_regionText));}
    bool load(HdvArchive &archive) {
        Common::Array<byte> game,inventory,decoded;
        if (!archive.read("NA/GAME",game) || game.size()!=13824 || !archive.read("NA/SRTN.INVENTORY",inventory) || inventory.size()!=67130) return false;
        InformationCatalog next;
        if (!plain(game,0x1508,64,next._defaultDeepLocation)) return false;
        const uint look=READ_LE_UINT16(game.data()+5+8*8),help=READ_LE_UINT16(game.data()+5+9*8);
        if (look+0x370>inventory.size() || help+0x211>inventory.size() || !plain(inventory,help+0x88,0x185,next._help)) return false;
        next._special.resize(0xfe);memcpy(next._special.data(),inventory.data()+look+0x272,0xfe);
        memcpy(next._regions,inventory.data()+look+0x255,21);
        for (uint i=0;i<4;++i) next._regionText[i]=READ_LE_UINT16(inventory.data()+look+0x26a+i*2);
        // The original named-object reader uses the same document table as Use.
        // Copy text/table data, not any original command program.
        next._documents.resize(inventory.size()-0xd4b3);memcpy(next._documents.data(),inventory.data()+0xd4b3,next._documents.size());
        const byte *packet=inventory.data()+0xca01;
        if (READ_BE_UINT16(packet+0x815)!=0x0e5b || !decodeReversePacket(packet,0x815,0x0e5b,decoded) || decoded[0xd8e+204]!=255) return false;
        memcpy(next._conditions,decoded.data()+0xd8e,204);
        for (uint i=1;i<204;i+=2) if (next._conditions[i]>7) return false;
        next._quests.resize(0xd8e-0x26e);memcpy(next._quests.data(),decoded.data()+0x26e,next._quests.size());
        uint cursor=0;Common::String item;for (uint i=0;i<51;++i) {if (!next.entry(next._quests,i,item)) return false;}
        for (uint i=0;i<next._quests.size();++i) cursor+=next._quests[i]==0;
        if (cursor!=51) return false;
        static const byte families[]={0,2,3,4,6,7,8};
        for (uint i=0;i<ARRAYSIZE(families);++i) {
            Common::Array<byte> encoded;
            if (!archive.read(Common::String::format("NA/DATA.LOOK%u",uint(families[i])),encoded) || encoded.size()<4 || encoded[0]!=234) return false;
            const uint size=READ_BE_UINT16(encoded.data()+encoded.size()-2);
            if (!decodeReversePacket(encoded.data()+1,encoded.size()-3,size,next._look[i])) return false;
            uint count=0;for (uint j=0;j<next._look[i].size();++j) count+=next._look[i][j]==0;
            if (count!=256 || !entry(next._look[i],255,item)) return false;
        }
        next._ready=true;*this=next;return true;
    }
    bool help(Common::String &out) const {
        if (!_ready) return false;out=_help;
        out+="\r\rComfort Edition\rTab: party roster.\rTab/Space again: inventory.\rIn combat, Tab: inventory.\rN: reorder party.\rV: cycle sound mode.";return true;
    }
    bool location(const World &world,Common::String &out) const {
        if (!_ready || !world.ready()) return false;
        const MapDefinition *map=world.mapDefinition();if (!map) return false;
        // Three valid directory records have no authored location string.
        // Explicit CE category labels replace an unrelated retained scratch name.
        if (map->metadata.size()==23) {
            if (world.mapType()==6 && world.mapId()==2) {out="Town";return true;}
            if (world.mapType()==7 && (world.mapId()==4 || world.mapId()==5)) {out="Castle";return true;}
            return false;
        }
        if (world.mapId()!=97) return plain(map->metadata,23,map->metadata.size(),out);
        byte x,y;
        if (world.locationAnchor(x,y)) {
            uint at=23;
            while (at<map->metadata.size() && map->metadata[at]!=255) {
                if (map->metadata.size()-at<3) return false;
                const byte entryX=map->metadata[at++],entryY=map->metadata[at++];Common::String name;
                if (!plain(map->metadata,at,map->metadata.size(),name)) return false;
                if (entryX==x && entryY==y) {out=name;return true;}
                at+=name.size()+1;
            }
        }
        // Old native saves have no historical entrance identity. Explicitly
        // migrate to the authored resident default rather than guess a name.
        out=_defaultDeepLocation;return true;
    }
    bool generic(byte type,byte tile,Common::String &out) const {
        if (!_ready) return false;if (type==5) type=6;
        static const byte families[]={0,2,3,4,6,7,8};
        for (uint i=0;i<ARRAYSIZE(families);++i) if (families[i]==type) return entry(_look[i],tile,out);
        return false;
    }
    bool quests(const byte *fields,uint size,Common::String &out) const {
        if (!_ready || !fields || size!=256) return false;Common::String body;
        for (uint i=0;i<51;++i) {
            const byte *condition=_conditions+i*4;
            if (!(fields[condition[0]]&(1U<<condition[1])) || (fields[condition[2]]&(1U<<condition[3]))) continue;
            Common::String text;if (!entry(_quests,i,text)) return false;
            body+="\r--\r";body+=text;
        }
        out=body;return true;
    }
    bool look(const World &world,const WorldScene &scene,byte appearance,byte direction,InformationText &out) const {
        if (!_ready || !world.ready() || direction>3) return false;
        static const byte cells[]={76,110,94,92};const byte base=cells[direction];
        const Common::Array<byte> *tables=world.currentTables();if (!tables || tables->size()!=1792) return false;
        const byte object=scene.objects.lookup[base];
        if (object!=255 && (*tables)[1024+object+3]==57) {
            // ASL/TAY in the source intentionally masks the selector to7 bits.
            const byte document=(*tables)[1024+object+7]&127;
            const uint at=READ_LE_UINT16(_documents.data()+document*2);
            if (at<0xd4b3) return false;
            InformationText result;result.window=true;
            if (!plain(_documents,at-0xd4b3,256,result.text)) return false;out=result;return true;
        }
        const byte cell=appearance==119 && direction==1?127:appearance==119 && direction==2?95:base;
        const byte terrain=scene.actors.terrain[cell],actor=scene.actors.lookup[cell],map=world.mapId(),sector=world.coordinateSector();
        InformationText result;uint special=0;
        if (!map) {
            if (sector==155 && terrain==13) special=0x9934;
            else if (actor==255 && (terrain==1 || (terrain>=67 && terrain<74))) {
                uint region=0;
                for (int i=20;i>=0;--i) {
                    if (_regions[i]==255) {++region;continue;}
                    if (_regions[i]==sector) {if (region>=4) return false;special=_regionText[region];break;}
                }
            }
        }
        if (!special && terrain>=216 && terrain<220) {
            if (map==11) special=0x98bc;else if (map==71) special=0x98d2;
            else if (map==58) special=0x9917;else if (map==24) special=0x9928;
        }
        if (!special && map==71 && (terrain==8 || terrain==220)) special=0x98eb;
        if (!special && map==73 && terrain==144) special=0x9903;
        if (!special && map==69 && (terrain==142 || terrain==66)) special=terrain==142?0x994c:0x995e;
        if (special) {if (special<0x9872 || !plain(_special,special-0x9872,_special.size(),result.text)) return false;out=result;return true;}
        byte tile=terrain;
        if (actor!=255) {
            const byte kind=scene.actors.kinds[cell]&7;if (kind!=0 && kind!=4) return false;
            const uint bank=kind==4?1280:768;if (uint(actor)+2>=256) return false;tile=(*tables)[bank+actor+2];
        } else {
            const byte slot=scene.objects.lookup[cell];
            if (slot!=255 && (*tables)[1024+slot+3]!=55) tile=(*tables)[1024+slot+2];
        }
        if (!generic(world.mapType(),tile,result.text)) return false;out=result;return true;
    }
private:
    static bool plain(const Common::Array<byte> &data,uint at,uint limit,Common::String &out) {
        if (at>=data.size()) return false;Common::String result;
        for (uint i=0;i<limit && at+i<data.size();++i) {
            const byte raw=data[at+i],c=raw&127;if (!raw) {out=result;return true;}
            if (c<32 && c!=13) return false;result+=char(c);
        }
        return false;
    }
    static bool entry(const Common::Array<byte> &data,uint index,Common::String &out) {
        uint at=0;
        for (uint i=0;i<index;++i) {while (at<data.size() && data[at]) ++at;if (at==data.size()) return false;++at;}
        return plain(data,at,data.size(),out);
    }
    bool _ready;byte _conditions[204],_regions[21];uint16 _regionText[4];
    Common::String _help,_defaultDeepLocation;Common::Array<byte> _quests,_special,_documents,_look[7];
};
} }
#endif
