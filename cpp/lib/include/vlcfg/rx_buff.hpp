#ifndef VLCFG_RX_BUFF_HPP
#define VLCFG_RX_BUFF_HPP

#include "vlcfg/common.hpp"

namespace vlcfg {

class RxBuff {
 public:
  const uint16_t capacity;
  uint8_t *buff;
  uint16_t write_pos = 0;
  uint16_t read_pos = 0;

  inline RxBuff(int capacity)
      : capacity(capacity), buff(new uint8_t[capacity]) {}
  inline ~RxBuff() { delete[] buff; }

  inline void init() {
    write_pos = 0;
    read_pos = 0;
  }

  inline uint16_t queued_size() const { return write_pos - read_pos; }
  inline uint16_t stored_size() const { return write_pos; }

  inline const uint8_t &operator[](uint16_t index) const { return buff[index]; }

  inline int peek(int offset) {
    if (read_pos + offset >= write_pos) {
      return -1;
    }
    return buff[read_pos + offset];
  }

  inline Result push(uint8_t b) {
    if (write_pos >= capacity) {
      VLCFG_THROW(Result::ERR_OVERFLOW);
    }
    buff[write_pos++] = b;
    return Result::SUCCESS;
  }

  inline Result popBytes(uint8_t *out, uint16_t len) {
    if (queued_size() < len) {
      VLCFG_THROW(Result::ERR_UNEXPECTED_EOF);
    }
    if (out == nullptr) {
      VLCFG_THROW(Result::ERR_NULL_POINTER);
    }
    for (uint16_t i = 0; i < len; i++) {
      out[i] = buff[read_pos++];
    }
    return Result::SUCCESS;
  }

  inline Result skip(uint16_t len) {
    if (queued_size() < len) {
      VLCFG_THROW(Result::ERR_UNEXPECTED_EOF);
    }
    read_pos += len;
    return Result::SUCCESS;
  }

  inline Result popU8(uint8_t *out) { return popBytes(out, 1); }
  inline Result popU16(uint16_t *out) {
    uint8_t buf[2];
    VLCFG_TRY(popBytes(buf, sizeof(buf)));
    *out = (static_cast<uint16_t>(buf[0]) << 8) | buf[1];
    return Result::SUCCESS;
  }
  inline Result popU32(uint32_t *out) {
    uint8_t buf[4];
    VLCFG_TRY(popBytes(buf, sizeof(buf)));
    *out = (static_cast<uint32_t>(buf[0]) << 24) |
           (static_cast<uint32_t>(buf[1]) << 16) |
           (static_cast<uint32_t>(buf[2]) << 8) | buf[3];
    return Result::SUCCESS;
  }
  inline Result popU64(uint64_t *out) {
    uint8_t buf[8];
    VLCFG_TRY(popBytes(buf, sizeof(buf)));
    *out = (static_cast<uint64_t>(buf[0]) << 56) |
           (static_cast<uint64_t>(buf[1]) << 48) |
           (static_cast<uint64_t>(buf[2]) << 40) |
           (static_cast<uint64_t>(buf[3]) << 32) |
           (static_cast<uint64_t>(buf[4]) << 24) |
           (static_cast<uint64_t>(buf[5]) << 16) |
           (static_cast<uint64_t>(buf[6]) << 8) | buf[7];
    return Result::SUCCESS;
  }

  Result read_item_header(CborMajorType *value_type, uint64_t *param);
  Result check_and_remove_crc();
};

#ifdef VLCFG_IMPLEMENTATION

Result RxBuff::read_item_header(CborMajorType *value_type, uint64_t *param) {
  uint8_t ib;
  VLCFG_TRY(popU8(&ib));

  *value_type = static_cast<CborMajorType>(ib >> 5);
  uint8_t short_count = (ib & 0x1f);

  if (*value_type != CborMajorType::SIMPLE_OR_FLOAT) {
    if (short_count <= 23) {
      *param = short_count;
    } else if (short_count <= 27) {
      uint8_t tmp[8];
      int8_t len = 1 << (short_count - 24);
      for (int8_t i = len - 1; i >= 0; i--) {
        VLCFG_TRY(popU8(&tmp[i]));
      }
      for (int8_t i = len; i < 8; i++) {
        tmp[i] = 0;
      }
      *param = (static_cast<uint64_t>(tmp[0]) << 0) |
               (static_cast<uint64_t>(tmp[1]) << 8) |
               (static_cast<uint64_t>(tmp[2]) << 16) |
               (static_cast<uint64_t>(tmp[3]) << 24) |
               (static_cast<uint64_t>(tmp[4]) << 32) |
               (static_cast<uint64_t>(tmp[5]) << 40) |
               (static_cast<uint64_t>(tmp[6]) << 48) |
               (static_cast<uint64_t>(tmp[7]) << 56);
    } else {
      VLCFG_THROW(Result::ERR_BAD_SHORT_COUNT);
    }
  } else if (short_count == 20 || short_count == 21) {
    *param = short_count;
    return Result::SUCCESS;
  } else {
    VLCFG_THROW(Result::ERR_UNSUPPORTED_TYPE);
  }

  return Result::SUCCESS;
}

Result RxBuff::check_and_remove_crc() {
  if (write_pos < 4) {
    VLCFG_THROW(Result::ERR_UNEXPECTED_EOF);
  }
  write_pos -= 4;

  uint32_t calcedCrc = crc32(buff, write_pos);
  uint32_t recvCrc = static_cast<uint32_t>(buff[write_pos]) << 24 |
                     static_cast<uint32_t>(buff[write_pos + 1]) << 16 |
                     static_cast<uint32_t>(buff[write_pos + 2]) << 8 |
                     static_cast<uint32_t>(buff[write_pos + 3]);
  if (calcedCrc != recvCrc) VLCFG_THROW(Result::ERR_BAD_CRC);
  VLCFG_PRINTF("CRC OK: 0x%08X\n", calcedCrc);
  return Result::SUCCESS;
}

#endif

}  // namespace vlcfg

#endif
