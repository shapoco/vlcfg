#ifndef VLCFG_COMMON_HPP
#define VLCFG_COMMON_HPP

#include <stdint.h>

#ifndef VLBS_RX_BAUDRATE
#define VLBS_RX_BAUDRATE (10)
#endif

#ifndef VLBS_RX_SAMPLES_PER_BIT
#define VLBS_RX_SAMPLES_PER_BIT (10)
#endif

#define VLCFG_SYMBOL_BITS (5)

namespace vlcfg {

static const uint32_t VLBS_RX_BIT_PERIOD_US = 1000000 / VLBS_RX_BAUDRATE;
static const uint32_t VLBS_RX_SAMPLE_PERIOD_US =
    VLBS_RX_BIT_PERIOD_US / VLBS_RX_SAMPLES_PER_BIT;
static const uint16_t VLBS_ADC_AVE_PERIOD = VLBS_RX_SAMPLES_PER_BIT * 6 * 4;

typedef enum : uint8_t {
  VLBS_CDR_LOS,
  VLBS_CDR_IDLE,
  VLBS_CDR_RXED_0,
  VLBS_CDR_RXED_1,
} vlbs_cdr_state_t;

typedef enum : uint8_t {
  VLBS_PCS_LOS,
  VLBS_PCS_RXED_SYNC1,
  VLBS_PCS_RXED_SYNC2,
  VLBS_PCS_RXED_SYNC3,
  VLBS_PCS_RXED_BYTE,
  VLBS_PCS_ERROR,
} vlbs_pcs_state_t;

typedef enum : int8_t {
  VLBS_4B5B_SYM_IDLE0 = -1,
  VLBS_4B5B_SYM_IDLE1 = -2,
  VLBS_4B5B_SYM_BAD = -8,
} vlbs_4b5b_symbol_t;

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

enum EntryFlags : uint8_t {
  VLCFG_ENTRY_FLAG_DUMMY,
};

struct ConfigEntry {
  const char* key;
  void* buffer;
  CborMajorType type;
  uint16_t max_size_in_bytes;
  EntryFlags flags;
};

}  // namespace vlcfg

#endif
