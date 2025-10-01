#ifndef VLCFG_COMMON_HPP
#define VLCFG_COMMON_HPP

#include <stdint.h>

#ifndef VLBS_RX_BAUDRATE
#define VLBS_RX_BAUDRATE (10)
#endif

#ifndef VLBS_RX_SAMPLES_PER_BIT
#define VLBS_RX_SAMPLES_PER_BIT (10)
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
  RXED_SYNC3,
  RXED_BYTE,
  ERROR,
};

struct CdrOutput {
  bool signal_detected;
  bool rxed;
  bool rx_bit;
};

struct PcsOutput {
  PcsState state;
  bool rxed;
  uint8_t rx_byte;
};

enum class Result : uint8_t {
  SUCCESS,
  ERR_OVERFLOW,
  ERR_SYNTAX,
  ERR_TOO_MANY_ENTRIES,
  ERR_KEY_TYPE_MISMATCH,
  ERR_KEY_TOO_LONG,
  ERR_KEY_NOT_FOUND,
  ERR_VALUE_TYPE_MISMATCH,
  ERR_VALUE_TOO_LONG,
  ERR_NULL_POINTER,
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

}  // namespace vlcfg

#endif
