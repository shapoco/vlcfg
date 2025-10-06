#ifndef VLCFG_CBOR_READER_HPP
#define VLCFG_CBOR_READER_HPP

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

  inline uint16_t size() const { return write_pos - read_pos; }

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
    if (size() < len) {
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
    if (size() < len) {
      VLCFG_THROW(Result::ERR_UNEXPECTED_EOF);
    }
    read_pos += len;
    return Result::SUCCESS;
  }

  inline bool readIfMatch(const uint8_t *pattern, uint16_t len) {
    if (size() < len) return false;
    for (uint16_t i = 0; i < len; i++) {
      if (peek(i) != pattern[i]) return false;
    }
    read_pos += len;
    return true;
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

  Result read_item_header_u32(ValueType *major_type, uint32_t *param);
  Result check_and_remove_crc();
};

#ifdef VLCFG_IMPLEMENTATION

Result RxBuff::read_item_header_u32(ValueType *major_type, uint32_t *param) {
  uint8_t ib;
  VLCFG_TRY(popU8(&ib));

  *major_type = static_cast<ValueType>(ib >> 5);

  uint8_t short_count = (ib & 0x1f);
  if (short_count <= 23) {
    *param = short_count;
  } else if (short_count == 24) {
    uint8_t tmp;
    VLCFG_TRY(popU8(&tmp));
    *param = tmp;
  } else if (short_count == 25) {
    uint16_t tmp;
    VLCFG_TRY(popU16(&tmp));
    *param = tmp;
  } else if (short_count == 26) {
    uint32_t tmp;
    VLCFG_TRY(popU32(&tmp));
    *param = tmp;
  } else {
    VLCFG_THROW(Result::ERR_BAD_SHORT_COUNT);
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
