#ifndef VLCFG_RX_DECODER_HPP
#define VLCFG_RX_DECODER_HPP

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
  RxState state = RxState::IDLE;

 public:
  inline RxDecoder(int capacity) : buff(capacity) { buff.init(); }

  void init(ConfigEntry* dst);
  Result update(PcsOutput* in, RxState* rx_state);
  inline RxState get_state() const { return state; }
  inline ConfigEntry* entry_from_key(const char* key) const {
    return vlcfg::entry_from_key(entries, key);
  }

 private:
  Result update_state(PcsOutput* in);
  Result rx_complete();
  Result read_key(int16_t* entry_index);
  Result read_value(ConfigEntry* entry);
};

#ifdef VLCFG_IMPLEMENTATION

void RxDecoder::init(ConfigEntry* entries) {
  this->buff.init();
  this->entries = entries;
  if (entries) {
    for (uint8_t i = 0; i < MAX_ENTRY_COUNT; i++) {
      ConfigEntry& entry = entries[i];
      if (entry.key == nullptr) break;
      entry.flags &= ~ConfigEntryFlags::ENTRY_RECEIVED;
      entry.received = 0;
    }
  }
  this->state = RxState::IDLE;
  VLCFG_PRINTF("RX Decoder initialized.\n");
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
  VLCFG_PRINTF("%d bytes received.\n", (int)buff.size());
  // #ifdef VLCFG_DEBUG
  //   VLCFG_PRINTF("buffer content:\n");
  //   for (uint16_t i = 0; i < buff.size(); i++) {
  //     VLCFG_PRINTF("  [%4d] %02X", (int)i, (int)buff.peek(i));
  //   }
  // #endif

  VLCFG_TRY(buff.check_and_remove_crc());

  CborMajorType mtype;
  uint64_t param;
  VLCFG_TRY(buff.read_item_header(&mtype, &param));
  if (mtype != CborMajorType::MAP) {
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
    VLCFG_TRY(read_key(&entry_index));
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

Result RxDecoder::read_key(int16_t* entry_index) {
  // read key
  CborMajorType mtype;
  uint64_t param;
  VLCFG_TRY(buff.read_item_header(&mtype, &param));
  if (mtype != CborMajorType::TEXT_STR) {
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

  *entry_index = find_key(entries, rx_key);

  VLCFG_PRINTF("--> field_index=%d\n", *entry_index);
  return Result::SUCCESS;
}

Result RxDecoder::read_value(ConfigEntry* entry) {
  CborMajorType mtype;
  uint64_t param;
  VLCFG_TRY(buff.read_item_header(&mtype, &param));

  if (entry != nullptr) {
    if (entry->buffer == nullptr) {
      VLCFG_THROW(Result::ERR_NULL_POINTER);
    }
  }

  switch (mtype) {
    case CborMajorType::UNSIGNED_INT:
    case CborMajorType::NEGATIVE_INT: {
      if (entry) {
        uint8_t buff_req = 0;
        bool rx_pos = mtype == CborMajorType::UNSIGNED_INT;
        bool rx_neg = mtype == CborMajorType::NEGATIVE_INT;
        bool rx_msb = (param & 0x8000000000000000) != 0;
        if (entry->type == ValueType::UINT) {
          if (rx_neg) VLCFG_THROW(Result::ERR_VALUE_OUT_OF_RANGE);
          if (param & 0xFFFFFFFF00000000) {
            buff_req = 8;
          } else if (param & 0x00000000FFFF0000) {
            buff_req = 4;
          } else if (param & 0x000000000000FF00) {
            buff_req = 2;
          } else {
            buff_req = 1;
          }
        } else if (entry->type == ValueType::INT) {
          if (rx_pos && rx_msb) VLCFG_THROW(Result::ERR_VALUE_OUT_OF_RANGE);
          if (rx_neg && rx_msb) VLCFG_THROW(Result::ERR_VALUE_OUT_OF_RANGE);
          if (param & 0xFFFFFFFF80000000) {
            buff_req = 8;
          } else if (param & 0x000000007FFF8000) {
            buff_req = 4;
          } else if (param & 0x0000000000007F80) {
            buff_req = 2;
          } else {
            buff_req = 1;
          }
          if (rx_neg) {
            param = static_cast<uint64_t>(-static_cast<int64_t>(param) - 1);
          }
        } else {
          VLCFG_THROW(Result::ERR_VALUE_TYPE_MISMATCH);
        }

        if (buff_req > entry->capacity) {
          VLCFG_THROW(Result::ERR_VALUE_TOO_LONG);
        }
        void* dst = entry->buffer;
        switch (entry->capacity) {
          case 1: *(uint8_t*)dst = static_cast<uint8_t>(param); break;
          case 2: *(uint16_t*)dst = static_cast<uint16_t>(param); break;
          case 4: *(uint32_t*)dst = static_cast<uint32_t>(param); break;
          case 8: *(uint64_t*)dst = static_cast<uint64_t>(param); break;
          default: VLCFG_THROW(Result::ERR_BUFF_SIZE_MISMATCH);
        }
        entry->received = entry->capacity;
      }
    } break;

    case CborMajorType::BYTE_STR:
    case CborMajorType::TEXT_STR: {
      bool is_text = (mtype == CborMajorType::TEXT_STR);
      uint16_t len = param;
      if (entry != nullptr) {
        ValueType exp_value_type =
            is_text ? ValueType::TEXT_STR : ValueType::BYTE_STR;
        if (entry->type != exp_value_type) {
          VLCFG_THROW(Result::ERR_VALUE_TYPE_MISMATCH);
        }

        uint16_t buff_req = is_text ? len + 1 : len;
        if (buff_req > entry->capacity) {
          VLCFG_THROW(Result::ERR_VALUE_TOO_LONG);
        }
        uint8_t* dst = (uint8_t*)entry->buffer;
        VLCFG_TRY(buff.popBytes(dst, len));
        if (is_text) {
          dst[len] = '\0';
          VLCFG_PRINTF("string value: '%s'\n", (char*)entry->buffer);
        }
        entry->received = buff_req;
      } else {
        VLCFG_TRY(buff.skip(len));
      }
    } break;

    case CborMajorType::SIMPLE_OR_FLOAT: {
      if (param == 20 || param == 21) {
        if (entry != nullptr) {
          if (entry->capacity != 1) {
            VLCFG_THROW(Result::ERR_BUFF_SIZE_MISMATCH);
          }
          bool value = (param == 20) ? false : true;
          *((uint8_t*)entry->buffer) = value ? 1 : 0;
          entry->received = 1;
          VLCFG_PRINTF("boolean value: %s\n", value ? "true" : "false");
        }
      } else {
        VLCFG_THROW(Result::ERR_UNSUPPORTED_TYPE);
      }

    } break;

    default: VLCFG_THROW(Result::ERR_UNSUPPORTED_TYPE);
  }

  if (entry) entry->flags |= ConfigEntryFlags::ENTRY_RECEIVED;
  return Result::SUCCESS;
}

#endif

}  // namespace vlcfg

#endif
