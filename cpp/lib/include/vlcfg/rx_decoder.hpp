#ifndef VLCFG_RX_HPP
#define VLCFG_RX_HPP

#include "vlcfg/common.hpp"
#include "vlcfg/rx_buff.hpp"

namespace vlcfg {

enum class RxState : uint8_t {
  IDLE,
  RECEIVING,
  COMPLETED,
  ERROR,
};

class RxDecoder {
 private:
  RxBuff buff;

  ConfigEntry* entries = nullptr;
  uint8_t num_entries = 0;
  RxState state = RxState::IDLE;

 public:
  inline RxDecoder(int capacity) : buff(capacity) { buff.init(); }

  void init(ConfigEntry* dst, uint8_t num_entries);
  Result update(PcsOutput* in, RxState* rx_state);

 private:
  Result update_state(PcsOutput* in);
  Result rx_complete();
  Result find_key(int16_t* entry_index);
  Result read_value(ConfigEntry* entry);
};

#ifdef VLCFG_IMPLEMENTATION

void RxDecoder::init(ConfigEntry* entries, uint8_t num_entries) {
  this->buff.init();
  this->entries = entries;
  this->num_entries = num_entries;
  for (uint8_t i = 0; i < num_entries; i++) {
    ConfigEntry& entry = entries[i];
    entry.flags &= ~ConfigEntryFlags::RECEIVED;
    entry.received = 0;
  }
  this->state = RxState::IDLE;
}

Result RxDecoder::update(PcsOutput* in, RxState* rx_state) {
  Result ret = update_state(in);
  if (ret != Result::SUCCESS) {
    state = RxState::ERROR;
  }
  if (rx_state) {
    *rx_state = state;
  }
  return ret;
}

Result RxDecoder::update_state(PcsOutput* in) {
  switch (state) {
    case RxState::IDLE:
      if (in->rxed && in->rx_byte == SYMBOL_SOF) {
        state = RxState::RECEIVING;
      }
      break;

    case RxState::RECEIVING:
      if (in->state == PcsState::LOS) {
        VLCFG_THROW(Result::ERR_LOS);
      } else if (in->rxed) {
        if (in->rx_byte == SYMBOL_EOF) {
          VLCFG_TRY(rx_complete());
          state = RxState::COMPLETED;
        } else if (0 <= in->rx_byte && in->rx_byte <= 255) {
          VLCFG_PRINTF("rxed: 0x%02X\n", (int)in->rx_byte);
          VLCFG_TRY(buff.push(in->rx_byte));
        } else {
          VLCFG_THROW(Result::ERR_EOF_EXPECTED);
        }
      }
      break;

    default: break;
  }
  return Result::SUCCESS;
}

Result RxDecoder::rx_complete() {
#ifdef VLCFG_DEBUG
  VLCFG_PRINTF("buffer content:");
  for (uint16_t i = 0; i < buff.size(); i++) {
    printf(" %02X", (int)buff.peek(i));
  }
  printf("\n");
#endif

  VLCFG_TRY(buff.check_and_remove_crc());

  uint32_t param;
  ValueType mtype;

  uint16_t pos = 0;
  VLCFG_TRY(buff.read_item_header_u32(&mtype, &param));
  if (mtype != ValueType::MAP) {
    VLCFG_THROW(Result::ERR_UNSUPPORTED_TYPE);
  }
  if (param > MAX_ENTRY_COUNT) {
    VLCFG_THROW(Result::ERR_TOO_MANY_ENTRIES);
  }

  uint8_t num_entries = param;

  VLCFG_PRINTF("CBOR object, num_entries=%d\n", num_entries);

  for (uint8_t i = 0; i < num_entries; i++) {
    // match key
    int16_t entry_index = -1;
    VLCFG_TRY(find_key(&entry_index));
    if (entry_index < 0) {
      VLCFG_THROW(Result::ERR_KEY_NOT_FOUND);
    }
    ConfigEntry& entry = entries[entry_index];

    // value
    VLCFG_TRY(read_value(&entry));
  }

  if (buff.size() != 0) {
    VLCFG_THROW(Result::ERR_EXTRA_BYTES);
  }

  VLCFG_PRINTF("CBOR parsing completed successfully.\n");

  return Result::SUCCESS;
}

Result RxDecoder::find_key(int16_t* entry_index) {
  // read key
  ValueType mtype;
  uint32_t param;
  VLCFG_TRY(buff.read_item_header_u32(&mtype, &param));
  if (mtype != ValueType::TEXT_STR) {
    VLCFG_THROW(Result::ERR_KEY_TYPE_MISMATCH);
  }
  if (param > MAX_KEY_LEN) {
    VLCFG_THROW(Result::ERR_KEY_TOO_LONG);
  }
  uint8_t rx_key_len = param;
  char rx_key[MAX_KEY_LEN + 1];
  VLCFG_TRY(buff.popBytes((uint8_t*)rx_key, rx_key_len));
  rx_key[rx_key_len] = '\0';
  VLCFG_PRINTF("key: '%s'\n", rx_key);

  *entry_index = -1;
  for (uint8_t i = 0; i < num_entries; i++) {
    ConfigEntry& entry = entries[i];
    if (entry.key == nullptr) {
      VLCFG_THROW(Result::ERR_NULL_POINTER);
    }
    for (uint8_t j = 0; j < MAX_KEY_LEN + 1; j++) {
      if (entry.key[j] != rx_key[j]) break;
      if (rx_key[j] == '\0') {
        *entry_index = i;
        break;
      }
    }
    if (*entry_index >= 0) break;
  }

  VLCFG_PRINTF("--> field_index=%d\n", *entry_index);
  return Result::SUCCESS;
}

Result RxDecoder::read_value(ConfigEntry* entry) {
  ValueType mtype;
  uint32_t param;
  VLCFG_TRY(buff.read_item_header_u32(&mtype, &param));

  if (entry != nullptr) {
    if (mtype != entry->type) {
      VLCFG_THROW(Result::ERR_VALUE_TYPE_MISMATCH);
    }
    if (entry->buffer == nullptr) {
      VLCFG_THROW(Result::ERR_NULL_POINTER);
    }
  }

  uint8_t len = 0;
  switch (mtype) {
    case ValueType::BYTE_STR:
    case ValueType::TEXT_STR: {
      bool is_text = (mtype == ValueType::TEXT_STR);
      len = param;
      if (entry != nullptr) {
        uint8_t buff_req = is_text ? len + 1 : len;
        if (buff_req > entry->capacity) {
          VLCFG_THROW(Result::ERR_VALUE_TOO_LONG);
        }
        uint8_t* dst = (uint8_t*)entry->buffer;
        VLCFG_TRY(buff.popBytes(dst, len));
        if (is_text) {
          dst[len] = '\0';
          VLCFG_PRINTF("string value: '%s'\n", (char*)entry->buffer);
        }
      } else {
        VLCFG_TRY(buff.skip(len));
      }
    } break;

    default: VLCFG_THROW(Result::ERR_VALUE_TYPE_MISMATCH);
  }

  entry->flags |= ConfigEntryFlags::RECEIVED;
  entry->received = len;
  return Result::SUCCESS;
}

#endif

}  // namespace vlcfg

#endif
