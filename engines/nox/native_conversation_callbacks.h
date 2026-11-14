/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_CONVERSATION_CALLBACKS_H
#define NOX_NATIVE_CONVERSATION_CALLBACKS_H
#include "common/scummsys.h"
namespace Nox { namespace Native {
// Typed callback transaction over canonical records. See the explicit
// main/auxiliary object reconciliation in the source contract.
class ConversationCallbacks {
public:
    static bool requested(byte map,const byte *fields) {
        return fields && ((fields[12]&32) || (fields[42]&128) || (fields[46]&178) || (map==68 && (fields[58]&32)));
    }
    static bool apply(byte map,byte *fields,byte *objects,const byte *authored,bool &redraw) {
        if (!fields || !objects || (map==68 && (fields[58]&32) && !authored)) return false;
        const bool changed=requested(map,fields);
        fields[12]&=223;
        if (map==68 && (fields[58]&32)) memcpy(objects,authored,12);
        if ((fields[42]&128) || (fields[46]&2)) {
            fields[42]&=127;objects[3]=254;fields[46]&=253;
        }
        if (fields[46]&128) {objects[3]=objects[71]=36;fields[46]&=127;}
        if (fields[46]&16) {objects[71]=36;fields[46]&=239;}
        if (fields[46]&32) {objects[147]=36;fields[46]&=223;}
        redraw=changed;return true;
    }
};
} }
#endif
