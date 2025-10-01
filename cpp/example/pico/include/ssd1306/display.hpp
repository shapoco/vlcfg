#pragma once

#include <cstddef>
#include <cstdint>

#include <hardware/gpio.h>
#include <hardware/i2c.h>

namespace ssd1306 {

enum Command : uint8_t {
  SET_MEM_MODE = 0x20,
  SET_COL_ADDR = 0x21,
  SET_PAGE_ADDR = 0x22,
  SET_HORIZ_SCROLL = 0x26,
  SET_SCROLL = 0x2E,
  SET_DISP_START_LINE = 0x40,
  SET_CONTRAST = 0x81,
  SET_CHARGE_PUMP = 0x8D,
  SET_SEG_REMAP = 0xA0,
  SET_ENTIRE_ON = 0xA4,
  SET_ALL_ON = 0xA5,
  SET_ALL_ON_RESUME = 0xA4,
  SET_NORM_DISP = 0xA6,
  SET_INV_DISP = 0xA7,
  SET_MUX_RATIO = 0xA8,
  SET_DISP = 0xAE,
  SET_COM_OUT_DIR = 0xC0,
  SET_DISP_OFFSET = 0xD3,
  SET_DISP_CLK_DIV = 0xD5,
  SET_PRECHARGE = 0xD9,
  SET_COM_PIN_CFG = 0xDA,
  SET_VCOM_DESEL = 0xDB,
};

class Display {
 public:
  i2c_inst_t* i2c;
  const int sdaPort;
  const int sclPort;
  const int width;
  const int height;
  const int rotation;
  const uint8_t devAddr;

  Display(int i2cHost, int sdaPort, int sclPort, int w, int h, int rotate = 0,
          uint8_t devAddr = 0x3C)
      : i2c(i2cHost == 0 ? i2c0 : i2c1),
        sdaPort(sdaPort),
        sclPort(sclPort),
        width(w),
        height(h),
        rotation(rotate),
        devAddr(devAddr) {}

  void writeCommandArray(Command cmd, const uint8_t* data, int length);

  inline void writeCommand(Command cmd) { writeCommandArray(cmd, nullptr, 0); }

  inline void writeCommand(Command cmd, uint8_t p0) {
    writeCommandArray(cmd, &p0, 1);
  }

  inline void writeCommand(Command cmd, uint8_t p0, uint8_t p1) {
    uint8_t params[] = {p0, p1};
    writeCommandArray(cmd, params, sizeof(params));
  }

  inline Command commandOr(Command cmd, uint8_t mask) {
    return static_cast<Command>(static_cast<uint8_t>(cmd) | mask);
  }

  void init();
  void setWindow(int x, int y, int w, int h);
  void writePixels(const void* data, int length);

  bool i2cBusReset();
};

#ifdef SSD1306_IMPLEMENTATION

void Display::writeCommandArray(Command cmd, const uint8_t* data, int length) {
  uint8_t buff[2] = {0x80, 0x00};
  buff[1] = static_cast<uint8_t>(cmd);
  i2c_write_blocking(i2c, devAddr, buff, 2, false);
  for (int i = 0; i < length; i++) {
    buff[1] = data[i];
    i2c_write_blocking(i2c, devAddr, buff, 2, false);
  }
}

void Display::init() {
  i2c_init(i2c, 400000);
  gpio_init(sdaPort);
  gpio_init(sclPort);
  gpio_pull_up(sdaPort);
  gpio_pull_up(sclPort);
  gpio_set_function(sdaPort, GPIO_FUNC_I2C);
  gpio_set_function(sclPort, GPIO_FUNC_I2C);

  writeCommand(Command::SET_DISP);
  writeCommand(Command::SET_MUX_RATIO, height - 1);
  writeCommand(Command::SET_DISP_OFFSET, 0);
  writeCommand(Command::SET_DISP_START_LINE);
  writeCommand(Command::SET_MEM_MODE, 0x00);
  if (rotation == 0) {
    writeCommand(Command::SET_SEG_REMAP);
    writeCommand(Command::SET_COM_OUT_DIR);
  } else {
    writeCommand(commandOr(Command::SET_SEG_REMAP, 0x01));
    writeCommand(commandOr(Command::SET_COM_OUT_DIR, 0x08));
  }
  writeCommand(Command::SET_VCOM_DESEL, 0x10);
  writeCommand(Command::SET_ALL_ON_RESUME);
  writeCommand(Command::SET_SCROLL);

  if (width == 128 && height == 64) {
    writeCommand(Command::SET_COM_PIN_CFG, 0x12);
  } else {
    writeCommand(Command::SET_COM_PIN_CFG, 0x02);
  }

  writeCommand(Command::SET_CONTRAST, 0xFF);
  writeCommand(Command::SET_ENTIRE_ON);
  writeCommand(Command::SET_NORM_DISP);
  writeCommand(Command::SET_CHARGE_PUMP, 0x14);

  writeCommand(commandOr(Command::SET_DISP, 0x01));
}

void Display::setWindow(int x, int y, int w, int h) {
  writeCommand(Command::SET_COL_ADDR, x, x + w - 1);
  writeCommand(Command::SET_PAGE_ADDR, y / 8, (y + h - 1) / 8);
}

void Display::writePixels(const void* data, int length) {
  //uint8_t firstByte = 0x40;
  //i2c_write_burst_blocking(i2c, devAddr, &firstByte, 1);
  //i2c_write_burst_blocking(i2c, devAddr, static_cast<const uint8_t*>(data),
  //                         length);
  //// force stop condition
  //uint8_t dummy = 0;
  //i2c_write_blocking(i2c, 0xFF, &dummy, 1, false);
  
  uint8_t firstByte = 0x40;
  i2c_write_burst_blocking(i2c, devAddr, &firstByte, 1);
  i2c_write_blocking(i2c, devAddr, static_cast<const uint8_t*>(data),
                           length, false);
}

bool Display::i2cBusReset() {
  constexpr uint8_t MAX_RETRIES = 3;
  constexpr uint8_t MAX_SCL_PULSES = 16;

  gpio_init(sdaPort);
  gpio_init(sclPort);
  gpio_set_function(sdaPort, GPIO_FUNC_SIO);
  gpio_set_function(sclPort, GPIO_FUNC_SIO);

  // change I2C pins to GPIO mode
  // disable();
  // gpio::setPullup(sdaPort, true);
  // gpio::setPullup(sclPort, true);
  // gpio::setDirMulti((1 << sdaPort) | (1 << sclPort), false);
  // gpio::writeMulti((1 << sdaPort) | (1 << sclPort), 0);
  gpio_pull_up(sdaPort);
  gpio_pull_up(sclPort);
  gpio_set_dir_masked((1 << sdaPort) | (1 << sclPort), 0);
  gpio_put_masked((1 << sdaPort) | (1 << sclPort), 0);
  sleep_us(50);

  bool success = false;
  if (gpio_get(sdaPort)) {
    // SDA is already high, no need to reset
    success = true;
  } else {
    for (int i = MAX_RETRIES; i != 0; i--) {
      // send SCL pulses until SDA is high
      // gpio::setDir(sdaPort, false);
      // gpio::setDir(sclPort, true);
      gpio_set_dir(sdaPort, false);
      gpio_set_dir(sclPort, true);
      sleep_us(50);
      for (int j = MAX_SCL_PULSES; j != 0; j--) {
        // gpio::setDir(sclPort, false);
        gpio_set_dir(sclPort, false);
        sleep_us(50);
        gpio_set_dir(sclPort, true);
        sleep_us(50);
        if (gpio_get(sdaPort)) break;
      }

      // send stop condition
      gpio_set_dir(sdaPort, true);
      sleep_us(50);
      gpio_set_dir(sclPort, false);
      sleep_us(50);
      gpio_set_dir(sdaPort, false);
      sleep_us(50);

      // check SDA state
      if (gpio_get(sdaPort)) {
        success = true;
        break;
      }
    }
  }

  // gpio_set_function(sdaPort, GPIO_FUNC_I2C);
  // gpio_set_function(sclPort, GPIO_FUNC_I2C);

  return success;
}

#endif

}  // namespace ssd1306
