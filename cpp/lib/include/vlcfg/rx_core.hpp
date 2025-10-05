#ifndef VLCFG_RX_HPP
#define VLCFG_RX_HPP

#include <vlcfg/rx_phy.hpp>

#include "vlcfg/common.hpp"

namespace vlcfg {

enum class RxState : uint8_t {
  IDLE,
  RECEIVING,
  COMPLETED,
  ERROR,
};

class RxCore {
 private:
  vlcfg::RxPhy phy;

  const uint16_t rx_buff_size;

  uint8_t* rx_buff;
  uint8_t rx_buff_pos = 0;
  ConfigEntry* entries = nullptr;
  uint8_t num_entries = 0;
  RxState state = RxState::IDLE;

 public:
  inline RxCore(int buffer_size)
      : rx_buff_size(buffer_size), rx_buff(new uint8_t[buffer_size]) {
    phy.init();
  }

  inline ~RxCore() { delete[] rx_buff; }
  void init(ConfigEntry* dst, uint8_t num_entries);
  Result update(PcsOutput* in, RxState* rx_state);

 private:
  Result rx_complete();

  Result find_key(uint16_t* pos, int16_t* entry_index);
  bool key_match(const char* key, uint16_t pos, uint16_t len);
  Result read_value(uint16_t* pos, ConfigEntry* entry);
  Result read_item_header_u8(uint16_t* pos, CborMajorType* major_type,
                             uint8_t* param);
};

#ifdef VLCFG_IMPLEMENTATION

void RxCore::init(ConfigEntry* entries, uint8_t num_entries) {
  this->entries = entries;
  this->num_entries = num_entries;
  this->rx_buff_pos = 0;
  this->state = RxState::IDLE;
}

Result RxCore::update(PcsOutput* in, RxState* rx_state) {
  if (rx_state == nullptr) return Result::ERR_NULL_POINTER;
  if (rx_buff == nullptr) return Result::ERR_NULL_POINTER;

  Result ret = Result::SUCCESS;

  switch (state) {
    case RxState::IDLE:
      if (in->rxed && in->rx_byte == SYMBOL_SOF) {
        state = RxState::RECEIVING;
        rx_buff_pos = 0;
      }
      break;

    case RxState::RECEIVING:
      if (in->state == PcsState::LOS) {
        state = RxState::ERROR;
        ret = Result::ERR_LOS;
      } else if (in->rxed) {
        if (in->rx_byte == SYMBOL_EOF) {
          ret = rx_complete();
          state =
              (ret == Result::SUCCESS) ? RxState::COMPLETED : RxState::ERROR;
          rx_buff_pos = 0;
        } else if (0 <= in->rx_byte && in->rx_byte <= 255) {
          if (rx_buff_pos < rx_buff_size) {
            VLCFG_PRINTF("rxed: 0x%02X\n", (int)in->rx_byte);
            rx_buff[rx_buff_pos++] = in->rx_byte;
          } else {
            state = RxState::ERROR;
            ret = Result::ERR_OVERFLOW;
          }
        } else {
          state = RxState::ERROR;
          ret = Result::ERR_EOF_EXPECTED;
        }
      }
      break;

    default: break;
  }

  *rx_state = state;
  return ret;
}

Result RxCore::rx_complete() {
  Result ret;

  if (rx_buff == nullptr) return Result::ERR_NULL_POINTER;

#ifdef VLCFG_DEBUG
  VLCFG_PRINTF("buffer content:");
  for (uint16_t i = 0; i < rx_buff_pos; i++) {
    printf(" %02X", (int)rx_buff[i]);
  }
  printf("\n");
#endif

  if (rx_buff_pos < 4) {
    return Result::ERR_SYNTAX_UNEXPECTED_EOF;
  }
  uint32_t calcedCrc = crc32(rx_buff, rx_buff_pos - 4);
  uint32_t recvCrc = 0;
  for (uint8_t i = 0; i < 4; i++) {
    recvCrc |= (rx_buff[rx_buff_pos - 4 + i] << (8 * i));
  }
  if (calcedCrc != recvCrc) {
    VLCFG_PRINTF("CRC mismatch: calculated=0x%08X, received=0x%08X\n",
                 calcedCrc, recvCrc);
    return Result::ERR_BAD_CRC;
  }
  VLCFG_PRINTF("CRC OK: 0x%08X\n", calcedCrc);

  uint8_t param;
  CborMajorType mtype;

  uint16_t pos = 0;
  ret = read_item_header_u8(&pos, &mtype, &param);
  if (ret != Result::SUCCESS) return ret;
  if (mtype != CborMajorType::MAP) return Result::ERR_SYNTAX_UNSUPPORTED_TYPE;
  if (param > 255) return Result::ERR_TOO_MANY_ENTRIES;

  uint8_t num_entries = param;

  VLCFG_PRINTF("CBOR object, num_entries=%d\n", num_entries);

  for (uint8_t i = 0; i < num_entries; i++) {
    // key
    int16_t entry_index = -1;
    auto ret = find_key(&pos, &entry_index);
    if (ret != Result::SUCCESS) return ret;
    if (entry_index < 0) return Result::ERR_KEY_NOT_FOUND;

    // value
    ret = read_value(&pos, &entries[entry_index]);
    if (ret != Result::SUCCESS) return ret;
  }

  VLCFG_PRINTF("CBOR parsing completed successfully.\n");

  return Result::SUCCESS;
}

Result RxCore::find_key(uint16_t* pos, int16_t* entry_index) {
  Result ret;
  CborMajorType mtype;
  uint8_t param;
  ret = read_item_header_u8(pos, &mtype, &param);
  if (ret != Result::SUCCESS) return ret;
  if (mtype != CborMajorType::TEXT_STR) return Result::ERR_KEY_TYPE_MISMATCH;
  if (param > 255) return Result::ERR_KEY_TOO_LONG;

  uint8_t key_len = param;
  if (*pos + key_len > rx_buff_pos) {
    return Result::ERR_SYNTAX_UNEXPECTED_EOF;
  }

  // Search key
  *entry_index = -1;
  for (uint8_t i = 0; i < num_entries; i++) {
    if (key_match(entries[i].key, *pos, key_len)) {
      *entry_index = i;
      break;
    }
  }

#ifdef VLCFG_DEBUG
  {
    char key[256];
    for (uint16_t i = 0; i < key_len; i++) {
      key[i] = (char)rx_buff[*pos + i];
    }
    key[key_len] = '\0';
    VLCFG_PRINTF("key '%s' --> field_index=%d\n", key, *entry_index);
  }
#endif

  *pos += key_len;
  return Result::SUCCESS;  // Not found
}

bool RxCore::key_match(const char* key, uint16_t pos, uint16_t len) {
  if (pos + len > rx_buff_pos) return false;
  for (uint16_t i = 0; i < len; i++) {
    if (key[i] == '\0' || key[i] != (char)rx_buff[pos + i]) return false;
  }
  return key[len] == '\0';
}

Result RxCore::read_value(uint16_t* pos, ConfigEntry* entry) {
  Result ret;

  CborMajorType mtype;
  uint8_t param;
  ret = read_item_header_u8(pos, &mtype, &param);
  if (ret != Result::SUCCESS) return ret;

  if (entry != nullptr) {
    if (mtype != entry->type) return Result::ERR_VALUE_TYPE_MISMATCH;
    if (entry->buffer == nullptr) return Result::ERR_NULL_POINTER;
  }

  switch (mtype) {
    case CborMajorType::TEXT_STR: {
      uint8_t len = param;
      if (*pos + len > rx_buff_pos) {
        return Result::ERR_SYNTAX_UNEXPECTED_EOF;
      }
      if (entry != nullptr) {
        if (entry->max_size_in_bytes < len + 1) {
          return Result::ERR_VALUE_TOO_LONG;
        }
        uint8_t* dst = (uint8_t*)entry->buffer;
        for (uint16_t i = 0; i < len; i++) {
          dst[i] = rx_buff[*pos + i];
        }
        dst[len] = '\0';

        VLCFG_PRINTF("string value: '%s'\n", (char*)entry->buffer);
      }
      *pos += len;

      return Result::SUCCESS;
    } break;

    default: return Result::ERR_VALUE_TYPE_MISMATCH;
  }
}

Result RxCore::read_item_header_u8(uint16_t* pos, CborMajorType* major_type,
                                   uint8_t* param) {
  if (*pos >= rx_buff_pos) return Result::ERR_SYNTAX_UNEXPECTED_EOF;
  uint8_t ib = rx_buff[(*pos)++];

  *major_type = static_cast<CborMajorType>(ib >> 5);
  uint8_t short_count = (ib & 0x1f);
  if (short_count <= 23) {
    *param = short_count;
    return Result::SUCCESS;
  } else if (short_count <= 27) {
    uint8_t len = 1 << (short_count - 24);
    uint32_t tmp = 0;
    for (uint8_t i = 0; i < len; i++) {
      if (*pos >= rx_buff_pos) {
        return Result::ERR_SYNTAX_UNEXPECTED_EOF;
      }
      tmp = (tmp << 8) | rx_buff[(*pos)++];
    }
    *param = tmp;
    return Result::SUCCESS;
  } else {
    return Result::ERR_SYNTAX_BAD_SHORT_COUNT;
  }
}

#endif

}  // namespace vlcfg

#endif
