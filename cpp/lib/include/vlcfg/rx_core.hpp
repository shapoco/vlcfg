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

  uint8_t rx_buff_size;
  uint8_t rx_buff_pos = 0;
  uint8_t* rx_buff = nullptr;
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
  Result update(bool rx_valid, uint8_t rx_byte);

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

Result RxCore::update(bool rx_valid, uint8_t rx_byte) {
  if (rx_buff == nullptr || rx_buff_size == 0) {
    state = RxState::IDLE;
    return Result::SUCCESS;
  }

  if (state == RxState::IDLE || state == RxState::RECEIVING) {
    if (rx_valid) {
      if (rx_buff_pos >= rx_buff_size) {
        state = RxState::ERROR;
        return Result::ERR_OVERFLOW;
      }
      rx_buff[rx_buff_pos++] = rx_byte;
      state = RxState::RECEIVING;
    } else {
      auto ret = rx_complete();
      state = (ret == Result::SUCCESS) ? RxState::COMPLETED : RxState::ERROR;
      rx_buff_pos = 0;
      return ret;
    }
  }

  return Result::SUCCESS;
}

Result RxCore::rx_complete() {
  Result ret;

  if (rx_buff == nullptr) return Result::ERR_NULL_POINTER;

  uint8_t param;
  CborMajorType mtype;

  uint16_t pos = 0;
  ret = read_item_header_u8(&pos, &mtype, &param);
  if (ret != Result::SUCCESS) return ret;
  if (mtype != CborMajorType::MAP) return Result::ERR_SYNTAX;
  if (param > 255) return Result::ERR_TOO_MANY_ENTRIES;

  uint8_t num_entries = param;

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
  if (*pos + key_len > rx_buff_pos) return Result::ERR_SYNTAX;

  // Search key
  *entry_index = -1;
  for (uint8_t i = 0; i < num_entries; i++) {
    if (key_match(entries[i].key, *pos, key_len)) {
      *entry_index = i;
      break;
    }
  }

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
      if (*pos + len > rx_buff_pos) return Result::ERR_SYNTAX;
      if (entry != nullptr) {
        if (entry->max_size_in_bytes < len + 1) {
          return Result::ERR_VALUE_TOO_LONG;
        }
        uint8_t* dst = (uint8_t*)entry->buffer;
        for (uint16_t i = 0; i < len; i++) {
          dst[i] = rx_buff[*pos + i];
        }
        dst[len] = '\0';
      }
      *pos += len;

      return Result::SUCCESS;
    } break;

    default: return Result::ERR_VALUE_TYPE_MISMATCH;
  }
}

Result RxCore::read_item_header_u8(uint16_t* pos, CborMajorType* major_type,
                                   uint8_t* param) {
  if (*pos >= rx_buff_pos) return Result::ERR_SYNTAX;
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
        return Result::ERR_SYNTAX;
      }
      tmp = (tmp << 8) | rx_buff[(*pos)++];
    }
    *param = tmp;
    return Result::SUCCESS;
  } else {
    return Result::ERR_SYNTAX;
  }
}

#endif

}  // namespace vlcfg

#endif
