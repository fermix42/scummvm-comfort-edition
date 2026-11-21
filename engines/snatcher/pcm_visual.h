/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_PCM_VISUAL_H
#define SNATCHER_PCM_VISUAL_H
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace Snatcher {
// Source13FEC: one byte per catalog ID, including unused index0. FF means no
// overlay; every other byte is a current-resource display-list index.
bool readPcmVisualTable(const uint8_t *sub,size_t size,size_t catalogCount,std::vector<uint8_t>& table,std::string& error);
bool lookupPcmVisual(const std::vector<uint8_t>& table,uint16_t catalogId,bool& mapped,uint8_t& displayList,std::string& error);
}
#endif
