#ifndef VLCFG_COMMON_HPP
#define VLCFG_COMMON_HPP

#include <stdint.h>

#ifndef VLBS_RX_BAUDRATE
#define VLBS_RX_BAUDRATE (10)
#endif

#ifndef VLBS_RX_SAMPLES_PER_BIT
#define VLBS_RX_SAMPLES_PER_BIT (10)
#endif

#ifdef VLCFG_DEBUG

// Arduino の場合
#ifdef ARDUINO

#define VLCFG_PRINTF_BUFF_SIZE (256)

#include <Arduino.h>
int vlcfg_printf(const char* fmt, ...);

#ifdef VLCFG_IMPLEMENTATION
int vlcfg_printf(const char* fmt, ...) {
  int len = 0;
  char buf[VLCFG_PRINTF_BUFF_SIZE];

  va_list arg_ptr;
  va_start(arg_ptr, fmt);
  len = vsnprintf(buf, VLCFG_PRINTF_BUFF_SIZE, fmt, arg_ptr);
  va_end(arg_ptr);

  // output to the serial console through the 'Serial'
  len = Serial.write((uint8_t*)buf, (size_t)len);

  return len;
}
#endif

#define VLCFG_PRINTF(fmt, ...)                                \
  do {                                                        \
    vlcfg_printf("[VLCFG:%d] " fmt, __LINE__, ##__VA_ARGS__); \
  } while (0)

#else

#include <stdio.h>
#ifdef __FILE_NAME__
#define VLCFG_PRINTF(fmt, ...)                                      \
  do {                                                              \
    printf("[%s:%d] " fmt, __FILE_NAME__, __LINE__, ##__VA_ARGS__); \
  } while (0)
#else
#define VLCFG_PRINTF(fmt, ...)             \
  do {                                     \
    printf("[VLCFG] " fmt, ##__VA_ARGS__); \
  } while (0)
#endif

#endif

#else

#define VLCFG_PRINTF(fmt, ...) \
  do {                         \
  } while (0)
#endif

#define VLCFG_THROW(x)                                                   \
  do {                                                                   \
    Result __throw_ret = (x);                                            \
    VLCFG_PRINTF("Error: %s (Code=%d)\n", result_to_string(__throw_ret), \
                 (int)__throw_ret);                                      \
    return __throw_ret;                                                  \
  } while (0)

#define VLCFG_TRY(x)                    \
  do {                                  \
    Result __try_ret = (x);             \
    if (__try_ret != Result::SUCCESS) { \
      VLCFG_THROW(__try_ret);           \
    }                                   \
  } while (0)

namespace vlcfg {

static constexpr uint8_t SYMBOL_BITS = 5;

static constexpr uint32_t RX_BIT_PERIOD_US = 1000000 / VLBS_RX_BAUDRATE;
static constexpr uint32_t RX_SAMPLE_PERIOD_US =
    RX_BIT_PERIOD_US / VLBS_RX_SAMPLES_PER_BIT;

static constexpr uint8_t MAX_ENTRY_COUNT = 32;
static constexpr uint8_t MAX_KEY_LEN = 16;

static constexpr int16_t SYMBOL_CONTROL = -1;
static constexpr int16_t SYMBOL_SYNC = -2;
static constexpr int16_t SYMBOL_SOF = -3;
static constexpr int16_t SYMBOL_EOF = -4;
static constexpr int16_t SYMBOL_INVALID = -16;

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
  ERR_NULL_POINTER,
  ERR_OVERFLOW,
  ERR_TOO_MANY_ENTRIES,
  ERR_KEY_TYPE_MISMATCH,
  ERR_KEY_TOO_LONG,
  ERR_KEY_NOT_FOUND,
  ERR_VALUE_TYPE_MISMATCH,
  ERR_VALUE_TOO_LONG,
  ERR_LOS,
  ERR_EOF_EXPECTED,
  ERR_UNEXPECTED_EOF,
  ERR_EXTRA_BYTES,
  ERR_BAD_SHORT_COUNT,
  ERR_UNSUPPORTED_TYPE,
  ERR_BAD_CRC,
};

enum class ValueType : int8_t {
  // UINT = 0,
  // INT = 1,
  BYTE_STR = 2,
  TEXT_STR = 3,
  // ARRAY = 4,
  MAP = 5,
  // TAG = 6,
  // SIMPLE_FLOAT = 7,
  BOOLEAN = 8,
  NONE = -1,
};

enum ConfigEntryFlags : uint8_t {
  ENTRY_RECEIVED = 0x01,
};

struct ConfigEntry {
  const char* key;
  void* buffer;
  ValueType type;
  uint8_t capacity;
  uint8_t received;
  uint8_t flags;

  inline bool was_received() const { return (flags & ENTRY_RECEIVED) != 0; }
};

const char* result_to_string(Result res);
int16_t find_key(const ConfigEntry* entries, const char* key);
ConfigEntry* entry_from_key(ConfigEntry* entries, const char* key);
uint32_t crc32(const uint8_t* data, uint16_t length);
uint16_t median3(uint16_t a, uint16_t b, uint16_t c);

#ifdef VLCFG_IMPLEMENTATION

const char* result_to_string(Result res) {
  switch (res) {
    case Result::SUCCESS: return "SUCCESS";
    case Result::ERR_NULL_POINTER: return "ERR_NULL_POINTER";
    case Result::ERR_OVERFLOW: return "ERR_OVERFLOW";
    case Result::ERR_TOO_MANY_ENTRIES: return "ERR_TOO_MANY_ENTRIES";
    case Result::ERR_KEY_TYPE_MISMATCH: return "ERR_KEY_TYPE_MISMATCH";
    case Result::ERR_KEY_TOO_LONG: return "ERR_KEY_TOO_LONG";
    case Result::ERR_KEY_NOT_FOUND: return "ERR_KEY_NOT_FOUND";
    case Result::ERR_VALUE_TYPE_MISMATCH: return "ERR_VALUE_TYPE_MISMATCH";
    case Result::ERR_VALUE_TOO_LONG: return "ERR_VALUE_TOO_LONG";
    case Result::ERR_LOS: return "ERR_LOS";
    case Result::ERR_EOF_EXPECTED: return "ERR_EOF_EXPECTED";
    case Result::ERR_UNEXPECTED_EOF: return "ERR_UNEXPECTED_EOF";
    case Result::ERR_EXTRA_BYTES: return "ERR_EXTRA_BYTES";
    case Result::ERR_BAD_SHORT_COUNT: return "ERR_BAD_SHORT_COUNT";
    case Result::ERR_UNSUPPORTED_TYPE: return "ERR_UNSUPPORTED_TYPE";
    case Result::ERR_BAD_CRC: return "ERR_BAD_CRC";
    default: return "(Unknown Error)";
  }
}

int16_t find_key(const ConfigEntry* entries, const char* key) {
  if (entries == nullptr || key == nullptr) return -1;

  int16_t entry_index = -1;
  for (uint8_t i = 0; i < MAX_ENTRY_COUNT; i++) {
    const ConfigEntry& entry = entries[i];
    if (entry.key == nullptr) {
      return -1;
    }
    for (uint8_t j = 0; j < MAX_KEY_LEN + 1; j++) {
      if (entry.key[j] != key[j]) break;
      if (key[j] == '\0') return i;
    }
  }
  return -1;
}

ConfigEntry* entry_from_key(ConfigEntry* entries, const char* key) {
  int16_t index = find_key(entries, key);
  if (index < 0) return nullptr;
  return &entries[index];
}

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
