#pragma once

#include <cstdint>
#include <cstring>

namespace ssd1306 {

class Bitmap {
 public:
  const int width;
  const int height;
  uint8_t* data;
  const bool autoFree;

  Bitmap(int w, int h, bool free = true)
      : width(w),
        height(h),
        data(new uint8_t[(w * h + 7) / 8]),
        autoFree(free) {}
  Bitmap(int w, int h, uint8_t* buf, bool free = false)
      : width(w), height(h), data(buf), autoFree(free) {}
  ~Bitmap() {
    if (autoFree) {
      delete[] data;
    }
  }

  void clear(bool white = false);
  void setPixel(int x, int y, bool white = true);
  bool getPixel(int x, int y) const;
  void fillRect(int x, int y, int w, int h, bool white = true);
  void drawRect(int x, int y, int w, int h, bool white) ;
  void copyTo(const Bitmap& dst) const;
};

#ifdef SSD1306_IMPLEMENTATION

void Bitmap::clear(bool white) {
  for (int i = 0; i < (width * height + 7) / 8; i++) {
    data[i] = white ? 0xFF : 0x00;
  }
}

void Bitmap::setPixel(int x, int y, bool white) {
  if (x < 0 || x >= width || y < 0 || y >= height) return;
  int row = y / 8;
  uint8_t mask = 1 << (y % 8);
  int index = row * width + x;
  if (white) {
    data[index] |= mask;
  } else {
    data[index] &= ~mask;
  }
}

bool Bitmap::getPixel(int x, int y) const {
  if (x < 0 || x >= width || y < 0 || y >= height) return false;
  int row = y / 8;
  uint8_t mask = 1 << (y % 8);
  int index = row * width + x;
  return (data[index] & mask) != 0;
}

void Bitmap::fillRect(int x, int y, int w, int h, bool white) {
  for (int j = 0; j < h; j++) {
    for (int i = 0; i < w; i++) {
      setPixel(x + i, y + j, white);
    }
  }
}

void Bitmap::drawRect(int x, int y, int w, int h, bool white)  {
  fillRect(x, y, w, 1, white);
  fillRect(x, y + h - 1, w, 1, white);
  fillRect(x, y, 1, h, white);
  fillRect(x + w - 1, y, 1, h, white);
}

void Bitmap::copyTo(const Bitmap& dst) const {
  std::memcpy(dst.data, data, width * ((height + 7) / 8));
}

#endif

}  // namespace ssd1306
