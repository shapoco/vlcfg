#pragma once

#include <stdint.h>

#include "ssd1306/bitmap.hpp"

namespace ssd1306 {

class Font {
 public:
  const int width;
  const int height;
  const Bitmap &bitmap;
  const int xSpacing;
  const int ySpacing;
  const uint16_t firstChar = 32;
  const uint16_t lastChar = 126;
  Font(int w, int h, const Bitmap &bmp, int xsp = 1, int ysp = 1)
      : width(w), height(h), bitmap(bmp), xSpacing(xsp), ySpacing(ysp) {}

  void drawCharTo(Bitmap &bmp, char c, int x, int y, bool white = true) const;
  int drawStringTo(Bitmap &bmp, const char *str, int x, int y,
                   bool white = true) const;
};

#ifdef SSD1306_IMPLEMENTATION

void Font::drawCharTo(Bitmap &bmp, char c, int x, int y, bool white) const {
  if (c < firstChar || c > lastChar) return;
  int charIndex = c - firstChar;
  for (int cy = 0; cy < height; cy++) {
    for (int cx = 0; cx < width; cx++) {
      if (bitmap.getPixel(charIndex * width + cx, cy)) {
        bmp.setPixel(x + cx, y + cy, white);
      }
    }
  }
}

int Font::drawStringTo(Bitmap &bmp, const char *str, int x, int y,
                       bool white) const {
  int startX = x;
  while (*str) {
    if (*str == '\n') {
      x = startX;
      y += height + ySpacing;
    } else {
      drawCharTo(bmp, *str, x, y, white);
      x += width + xSpacing;
    }
    str++;
  }
  return x - startX;
}

#endif

}  // namespace ssd1306
