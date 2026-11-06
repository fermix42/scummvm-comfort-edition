/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_NPC_PATH_H
#define NOX_NATIVE_NPC_PATH_H
#include "nox/native_world_story.h"
#include "nox/native_graphics.h"
#include "nox/native_random.h"
#include "common/rect.h"
namespace Nox { namespace Native {
enum NpcPathStatus { kNpcPathSearching,kNpcPathComplete,kNpcPathError,kNpcPathUnsupported,kNpcPathIdle };
struct NpcPathNode {
    byte parent,fromX,fromY,x,y;
    NpcPathNode() : parent(170),fromX(170),fromY(170),x(170),y(170) {}
};
struct NpcPathFrontier {
    byte distance,node;
    NpcPathFrontier() : distance(170),node(170) {}
    NpcPathFrontier(byte d, byte n) : distance(d),node(n) {}
};
// Cooperative route search over owned48x48 cells with the recovered bounded
// greedy frontier and retained RNG. An explicit reference-only data profile
// supports isolated original-rule comparisons. State holds nodes, goals and
// data records; there are no machine registers, addresses or instruction steps.
class NpcPathSearch {
public:
    NpcPathSearch() : _type(255),_status(kNpcPathIdle),_fresh(false),_zeroGoalVisitProfile(false),_rootX(0),_rootY(0),_targetX(0),_targetY(0),_current(0),_currentX(0),_currentY(0),_frontierCount(0) {
        memset(_ranges,0,4); memset(_visited,0,1024); memset(_ownedVisited,0,2304); memset(_route,170,256);
    }
    bool configure(HdvArchive &archive, byte type) {
        if (_type != 255) return false;
        Common::Array<byte> game,footer; MapAssetRange range;
        if (!archive.read("NA/GAME",game) || !resolveMapAssets(game,type,range) ||
            !archive.readRange(range.resource,range.offset + range.size,72,footer)) return false;
        memcpy(_ranges,footer.data() + 22,4); _type = type; return true;
    }
    bool seedWorkspaceTails(byte visitedTail, byte routeTail, byte frontierNodeTail) {
        if (_status == kNpcPathSearching) return false;
        _visited[1023] = visitedTail; _route[255] = routeTail; _frontier[126].node = frontierNodeTail; return true;
    }
    bool start(byte originX, byte originY, byte destinationX, byte destinationY, bool zeroGoalVisitProfile = false) {
        if (_type == 255 || _status == kNpcPathSearching || (originX == destinationX && originY == destinationY)) return false;
        NpcPathSearch next(*this); next._status = kNpcPathSearching; next._fresh = true; next._zeroGoalVisitProfile = zeroGoalVisitProfile;
        next._rootX = originX; next._rootY = originY; next._targetX = destinationX; next._targetY = destinationY;
        if (!zeroGoalVisitProfile && (originX >= 48 || originY >= 48 || destinationX >= 48 || destinationY >= 48)) return false;
        next._current = 0; next._currentX = originX; next._currentY = originY; next._frontierCount = 1; next._nodes.clear();
        const byte visitedTail = next._visited[1023],routeTail = next._route[255],frontierTail = next._frontier[126].node;
        memset(next._visited,0,1024); memset(next._ownedVisited,0,2304); memset(next._route,170,256);
        for (uint i = 0; i < 127; ++i) next._frontier[i] = NpcPathFrontier();
        // The data clear excludes each range's final byte. Preserve those
        // opaque workspace fields rather than manufacturing bootstrap memory.
        next._visited[1023] = visitedTail; next._route[255] = routeTail; next._frontier[126].node = frontierTail;
        NpcPathNode root; root.parent = 0; root.fromX = originX; root.fromY = originY; root.x = root.y = 0; next._nodes.push_back(root);
        uint index; if (!next.visitIndex(originX,originY,index)) return false; next.visit(index) = 1;
        *this = next; return true;
    }
    NpcPathStatus advance(const WorldStory &window, GameplayRandom &random) {
        if (_status != kNpcPathSearching || !window.state()) return kNpcPathUnsupported;
        NpcPathSearch next(*this); GameplayRandom after = random;
        const NpcPathStatus result = next.expand(window.state()->terrain,after);
        if (result == kNpcPathUnsupported) return result;
        *this = next; random = after; return result;
    }
    NpcPathStatus status() const { return _status; }
    uint nodeCount() const { return _nodes.size(); } byte frontierCount() const { return _frontierCount; }
    byte currentNode() const { return _current; } byte currentX() const { return _currentX; } byte currentY() const { return _currentY; }
    const NpcPathNode *node(uint index) const { return index < _nodes.size() ? &_nodes[index] : nullptr; }
    const NpcPathFrontier *frontier(uint index) const { return index < _frontierCount ? &_frontier[index] : nullptr; }
    const byte *visited() const { return _zeroGoalVisitProfile ? _visited : _ownedVisited; }
    uint visitedSize() const { return _zeroGoalVisitProfile ? 1024 : 2304; }
    bool referenceVisitProfile() const { return _zeroGoalVisitProfile; } const byte *route() const { return _route; }
    bool routePoints(Common::Array<Common::Point> &output) const {
        if (_status != kNpcPathComplete) return false;
        Common::Array<Common::Point> reverse; uint id = _current;
        for (uint guard = 0; guard < _nodes.size(); ++guard) {
            if (!id) { reverse.push_back(Common::Point(_rootX,_rootY)); break; }
            if (id >= _nodes.size()) return false;
            reverse.push_back(Common::Point(_nodes[id].x,_nodes[id].y)); id = _nodes[id].parent;
        }
        if (reverse.empty() || reverse.back() != Common::Point(_rootX,_rootY)) return false;
        output.clear(); for (uint i = reverse.size(); i; --i) output.push_back(reverse[i - 1]); return true;
    }
    bool save(Common::WriteStream &output) const {
        if (_type == 255 || _status > kNpcPathError || !_nodes.size()) return false;
        output.writeUint32BE(MKTAG('N','P','S','R')); output.writeUint16LE(2); output.writeByte(_type); output.writeByte(byte(_status)); output.writeByte(_fresh); output.writeByte(_zeroGoalVisitProfile); output.writeByte(_frontier[126].node); output.writeByte(_visited[1023]);
        output.writeByte(_rootX); output.writeByte(_rootY); output.writeByte(_targetX); output.writeByte(_targetY); output.writeByte(_current); output.writeByte(_currentX); output.writeByte(_currentY);
        output.writeUint16LE(uint16(_nodes.size())); output.writeByte(_frontierCount); output.write(visited(),visitedSize()); output.write(_route,256);
        for (uint i = 0; i < _nodes.size(); ++i) { const NpcPathNode &n = _nodes[i]; output.writeByte(n.parent); output.writeByte(n.fromX); output.writeByte(n.fromY); output.writeByte(n.x); output.writeByte(n.y); }
        for (uint i = 0; i < _frontierCount; ++i) { output.writeByte(_frontier[i].distance); output.writeByte(_frontier[i].node); }
        return !output.err();
    }
    bool load(Common::SeekableReadStream &input, HdvArchive &archive) {
        if (input.readUint32BE() != MKTAG('N','P','S','R')) return false;
        const uint16 version = input.readUint16LE(); if (version != 1 && version != 2) return false;
        NpcPathSearch next; const byte type = input.readByte(); if (!next.configure(archive,type)) return false;
        const byte status = input.readByte(),fresh = input.readByte(),zeroGoals = input.readByte(); const byte frontierTail = input.readByte(),referenceTail = version == 2 ? input.readByte() : 0; if (status > kNpcPathError || fresh > 1 || zeroGoals > 1) return false;
        next._status = NpcPathStatus(status); next._fresh = fresh != 0; next._zeroGoalVisitProfile = zeroGoals != 0; if (version == 1 && !next._zeroGoalVisitProfile) return false; next._rootX = input.readByte(); next._rootY = input.readByte(); next._targetX = input.readByte(); next._targetY = input.readByte(); next._current = input.readByte(); next._currentX = input.readByte(); next._currentY = input.readByte();
        const uint count = input.readUint16LE(); next._frontierCount = input.readByte();
        if (!count || count > 256 || !next._frontierCount || next._frontierCount > 127 || next._current >= count ||
            input.size() - input.pos() != int64(next.visitedSize() + 256 + count * 5 + next._frontierCount * 2) || input.read(next._zeroGoalVisitProfile ? next._visited : next._ownedVisited,next.visitedSize()) != next.visitedSize() || input.read(next._route,256) != 256) return false;
        for (uint i = 0; i < count; ++i) {
            NpcPathNode n; n.parent = input.readByte(); n.fromX = input.readByte(); n.fromY = input.readByte(); n.x = input.readByte(); n.y = input.readByte();
            if ((!i && (n.parent || n.fromX != next._rootX || n.fromY != next._rootY || n.x || n.y)) ||
                (i && (n.parent >= i || n.fromX != (n.parent ? next._nodes[n.parent].x : next._rootX) || n.fromY != (n.parent ? next._nodes[n.parent].y : next._rootY)))) return false;
            next._nodes.push_back(n);
        }
        for (uint i = 0; i < next._frontierCount; ++i) {
            next._frontier[i].distance = input.readByte(); next._frontier[i].node = input.readByte();
            if (next._frontier[i].node >= count && next._frontier[i].distance != 170 && next._frontier[i].distance != 254) return false;
        }
        if (next._frontierCount == 127 && next._frontier[126].node != frontierTail) return false;
        next._frontier[126].node = frontierTail;
        if (version == 2) { if (next._zeroGoalVisitProfile && next._visited[1023] != referenceTail) return false; next._visited[1023] = referenceTail; }
        if (input.err()) return false; *this = next; return true;
    }
private:
    bool visitIndex(byte x, byte y, uint &index) const {
        if (!_zeroGoalVisitProfile) {
            if (x >= 48 || y >= 48) return false;
            index = uint(y) * 48 + x; return true;
        }
        const byte row = byte(y - 7);
        // Preserve the bounded original coordinate hash, including authored
        // X aliases and the source-default zero-goal arithmetic profile.
        // Nonzero-goal borrowed-layout contexts remain explicitly unsupported;
        // there is no machine-memory escape to resolve them.
        if (row < 32) index = uint(row) * 32 + byte(x - 8);
        else {
            if (!_zeroGoalVisitProfile) return false;
            index = (row < 64 ? uint(row - 32) / 8 : 0) + byte(x - 8);
        }
        return index < 1280;
    }
    byte &visit(uint index) { return !_zeroGoalVisitProfile ? _ownedVisited[index] : index < 1024 ? _visited[index] : _route[index - 1024]; }
    bool allowed(byte tile) const { return (tile >= _ranges[0] && tile < _ranges[1]) || (tile >= _ranges[2] && tile < _ranges[3]); }
    static byte distance(byte x, byte y, byte targetX, byte targetY) {
        return byte((x >= targetX ? x - targetX : targetX - x) + (y >= targetY ? y - targetY : targetY - y));
    }
    NpcPathStatus finish() {
        const NpcPathNode &found = _nodes[_current]; _route[2] = _targetX; _route[3] = _targetY; _route[4] = found.fromX; _route[5] = found.fromY;
        byte ancestor = found.parent; uint output = 6,pairs = 2;
        for (uint guard = 0; guard < 256; ++guard) {
            if (ancestor >= _nodes.size()) return kNpcPathUnsupported;
            if (output + 1 >= 256) { if (_zeroGoalVisitProfile) return kNpcPathUnsupported; _status = kNpcPathComplete; return _status; }
            const NpcPathNode &n = _nodes[ancestor]; ancestor = n.parent; _route[output] = n.fromX; _route[output + 1] = n.fromY;
            if (n.fromX == _rootX && n.fromY == _rootY) { _route[0] = byte(pairs * 2); _status = kNpcPathComplete; return _status; }
            if (n.fromY == 255) { _status = kNpcPathError; return _status; }
            ++pairs; output += 2;
        }
        return kNpcPathUnsupported;
    }
    NpcPathStatus expand(const byte *terrain, GameplayRandom &random) {
        if (_current >= _nodes.size() || !_frontierCount) return kNpcPathUnsupported;
        NpcPathNode current = _nodes[_current]; current.x = _currentX; current.y = _currentY; const int base = int(current.y) * 48 + current.x;
        const int offsets[4] = {-48,48,1,-1}; byte blocked[4]; bool used[4] = {};
        for (uint i = 0; i < 4; ++i) {
            const int x = int(current.x) + (i == 2 ? 1 : i == 3 ? -1 : 0),y = int(current.y) + (i == 1 ? 1 : i == 0 ? -1 : 0);
            if (!_zeroGoalVisitProfile && (x < 0 || x >= 48 || y < 0 || y >= 48)) { blocked[i] = 1; continue; }
            const int index = base + offsets[i]; if (index < 0 || index >= 2304) return kNpcPathUnsupported; blocked[i] = !allowed(terrain[index]);
        }
        byte threshold = _frontier[0].distance; _frontier[0].distance = 254;
        int insertion = _frontierCount - 1; uint reserve = _fresh ? 0 : 1,processed = 0;
        while (processed < 4) {
            uint direction = 0;
            if (processed >= 3) { while (direction < 4 && used[direction]) ++direction; }
            else {
                byte roll; if (!random.next(1,255,roll)) return kNpcPathUnsupported;
                direction = roll < 63 ? 0 : roll < 116 ? 1 : roll < 191 ? 2 : 3;
                if (used[direction]) continue;
            }
            ++processed;
            if (!blocked[direction]) {
                const byte x = byte(current.x + (direction == 2 ? 1 : direction == 3 ? -1 : 0));
                const byte y = byte(current.y + (direction == 1 ? 1 : direction == 0 ? -1 : 0)); uint index;
                if (!visitIndex(x,y,index)) return kNpcPathUnsupported;
                if (!visit(index)) {
                    visit(index) = 1;
                    if (_nodes.size() >= 256) { _status = kNpcPathError; return _status; }
                    const byte id = byte(_nodes.size()); NpcPathNode n; n.parent = _current; n.fromX = current.x; n.fromY = current.y; n.x = x; n.y = y; _nodes.push_back(n);
                    const NpcPathFrontier candidate(distance(x,y,_targetX,_targetY),id);
                    if (candidate.distance < threshold) {
                        const NpcPathFrontier previous = _frontier[0]; _frontier[0] = candidate; threshold = candidate.distance;
                        if (_frontierCount >= 2) {
                            if (previous.distance == 254) --insertion;
                            else { if (insertion < 0 || insertion >= 127) return kNpcPathUnsupported; _frontier[insertion] = previous; }
                        }
                    } else { if (insertion < 0 || insertion >= 127) return kNpcPathUnsupported; _frontier[insertion] = candidate; }
                    if (_frontierCount < 127) { _frontierCount = byte(_frontierCount + 1 - reserve); reserve = 0; ++insertion; }
                }
            }
            used[direction] = true;
        }
        if (uint(blocked[0]) + blocked[1] + blocked[2] + blocked[3] == 4 || _frontierCount < 2) { _status = kNpcPathError; return _status; }
        // Original byte-successor insertion comparison is stable for equal
        // ordinary keys; successor255 wraps and moves that key to the front.
        for (uint i = 1; i < _frontierCount; ++i) {
            const NpcPathFrontier value = _frontier[i]; uint at = i;
            while (at && _frontier[at - 1].distance >= byte(value.distance + 1)) { _frontier[at] = _frontier[at - 1]; --at; } _frontier[at] = value;
        }
        _fresh = false;
        if (_frontier[0].distance == 254) { _status = kNpcPathError; return _status; }
        if (!_zeroGoalVisitProfile && (_frontier[0].distance >= 170 || _frontier[0].node >= _nodes.size())) { _status = kNpcPathError; return _status; }
        if (_frontier[0].node >= _nodes.size()) return kNpcPathUnsupported;
        _current = _frontier[0].node; _currentX = _nodes[_current].x; _currentY = _nodes[_current].y;
        if (_currentX == _targetX && _currentY == _targetY) return finish();
        _status = kNpcPathSearching; return _status;
    }
    byte _type; NpcPathStatus _status; bool _fresh,_zeroGoalVisitProfile; byte _rootX,_rootY,_targetX,_targetY,_current,_currentX,_currentY,_frontierCount,_ranges[4];
    byte _visited[1024],_ownedVisited[2304],_route[256]; Common::Array<NpcPathNode> _nodes; NpcPathFrontier _frontier[127];
};
} }
#endif
