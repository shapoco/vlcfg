#ifndef VLCFG_COMMON_HPP
#define VLCFG_COMMON_HPP

#include <stdint.h>

#ifndef VLBS_RX_BAUDRATE
#define VLBS_RX_BAUDRATE (10)
#endif

#ifndef VLBS_RX_SAMPLES_PER_BIT
#define VLBS_RX_SAMPLES_PER_BIT (10)
#endif

#define VLCFG_DEBUG
#ifdef VLCFG_DEBUG
#include <stdio.h>
#define VLCFG_PRINTF(fmt, ...)  \
  do {                          \
    printf("[%s] ", __func__);  \
    printf(fmt, ##__VA_ARGS__); \
  } while (0)
#else
#define VLCFG_PRINTF(fmt, ...) \
  do {                         \
  } while (0)
#endif

namespace vlcfg {

static constexpr uint8_t SYMBOL_BITS = 5;

static constexpr uint32_t RX_BIT_PERIOD_US = 1000000 / VLBS_RX_BAUDRATE;
static constexpr uint32_t RX_SAMPLE_PERIOD_US =
    RX_BIT_PERIOD_US / VLBS_RX_SAMPLES_PER_BIT;

enum class PcsState : uint8_t {
  LOS,
  RXED_SYNC1,
  RXED_SYNC2,
  RXED_SOF,
  RXED_BYTE,
  RXED_EOF,
};

struct CdrOutput {
  bool signal_detected;
  bool rxed;
  bool rx_bit;
};

struct PcsOutput {
  PcsState state;
  bool rxed;
  int16_t rx_byte;
};

enum class Result : uint8_t {
  SUCCESS,
  ERR_OVERFLOW,
  ERR_TOO_MANY_ENTRIES,
  ERR_KEY_TYPE_MISMATCH,
  ERR_KEY_TOO_LONG,
  ERR_KEY_NOT_FOUND,
  ERR_VALUE_TYPE_MISMATCH,
  ERR_VALUE_TOO_LONG,
  ERR_NULL_POINTER,
  ERR_LOS,
  ERR_EOF_EXPECTED,
  ERR_SYNTAX_UNEXPECTED_EOF,
  ERR_SYNTAX_BAD_SHORT_COUNT,
  ERR_SYNTAX_UNSUPPORTED_TYPE,
  ERR_BAD_CRC,
};

enum class CborMajorType : int8_t {
  // UINT = 0,
  // INT = 1,
  // BYTE_STR = 2,
  TEXT_STR = 3,
  // ARRAY = 4,
  MAP = 5,
  // TAG = 6,
  // SIMPLE_FLOAT = 7,
  INVALID = -1,
};

enum ConfigEntryFlags : uint8_t {
  NONE = 0,
};

struct ConfigEntry {
  const char* key;
  void* buffer;
  CborMajorType type;
  uint16_t max_size_in_bytes;
  ConfigEntryFlags flags;
};

uint32_t crc32(const uint8_t* data, uint16_t length);
uint16_t median3(uint16_t a, uint16_t b, uint16_t c);

#ifdef VLCFG_IMPLEMENTATION

uint32_t crc32(const uint8_t* data, uint16_t length) {
  uint32_t crc = 0xffffffff;
  for (uint32_t i = 0; i < length; i++) {
    uint8_t byte = data[i];
    crc ^= byte;
    for (uint8_t j = 0; j < 8; j++) {
      uint32_t mask = -(crc & 1);
      crc = (crc >> 1) ^ (0xedb88320 & mask);
    }
  }
  return ~crc;
}

uint16_t median3(uint16_t a, uint16_t b, uint16_t c) {
  if (a > b) {
    if (b > c) {
      return b;
    } else if (a > c) {
      return c;
    } else {
      return a;
    }
  } else {
    if (a > c) {
      return a;
    } else if (b > c) {
      return c;
    } else {
      return b;
    }
  }
}

#endif

}  // namespace vlcfg

#endif
