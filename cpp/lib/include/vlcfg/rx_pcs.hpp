#ifndef VLCFG_RX_PCS_HPP
#define VLCFG_RX_PCS_HPP

#include "vlcfg/common.hpp"

namespace vlcfg {

class RxPcs {
  // todo: private:
 public:
  PcsState state;
  uint16_t shift_reg;
  uint8_t phase;

 public:
  inline RxPcs() { init(); }
  void init();
  void update(const CdrOutput &in, PcsOutput &out);
  inline PcsState get_state() const { return state; }
};

#ifdef VLCFG_IMPLEMENTATION

typedef enum : int8_t {
  SYMBOL_SYNC0 = -1,
  SYMBOL_SYNC1 = -2,
  SYMBOL_INVALID = -8,
} vlbs_4b5b_symbol_t;

static const int8_t DECODE_TABLE[1 << SYMBOL_BITS] = {
    SYMBOL_INVALID,  // 0b00000
    SYMBOL_INVALID,  // 0b00001
    SYMBOL_INVALID,  // 0b00010
    SYMBOL_INVALID,  // 0b00011
    SYMBOL_INVALID,  // 0b00100
    0x0,             // 0b00101
    0x1,             // 0b00110
    SYMBOL_INVALID,  // 0b00111
    SYMBOL_INVALID,  // 0b01000
    0x2,             // 0b01001
    SYMBOL_SYNC0,    // 0b01010
    0x3,             // 0b01011
    0x4,             // 0b01100
    0x5,             // 0b01101
    0x6,             // 0b01110
    SYMBOL_INVALID,  // 0b01111
    SYMBOL_INVALID,  // 0b10000
    SYMBOL_SYNC1,    // 0b10001
    0x7,             // 0b10010
    0x8,             // 0b10011
    0x9,             // 0b10100
    0xA,             // 0b10101
    0xB,             // 0b10110
    SYMBOL_INVALID,  // 0b10111
    0xC,             // 0b11000
    0xD,             // 0b11001
    0xE,             // 0b11010
    SYMBOL_INVALID,  // 0b11011
    0xF,             // 0b11100
    SYMBOL_INVALID,  // 0b11101
    SYMBOL_INVALID,  // 0b11110
    SYMBOL_INVALID,  // 0b11111
};

void RxPcs::init() {
  state = PcsState::LOS;
  phase = 0;
  shift_reg = 0;
}

void RxPcs::update(const CdrOutput &in, PcsOutput &out) {
  if (!in.signal_detected) {
    init();
    out.state = state;
    out.rxed = false;
    return;
  } else if (!in.rxed) {
    out.state = state;
    out.rxed = false;
    return;
  }

  // shift register
  constexpr uint16_t SHIFT_REG_MASK = (1 << (SYMBOL_BITS * 2)) - 1;
  shift_reg = (shift_reg << 1) & SHIFT_REG_MASK;
  if (in.rx_bit) shift_reg |= 1;

  constexpr uint8_t SYMBOL_MASK = (1 << SYMBOL_BITS) - 1;
  int8_t nibble_h = DECODE_TABLE[(shift_reg >> SYMBOL_BITS) & SYMBOL_MASK];
  int8_t nibble_l = DECODE_TABLE[shift_reg & SYMBOL_MASK];
  bool sync_rxed = nibble_h == SYMBOL_SYNC0 && nibble_l == SYMBOL_SYNC1;
  bool rxed = false;
  if (state == PcsState::LOS || state == PcsState::ERROR) {
    if (sync_rxed) {
      // symbol lock
      phase = 0;
      state = PcsState::RXED_SYNC1;
    } else {
      state = PcsState::LOS;
    }
  } else {
    if (phase == (SYMBOL_BITS * 2 - 1)) {
      // symbol decode
      switch (state) {
        case PcsState::RXED_SYNC1:
          if (sync_rxed) {
            state = PcsState::RXED_SYNC2;
          } else {
            state = PcsState::LOS;
          }
          break;

        case PcsState::RXED_SYNC2:
          if (sync_rxed) {
            state = PcsState::RXED_SYNC3;
          } else {
            state = PcsState::LOS;
          }
          break;

        case PcsState::RXED_SYNC3:
        case PcsState::RXED_BYTE:
          if (sync_rxed) {
            state = PcsState::RXED_SYNC3;
          } else {
            if (nibble_h >= 0 && nibble_l >= 0) {
              out.rx_byte = (nibble_h << 4) | nibble_l;
              rxed = true;
              state = PcsState::RXED_BYTE;
            } else {
              state = PcsState::ERROR;
            }
          }
          break;

        default: break;
      }
    }

    // step decoder phase
    if (phase < (SYMBOL_BITS * 2 - 1)) {
      phase++;
    } else {
      phase = 0;
    }
  }

  out.state = state;
  out.rxed = rxed;
}
#endif

}  // namespace vlcfg

#endif
