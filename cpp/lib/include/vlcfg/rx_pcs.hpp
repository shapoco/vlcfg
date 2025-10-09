#ifndef VLCFG_RX_PCS_HPP
#define VLCFG_RX_PCS_HPP

#include "vlcfg/common.hpp"

namespace vlcfg {

class RxPcs {
 private:
  PcsState state;
  uint16_t shift_reg;
  uint8_t phase;

 public:
#ifdef VLCFG_DEBUG
  int8_t dbg_rxed_symbol;
#endif

  inline RxPcs() { init(); }
  void init();
  Result update(const CdrOutput *in, PcsOutput *out);
  inline PcsState get_state() const { return state; }

 private:
  void reset_internal();
};

#ifdef VLCFG_IMPLEMENTATION

static const int8_t DECODE_TABLE[1 << SYMBOL_BITS] = {
    SYMBOL_INVALID,  // 0b00000
    SYMBOL_INVALID,  // 0b00001
    SYMBOL_INVALID,  // 0b00010
    SYMBOL_SOF,      // 0b00011
    SYMBOL_INVALID,  // 0b00100
    0x0,             // 0b00101
    0x1,             // 0b00110
    SYMBOL_EOF,      // 0b00111
    SYMBOL_INVALID,  // 0b01000
    0x2,             // 0b01001
    SYMBOL_CTRL,     // 0b01010
    0x3,             // 0b01011
    0x4,             // 0b01100
    0x5,             // 0b01101
    0x6,             // 0b01110
    SYMBOL_INVALID,  // 0b01111
    SYMBOL_INVALID,  // 0b10000
    SYMBOL_SYNC,     // 0b10001
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
  reset_internal();
  VLCFG_PRINTF("RX PCS initialized.\n");
}

Result RxPcs::update(const CdrOutput *in, PcsOutput *out) {
#ifdef VLCFG_DEBUG
  dbg_rxed_symbol = SYMBOL_NONE;
#endif

  if (out == nullptr) {
    VLCFG_THROW(Result::ERR_NULL_POINTER);
  }

  if (!in->signal_detected) {
    reset_internal();
    out->state = state;
    out->rxed = false;
    return Result::SUCCESS;
  } else if (!in->rxed) {
    out->state = state;
    out->rxed = false;
    return Result::SUCCESS;
  }

  // shift register
  constexpr uint16_t SHIFT_REG_MASK = (1 << (SYMBOL_BITS * 2)) - 1;
  shift_reg = (shift_reg << 1) & SHIFT_REG_MASK;
  if (in->rx_bit) shift_reg |= 1;

  constexpr uint8_t SYMBOL_MASK = (1 << SYMBOL_BITS) - 1;
  int8_t nibble_h = DECODE_TABLE[(shift_reg >> SYMBOL_BITS) & SYMBOL_MASK];
  int8_t nibble_l = DECODE_TABLE[shift_reg & SYMBOL_MASK];
  bool rxed_sync = false, rxed_sof = false, rxed_eof = false;
  if (nibble_h == SYMBOL_CTRL) {
    rxed_sync = (nibble_l == SYMBOL_SYNC);
    rxed_sof = (nibble_l == SYMBOL_SOF);
    rxed_eof = (nibble_l == SYMBOL_EOF);
  }

#ifdef VLCFG_DEBUG
  if ((state == PcsState::LOS) || (phase == SYMBOL_BITS - 1) || (phase == SYMBOL_BITS * 2 - 1)) {
    dbg_rxed_symbol = nibble_l;
  }
#endif

  bool rxed = false;
  PcsState last_state = state;
  if (state == PcsState::LOS) {
    if (rxed_sync) {
      // symbol lock
      phase = 0;
      state = PcsState::RXED_SYNC1;
    } else {
      state = PcsState::LOS;
    }
  } else if (phase < (SYMBOL_BITS * 2 - 1)) {
    phase++;
  } else {
    phase = 0;

    // symbol decode
    switch (state) {
      case PcsState::RXED_SYNC1:
        if (rxed_sync) {
          state = PcsState::RXED_SYNC2;
        } else {
          state = PcsState::LOS;
        }
        break;

      case PcsState::RXED_SYNC2:
        if (rxed_sof) {
          rxed = true;
          out->rx_byte = SYMBOL_SOF;
          state = PcsState::RXED_SOF;
        } else if (rxed_sync) {
          state = PcsState::RXED_SYNC2;
        } else {
          state = PcsState::LOS;
        }
        break;

      case PcsState::RXED_SOF:
      case PcsState::RXED_BYTE:
        if (rxed_eof) {
          rxed = true;
          out->rx_byte = SYMBOL_EOF;
          state = PcsState::RXED_EOF;
        } else if (nibble_h >= 0 && nibble_l >= 0) {
          rxed = true;
          out->rx_byte = (nibble_h << 4) | nibble_l;
          state = PcsState::RXED_BYTE;
        } else {
          state = PcsState::LOS;
        }
        break;

      case PcsState::RXED_EOF:
        if (rxed_sof) {
          rxed = true;
          out->rx_byte = SYMBOL_SOF;
          state = PcsState::RXED_SOF;
        } else if (rxed_sync) {
          state = PcsState::RXED_SYNC2;
        } else {
          state = PcsState::LOS;
        }
        break;

      default: break;
    }
  }

#ifdef VLCFG_DEBUG
  if (last_state != state) {
    VLCFG_PRINTF("PCS State: %d --> %d\n", (int)last_state, (int)state);
  }
#endif

  out->state = state;
  out->rxed = rxed;
  return Result::SUCCESS;
}

void RxPcs::reset_internal() {
  state = PcsState::LOS;
  phase = 0;
  shift_reg = 0;
#ifdef VLCFG_DEBUG
  dbg_rxed_symbol = SYMBOL_NONE;
#endif
}

#endif

}  // namespace vlcfg

#endif
