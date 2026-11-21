/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef BACKENDS_GRAPHICS_FRAME_DIAGNOSTICS_H
#define BACKENDS_GRAPHICS_FRAME_DIAGNOSTICS_H

#include "common/system.h"
#include "common/str.h"

// Main-thread-only, opt-in CPU wall timings. No GPU fences, I/O or driver changes.
// The engine enables this only around its own updateScreen call; other callers
// and other engines leave it inactive. Header storage is shared across TUs.
namespace GraphicsDiagnostics {
enum Stage { kLock, kPrepare, kScale, kUpload, kRender, kPresent, kMetadata, kOther, kStageCount };
inline const char *stageName(unsigned stage) {
 static const char *const names[] = {"lock", "prepare", "scale", "upload", "render", "present", "metadata", "other"};
 return names[stage];
}
enum TextureRole { kGame, kCursor, kCursorMask, kOverlay, kOSDMessage, kOSDIcon, kUnattributed, kTextureRoleCount };
enum TextureOp { kTextureUpdate, kConvertScale, kEdgeCopy, kBindTexture, kPixelStore, kTexSubImage, kTexImage, kErrorClear, kErrorCheck, kTextureOpCount };
inline const char *textureRoleName(unsigned role) {
 static const char *const names[] = {"game", "cursor", "cursor_mask", "overlay", "osd_message", "osd_icon", "unattributed"};
 return names[role];
}
inline const char *textureOpName(unsigned op) {
 static const char *const names[] = {"update", "convert_scale", "edge_prepare", "gl_bind_texture", "gl_pixel_store", "gl_tex_sub_image", "gl_tex_image", "gl_error_clear", "gl_error_check"};
 return names[op];
}
struct OperationTiming {
 uint64 total = 0, calls = 0;
 uint32 maximum = 0;
 void add(uint32 elapsed) { total += elapsed; ++calls; if (elapsed > maximum) maximum = elapsed; }
 void merge(const OperationTiming &other) {
  total += other.total; calls += other.calls;
  if (other.maximum > maximum) maximum = other.maximum;
 }
};
struct UploadInfo {
 uint32 texture = 0, internalFormat = 0, format = 0, type = 0, millis = 0, duration = 0;
 int allocatedWidth = 0, allocatedHeight = 0, sourceWidth = 0, sourceHeight = 0, pitch = 0, bpp = 0;
 int left = 0, top = 0, right = 0, bottom = 0;
 uint64 bytes = 0;
};
struct TextureTiming {
 OperationTiming operations[kTextureOpCount];
 uint64 uploads = 0, bytes = 0;
 UploadInfo last, slowest;
 void merge(const TextureTiming &other) {
  for (unsigned i = 0; i < kTextureOpCount; ++i) operations[i].merge(other.operations[i]);
  if (other.uploads) {
   if (!uploads || other.slowest.duration > slowest.duration) slowest = other.slowest;
   last = other.last;
  }
  uploads += other.uploads; bytes += other.bytes;
 }
};
struct Frame {
 bool active = false, queryMetadata = false;
 TextureRole textureRole = kUnattributed;
 TextureTiming textures[kTextureRoleCount];
 uint32 stamp = 0, millis[kStageCount] = {}, calls[kStageCount] = {};
 Common::String backend, renderer, vendor, version, videoDriver;
 int vsync = -2, width = 0, height = 0; // -2 means unknown; -1 may mean adaptive.
 void copyIdentity(const Frame &other) {
  backend = other.backend; renderer = other.renderer; vendor = other.vendor;
  version = other.version; videoDriver = other.videoDriver;
  vsync = other.vsync; width = other.width; height = other.height;
 }
};
inline Frame &state() { static Frame frame; return frame; }
inline void begin(bool metadata) {
 Frame &f = state();
 for (unsigned i = 0; i < kStageCount; ++i) f.millis[i] = f.calls[i] = 0;
 for (unsigned i = 0; i < kTextureRoleCount; ++i) f.textures[i] = TextureTiming();
 f.textureRole = kUnattributed;
 f.queryMetadata = metadata;
 f.active = true;
 f.stamp = g_system->getMillis(true);
}
// Attribute the elapsed segment since the previous mark to this stage.
inline void mark(Stage stage) {
 Frame &f = state();
 if (!f.active) return;
 const uint32 now = g_system->getMillis(true);
 f.millis[stage] += now - f.stamp;
 ++f.calls[stage];
 f.stamp = now;
}
inline void backend(const char *name) {
 Frame &f = state();
 if (!f.active || f.backend == name) return;
 f.backend = name;
 f.renderer.clear(); f.vendor.clear(); f.version.clear(); f.videoDriver.clear();
 f.vsync = -2; f.width = f.height = 0;
 f.queryMetadata = true;
}
// Independent nested timers: these do not disturb the coarse stage clock.
class TextureTimer {
 bool _active;
 TextureRole _role;
 TextureOp _op;
 uint32 _start;
public:
 explicit TextureTimer(TextureOp op) : _active(state().active), _role(state().textureRole), _op(op),
  _start(_active ? g_system->getMillis(true) : 0) {}
 ~TextureTimer() { stop(); }
 void stop() {
  if (!_active) return;
  state().textures[_role].operations[_op].add(g_system->getMillis(true) - _start);
  _active = false;
 }
};
class TextureScope {
 TextureRole _previous;
 bool _active;
 uint32 _start;
public:
 explicit TextureScope(TextureRole role) : _previous(state().textureRole), _active(state().active),
  _start(_active ? g_system->getMillis(true) : 0) { if (_active) state().textureRole = role; }
 ~TextureScope() {
  if (!_active) return;
  state().textures[state().textureRole].operations[kTextureUpdate].add(g_system->getMillis(true) - _start);
  state().textureRole = _previous;
 }
};
// Preserve error-clear -> actual-call -> error-check ordering. No extra GL calls.
template<class Call, class Clear, class Check>
inline void textureGLCall(TextureOp op, Call call, Clear clear, Check check) {
 { TextureTimer timer(kErrorClear); clear(); }
 { TextureTimer timer(op); call(); }
 { TextureTimer timer(kErrorCheck); check(); }
}
inline void noteUpload(UploadInfo info, uint32 start) {
 if (!state().active) return;
 TextureTiming &texture = state().textures[state().textureRole];
 info.millis = g_system->getMillis(true); info.duration = info.millis - start;
 if (!texture.uploads || info.duration > texture.slowest.duration) texture.slowest = info;
 texture.last = info; ++texture.uploads; texture.bytes += info.bytes;
}
inline void end() { mark(kOther); state().active = false; }
}
#endif
