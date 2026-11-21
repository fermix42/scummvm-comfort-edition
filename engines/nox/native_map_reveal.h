/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_MAP_REVEAL_H
#define NOX_NATIVE_MAP_REVEAL_H

#include "engines/ce_achievements.h"
#include "nox/native_menu_chrome.h"
#include "nox/native_world.h"
#include "nox/native_tile_paint.h"
#include "nox/native_world_presentation.h"
#include "nox/native_visibility.h"

namespace Nox { namespace Native {

constexpr char kRevealExplorationMap[] = "nox_reveal_exploration_map";
constexpr char kRevealMapAssistance[] = "nox.reveal_exploration_map";

inline bool revealExplorationMap() {
    return Common::CEAchievements().shouldApplyCheat(kRevealExplorationMap);
}

inline bool paintRevealedViewport(const World &world, const WorldStory &window,
        const ActorPaintRules &actors, const PresentationState &presentation,
        const WorldScene &ordinary, const VisibilityMask &visibility,
        const TilePainter &painter, byte animation, HgrFrame &frame) {
    // Use the ordinary ordering and source object pictures on a disposable
    // world. The presentation pass must not retire an open door's timer.
    World projection(world);
    PresentationState visualState = presentation;
    WorldScene revealed;
    const byte allVisible[187] = {};
    if (!WorldPresentation::run(projection, actors, allVisible, 187, visualState, revealed, &window, false))
        return false;
    const bool frigate = presentation.partyObject != 255 &&
        world.currentTables()->data()[1026 + presentation.partyObject] == 119;
    const auto revealCell = [&](uint cell) {
        return visibility.cells()[cell] && cell != 93 &&
            !(frigate && (cell == 94 || cell == 110 || cell == 111));
    };
    for (uint cell = 0; cell < 187; ++cell)
        if (revealCell(cell) && !painter.draw(frame, 0, ordinary.actors.terrain[cell], 1, animation, cell, 16))
            return false;
    for (const auto &draw : revealed.draws)
        if (revealCell(draw.cell) && !painter.draw(frame, draw.tile, revealed.actors.terrain[draw.cell], 1, animation, draw.cell, draw.height))
            return false;
    return true;
}

// This view owns only pixels. In particular, opening it does not change the
// source visibility mask, which is also used by world actors and movement.
inline bool paintMapOverview(const World &world, const GlyphFont &font,
        const TilePainter &painter, Common::Array<byte> &rgb) {
    Common::Array<byte> tiles;
    uint width;
    if (!world.mapTerrain(tiles, width))
        return false;
    HgrFrame chrome;
    if (!MenuChrome::begin(font, chrome, "Exploration map", 175) ||
        !MenuChrome::text(font, chrome, 2, 22, "M/Esc: close  White: party  Gold: doors"))
        return false;
    chrome.renderRGB(rgb);

    // Derive a representative colour from each original terrain tile.
    // Rendering in two batches avoids decoding terrain sectors per pixel.
    byte colours[256][3];
    for (uint first = 0; first < 256; first += 187) {
        HgrFrame swatches;
        const uint count = MIN<uint>(187, 256 - first);
        for (uint cell = 0; cell < count; ++cell)
            if (!painter.draw(swatches, 0, byte(first + cell), 1, 0, cell, 16))
                return false;
        Common::Array<byte> samples;
        swatches.renderRGB(samples);
        for (uint cell = 0; cell < count; ++cell) {
            uint sums[3] = {};
            const uint x = (cell % 17) * 28, y = 16 + (cell / 17) * 32;
            for (uint dy = 0; dy < 32; ++dy)
                for (uint dx = 0; dx < 28; ++dx)
                    for (uint channel = 0; channel < 3; ++channel)
                        sums[channel] += samples[((y + dy) * 560 + x + dx) * 3 + channel];
            for (uint channel = 0; channel < 3; ++channel)
                colours[first + cell][channel] = byte(20 + sums[channel] / (28 * 32) * 4 / 5);
        }
    }
    const uint scale = 288 / width;
    const uint left = (560 - width * scale) / 2;
    const uint top = (384 - width * scale) / 2;
    for (uint y = 0; y < width * scale; ++y)
        for (uint x = 0; x < width * scale; ++x)
            memcpy(rgb.data() + ((top + y) * 560 + left + x) * 3,
                   colours[tiles[(y / scale) * width + x / scale]], 3);
    const byte *objects = world.currentTables()->data() + 1024;
    const byte doorColour[] = {255, 180, 60};
    for (uint slot = 0; slot <= 252; slot += 4) {
        const byte *object = objects + slot;
        const byte state = object[3];
        if (!object[2] || !((state >= 16 && state < 32) || (state >= 33 && state < 42)) ||
            object[0] >= width || object[1] >= width)
            continue;
        const uint x = left + object[0] * scale, y = top + object[1] * scale;
        for (uint row = 0; row < scale; ++row)
            for (uint column = 0; column < scale; ++column)
                memcpy(rgb.data() + ((y + row) * 560 + x + column) * 3, doorColour, 3);
    }
    const int partyX = left + world.x() * scale + scale / 2;
    const int partyY = top + world.y() * scale + scale / 2;
    for (int offset = -3; offset <= 3; ++offset) {
        memset(rgb.data() + (partyY * 560 + partyX + offset) * 3, 255, 3);
        memset(rgb.data() + ((partyY + offset) * 560 + partyX) * 3, 255, 3);
    }
    return true;
}

} }
#endif
