/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_PARTY_ORDER_H
#define NOX_NATIVE_PARTY_ORDER_H
#include "nox/native_party_selection.h"
#include "nox/native_equipment_ownership.h"
namespace Nox { namespace Native {
class PartyOrder {
public:
    static bool exchange(const EquipmentOwnership &ownership,byte first,byte second,Party &party,Inventory &inventory) {
        if (!party.ready() || first>=party.memberCount() || second>=party.memberCount()) return false;
        byte a,ac,b,bc;if (!ownership.masks(first,a,ac) || !ownership.masks(second,b,bc)) return false;
        Party next=party;Inventory items=inventory;
        if (!next.exchangeMembers(first,second) || !items.exchangeOwners(a,b)) return false;
        party=next;inventory=items;return true;
    }
};
enum PartyOrderResult {kPartyOrderPending,kPartyOrderClosed,kPartyOrderUnsupported};
// Two source8600 selections, with source30F8=1: arrows include dead members;
// shifted-number aliases retain the existing living-only selector semantics.
class PartyOrderInput {
public:
    PartyOrderInput() : _phase(0),_first(0) {}
    bool active() const {return _phase!=0;}
    byte phase() const {return _phase;}
    bool begin(HdvArchive &archive,const Party &party,HgrFrame &frame) {
        if (active()) return false;
        PartyOrderInput next;next._underlay=frame;next._phase=1;
        if (!next._selection.begin(party,true) || !next.paint(archive,party,frame)) return false;
        *this=next;return true;
    }
    PartyOrderResult handle(const Common::Event &event,HdvArchive &archive,Party &party,Inventory &inventory,HgrFrame &frame) {
        if (!active()) return kPartyOrderUnsupported;
        const PartySelectionResult result=_selection.handle(event,party);
        if (result==kPartySelectionInvalid) return kPartyOrderUnsupported;
        if (result==kPartySelectionCancelled) {close(frame);return kPartyOrderClosed;}
        if (result==kPartySelectionAccepted) {
            if (_phase==1) {
                _first=_selection.selected();_phase=2;
                if (!_selection.begin(party,true)) return kPartyOrderUnsupported;
            } else {
                EquipmentOwnership ownership;
                if (!ownership.load(archive) || !PartyOrder::exchange(ownership,_first,_selection.selected(),party,inventory)) return kPartyOrderUnsupported;
                close(frame);return kPartyOrderClosed;
            }
        }
        return paint(archive,party,frame)?kPartyOrderPending:kPartyOrderUnsupported;
    }
private:
    void close(HgrFrame &frame) {frame=_underlay;*this=PartyOrderInput();}
    bool paint(HdvArchive &archive,const Party &party,HgrFrame &frame) const {
        HgrFrame next=_underlay;GlyphFont font;
        if (!font.load(archive,1) || !_selection.paint(archive,party,next) || !next.clearTextSpan(7,23,25)) return false;
        const char *prompt=_phase==1?"Exchange which member?":"With which member?";
        for (uint i=0;prompt[i];++i) if (!next.drawGlyph(font,byte(prompt[i]),7+i,23,0)) return false;
        frame=next;return true;
    }
    byte _phase,_first;PartySelection _selection;HgrFrame _underlay;
};
} }
#endif
