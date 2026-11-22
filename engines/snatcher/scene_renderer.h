/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_SCENE_RENDERER_H
#define SNATCHER_SCENE_RENDERER_H
#include "scene_assets.h"
namespace Snatcher {
// Explicit game layout, never a register set. Ownership must establish it before
// rendering; dimensions are not inferred from a partially loaded map packet.
struct SceneViewBand {
 size_t firstRow;int sourceX[2],sourceY[2];bool displayEnabled;
 SceneViewBand():firstRow(0),displayEnabled(true){sourceX[0]=sourceX[1]=sourceY[0]=sourceY[1]=0;}
};
// Source-owned palette publication in display order. Positions are explicit
// semantic compositor inputs; deriving them from an unqualified timer is not
// the renderer's job. Equal positions preserve producer order.
struct ScenePaletteSpan {
 size_t row,column;unsigned firstColor;std::vector<uint16_t> colors;
 ScenePaletteSpan():row(0),column(0),firstColor(0){}
};
struct SceneLayout {
 std::vector<SceneViewBand> viewBands;
 size_t width,height,columns,rows;unsigned planeAllocation[2];int sourceX[2],sourceY[2];bool planeEnabled[2];bool displayEnabled;uint8_t backdrop;
 SceneLayout():width(0),height(0),columns(0),rows(0),displayEnabled(true),backdrop(0){for(unsigned i=0;i<2;++i){planeAllocation[i]=i?0xc000:0xe000;sourceX[i]=sourceY[i]=0;planeEnabled[i]=true;}}
};
enum SceneLayoutPreset {kSceneSquareLayout,kSceneTallLayout,kSceneWideLayout};
// Reads the three game layout presets embedded in the owner-supplied main IP.
// Selection remains an explicitly owned startup/controller decision.
bool readSceneLayoutPreset(const uint8_t *mainIp,size_t size,SceneLayoutPreset preset,SceneLayout& layout,std::string& error);
enum SceneColorEncoding {kSceneRGB888,kSceneReferenceRGB565};
struct SceneImage {
 size_t width,height,submittedPieces;bool spriteLimitReached;
 std::vector<uint8_t> indexed,rgb;
 SceneImage():width(0),height(0),submittedPieces(0),spriteLimitReached(false){}
};
// Resource-owned cells, two game tile planes, and the original ordered actor
// draw list. No CPU memory, VDP registers, BIOS, or executable dispatch exists.
class SceneRenderer {
public:
 SceneRenderer();
 void clear();
 bool configure(const SceneLayout& layout,std::string& error);
 bool setPalette(const std::vector<uint16_t>& colors,unsigned firstColor,std::string& error);
 bool applyTransfer(const GraphicsTransfer& transfer,std::string& error);
 bool matchesNameTransfer(const GraphicsTransfer& transfer)const;
 bool fillPlaneCells(unsigned plane,size_t column,size_t row,size_t columns,size_t rows,uint16_t attribute,std::string& error);
 // Only a recovered owner reset may establish previously unfilled cells as
 // blank. A direct test load must explicitly provide this state as a fixture.
 void establishBlankAssets();
 // Source Main FF15B0/FF15D2 resets only their owned graphics ranges;
 // unreset cells retain inherited known/unknown ownership.
 bool establishSceneReset(SceneLayoutPreset preset,std::string& error);
 bool render(const std::vector<PlacedPiece>& orderedPieces,SceneColorEncoding encoding,SceneImage& image,std::string& error,const std::vector<ScenePaletteSpan>* paletteSpans=nullptr)const;
 const SceneLayout& layout()const{return _layout;}
 bool configured()const{return _configured;}bool paletteKnown()const{return _paletteKnown;}
private:
 friend class SceneStateCodec;
 friend class NativeVisualCommit;
 PatternBank _patterns;std::vector<uint16_t> _maps[2];std::vector<bool> _known[2];std::vector<uint16_t> _palette;
 SceneLayout _layout;bool _configured,_paletteKnown;
 bool planePixel(unsigned plane,size_t x,size_t y,uint8_t& color,bool& priority,std::string& error)const;
};
// Measured normal-intensity source levels are 0,34,...238. RGB565 normalization
// is chosen before comparison, matching the reference software callback format.
bool sceneColor(uint16_t raw,SceneColorEncoding encoding,uint8_t& red,uint8_t& green,uint8_t& blue,std::string& error);
}
#endif
