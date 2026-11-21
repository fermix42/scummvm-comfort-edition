/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_FRAME_PUBLICATION_H
#define SNATCHER_FRAME_PUBLICATION_H
#include <cstdint>
#include <cstddef>
#include <vector>
namespace Snatcher {
// Host presentation state only. Compare final pixels, including native mouse
// feedback, rather than guessing visibility from simulation/animation events.
class FramePublication {
public:
 void prepare(unsigned width,unsigned height) {
  if(width!=_width||height!=_height){_width=width;_height=height;invalidate();}
  _pixels.resize(std::size_t(width)*height);
 }
 uint16_t *pixels(){return _pixels.data();}
 bool publish() {
  if(_valid&&_previous==_pixels){++_unchanged;return false;}
  _previous=_pixels;_valid=true;++_uploads;return true;
 }
 void invalidate(){_valid=false;}
 uint64_t uploads()const{return _uploads;}
 uint64_t unchanged()const{return _unchanged;}
private:
 unsigned _width=0,_height=0;
 bool _valid=false;
 uint64_t _uploads=0,_unchanged=0;
 std::vector<uint16_t> _pixels,_previous;
};
}
#endif
