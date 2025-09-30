#ifndef VLCFG_RX_PHY_HPP
#define VLCFG_RX_PHY_HPP

#include "vlcfg/common.hpp"

namespace vlcfg {

static uint16_t vlbs_u16log2(uint16_t x);

static const uint16_t VLBS_U16LOG2_TABLE[17] = {
    0,   22,  44,  63,  82,  100, 118, 134, 150,
    165, 179, 193, 207, 220, 232, 244, 256,
};
extern const int8_t VLBS_PCR_TABLE[1 << VLCFG_SYMBOL_BITS];

class RxPhy {
 private:
  vlbs_cdr_state_t cdr_state;
  uint32_t cdr_adc_sum_value;
  uint8_t cdr_adc_sum_count;
  uint16_t cdr_adc_ave;
  uint8_t cdr_last_digital_level;
  uint8_t cdr_phase;
  uint8_t cdr_sample_phase;
  uint16_t cdr_edge_level[VLBS_RX_SAMPLES_PER_BIT];
  uint8_t cdr_elapsed;

  vlbs_pcs_state_t pcs_state;
  uint16_t pcs_shift_reg;
  uint8_t pcs_phase;
  uint8_t last_byte;

  void vlbs_phy_cdr_update(uint16_t adc_val);
  bool vlbs_phy_pcs_update(vlbs_cdr_state_t cdr_state);

 public:
  RxPhy();
  void init();
  bool update(uint16_t adc_val, uint8_t *rx_byte);
  inline vlbs_cdr_state_t getCdrState() const { return cdr_state; }
  inline vlbs_pcs_state_t getPcsState() const { return pcs_state; }
};  // class

#ifdef VLBS_IMPLEMENTATION

const int8_t VLBS_PCR_TABLE[1 << VLCFG_SYMBOL_BITS] = {
#if (VLCFG_SYMBOL_BITS) == 5
    VLBS_4B5B_SYM_BAD,    // 0b00000
    VLBS_4B5B_SYM_BAD,    // 0b00001
    VLBS_4B5B_SYM_BAD,    // 0b00010
    VLBS_4B5B_SYM_BAD,    // 0b00011
    VLBS_4B5B_SYM_BAD,    // 0b00100
    0x0,                  // 0b00101
    0x1,                  // 0b00110
    VLBS_4B5B_SYM_BAD,    // 0b00111
    VLBS_4B5B_SYM_BAD,    // 0b01000
    0x2,                  // 0b01001
    VLBS_4B5B_SYM_IDLE0,  // 0b01010
    0x3,                  // 0b01011
    0x4,                  // 0b01100
    0x5,                  // 0b01101
    0x6,                  // 0b01110
    VLBS_4B5B_SYM_BAD,    // 0b01111
    VLBS_4B5B_SYM_BAD,    // 0b10000
    VLBS_4B5B_SYM_IDLE1,  // 0b10001
    0x7,                  // 0b10010
    0x8,                  // 0b10011
    0x9,                  // 0b10100
    0xA,                  // 0b10101
    0xB,                  // 0b10110
    VLBS_4B5B_SYM_BAD,    // 0b10111
    0xC,                  // 0b11000
    0xD,                  // 0b11001
    0xE,                  // 0b11010
    VLBS_4B5B_SYM_BAD,    // 0b11011
    0xF,                  // 0b11100
    VLBS_4B5B_SYM_BAD,    // 0b11101
    VLBS_4B5B_SYM_BAD,    // 0b11110
    VLBS_4B5B_SYM_BAD,    // 0b11111
#else
    VLBS_4B5B_SYM_BAD,    //
    VLBS_4B5B_SYM_BAD,    //
    VLBS_4B5B_SYM_BAD,    //
    VLBS_4B5B_SYM_BAD,    //
    0x1,                  //
    0x0,                  //
    0x0,                  //
    0x0,                  //
    VLBS_4B5B_SYM_IDLE0,  //
    VLBS_4B5B_SYM_IDLE0,  //
    0x2,                  //
    VLBS_4B5B_SYM_IDLE0,  //
    0x1,                  //
    0x1,                  //
    0x2,                  //
    0x2,                  //
    VLBS_4B5B_SYM_BAD,    //
    0x6,                  //
    0x6,                  //
    0x6,                  //
    0x4,                  //
    0x5,                  //
    0x5,                  //
    0x5,                  //
    0x3,                  //
    0x3,                  //
    VLBS_4B5B_SYM_IDLE1,  //
    0x3,                  //
    0x4,                  //
    0x4,                  //
    VLBS_4B5B_SYM_IDLE1,  //
    VLBS_4B5B_SYM_IDLE1,  //
    VLBS_4B5B_SYM_BAD,    //
    0xF,                  //
    0xF,                  //
    0xF,                  //
    0xD,                  //
    0xE,                  //
    0xE,                  //
    0xE,                  //
    0xB,                  //
    0xB,                  //
    0xC,                  //
    0xC,                  //
    0xD,                  //
    0xD,                  //
    0xC,                  //
    VLBS_4B5B_SYM_BAD,    //
    0x7,                  //
    0x7,                  //
    0x8,                  //
    0x7,                  //
    0x9,                  //
    0x9,                  //
    0x8,                  //
    0x8,                  //
    0xB,                  //
    0xA,                  //
    0xA,                  //
    0xA,                  //
    0x9,                  //
    VLBS_4B5B_SYM_BAD,    //
    VLBS_4B5B_SYM_BAD,    //
    VLBS_4B5B_SYM_BAD,    //
#endif
};

RxPhy::RxPhy() { init(); }

void RxPhy::init() {
  cdr_state = VLBS_CDR_LOS;
  cdr_adc_ave = 2048;
  cdr_last_digital_level = false;
  cdr_phase = 0;
  cdr_sample_phase = VLBS_RX_SAMPLES_PER_BIT * 3 / 4;

  pcs_state = VLBS_PCS_LOS;
  pcs_phase = 0;
  pcs_shift_reg = 0;
}

bool RxPhy::update(uint16_t adc_val, uint8_t *rx_byte) {
  vlbs_phy_cdr_update(adc_val);
  bool rxed = vlbs_phy_pcs_update(cdr_state);
  if (rxed && rx_byte) {
    *rx_byte = last_byte;
  }
  return rxed;
}

// clock data recovery
void RxPhy::vlbs_phy_cdr_update(uint16_t adc_val) {
  adc_val = vlbs_u16log2(adc_val);

  // calc ADC average level
  cdr_adc_sum_value += adc_val;
  if (cdr_adc_sum_count < VLBS_ADC_AVE_PERIOD) {
    cdr_adc_sum_count++;
  } else {
    cdr_adc_ave = cdr_adc_sum_value / VLBS_ADC_AVE_PERIOD;
    cdr_adc_sum_value = 0;
    cdr_adc_sum_count = 0;
  }

  // hysteresis threshold
  int16_t thresh = cdr_adc_ave;
  if (cdr_last_digital_level == 0) {
    thresh += 0x100;
  } else {
    thresh -= 0x100;
  }

  // level/edge detection
  bool digital_level = adc_val >= thresh;
  bool edge = (digital_level != cdr_last_digital_level);
  cdr_last_digital_level = digital_level;

  // edge logging
  if (edge) {
    if (cdr_edge_level[cdr_phase] < VLBS_RX_SAMPLES_PER_BIT * 2) {
      cdr_edge_level[cdr_phase] += VLBS_RX_SAMPLES_PER_BIT;
    }
  } else {
    if (cdr_edge_level[cdr_phase] > 0) {
      cdr_edge_level[cdr_phase]--;
    }
  }

  // data phase detection
  // bool signal_detected = false;
  if (edge) {
    uint8_t edge_max_level = 0;
    int edge_max_phase = 0;
    for (int i = 0; i < VLBS_RX_SAMPLES_PER_BIT; i++) {
      if (cdr_edge_level[i] > edge_max_level) {
        edge_max_level = cdr_edge_level[i];
        edge_max_phase = i;
      }
    }
    // signal_detected = edge_max_level >= VLBS_RX_SAMPLES_PER_BIT / 2;
    // cdr_sample_phase = edge_max_phase + VLBS_RX_SAMPLES_PER_BIT * 3 / 4;
    cdr_sample_phase = edge_max_phase + VLBS_RX_SAMPLES_PER_BIT / 2;
    if (cdr_sample_phase >= VLBS_RX_SAMPLES_PER_BIT) {
      cdr_sample_phase -= VLBS_RX_SAMPLES_PER_BIT;
    }
  }

  // data recovery
  if (cdr_elapsed < VLBS_RX_SAMPLES_PER_BIT * 2) {
    cdr_elapsed++;
  }
  /*if (!signal_detected) {
    cdr_state = VLBS_CDR_LOS;
  } else*/
  if (cdr_phase == cdr_sample_phase) {
    const uint8_t tol = (VLBS_RX_SAMPLES_PER_BIT + 4) / 5;
    const uint8_t mn = VLBS_RX_SAMPLES_PER_BIT - tol;
    const uint8_t mx = VLBS_RX_SAMPLES_PER_BIT + tol;
    if (mn <= cdr_elapsed && cdr_elapsed <= mx) {
      cdr_state = digital_level ? VLBS_CDR_RXED_1 : VLBS_CDR_RXED_0;
    } else {
      // unexpected edge timing
      cdr_state = VLBS_CDR_LOS;
    }
    cdr_elapsed = 0;
  } else if (cdr_state == VLBS_CDR_RXED_0 || cdr_state == VLBS_CDR_RXED_1 ||
             cdr_state == VLBS_CDR_IDLE) {
    cdr_state = VLBS_CDR_IDLE;
  } else {
    cdr_state = VLBS_CDR_LOS;
  }

  // step CDR phase
  if (cdr_phase < VLBS_RX_SAMPLES_PER_BIT - 1) {
    cdr_phase++;
  } else {
    cdr_phase = 0;
  }
}

// symbol detection
bool RxPhy::vlbs_phy_pcs_update(vlbs_cdr_state_t cdr_state) {
  if (cdr_state == VLBS_CDR_IDLE) {
    return false;
  } else if (cdr_state != VLBS_CDR_RXED_0 && cdr_state != VLBS_CDR_RXED_1) {
    pcs_state = (cdr_state == VLBS_CDR_LOS) ? VLBS_PCS_LOS : VLBS_PCS_ERROR;
    pcs_shift_reg = 0;
    pcs_phase = 0;
    return false;
  }

  // shift register
  constexpr uint16_t SHIFT_REG_MASK = (1 << (VLCFG_SYMBOL_BITS * 2)) - 1;
  pcs_shift_reg = (pcs_shift_reg << 1) & SHIFT_REG_MASK;
  if (cdr_state == VLBS_CDR_RXED_1) {
    pcs_shift_reg |= 1;
  }

  constexpr uint8_t SYMBOL_MASK = (1 << VLCFG_SYMBOL_BITS) - 1;
  int8_t nibble_h =
      VLBS_PCR_TABLE[(pcs_shift_reg >> VLCFG_SYMBOL_BITS) & SYMBOL_MASK];
  int8_t nibble_l = VLBS_PCR_TABLE[pcs_shift_reg & SYMBOL_MASK];
  bool sym_is_idle =
      nibble_h == VLBS_4B5B_SYM_IDLE0 && nibble_l == VLBS_4B5B_SYM_IDLE1;
  bool rxed = false;
  if (pcs_state == VLBS_PCS_LOS || pcs_state == VLBS_PCS_ERROR) {
    if (sym_is_idle) {
      // symbol lock
      pcs_phase = 0;
      pcs_state = VLBS_PCS_RXED_SYNC1;
    } else {
      pcs_state = VLBS_PCS_LOS;
    }
  } else {
    if (pcs_phase == (VLCFG_SYMBOL_BITS * 2 - 1)) {
      // symbol decode
      switch (pcs_state) {
        case VLBS_PCS_RXED_SYNC1:
          if (sym_is_idle) {
            pcs_state = VLBS_PCS_RXED_SYNC2;
          } else {
            pcs_state = VLBS_PCS_LOS;
          }
          break;

        case VLBS_PCS_RXED_SYNC2:
          if (sym_is_idle) {
            pcs_state = VLBS_PCS_RXED_SYNC3;
          } else {
            pcs_state = VLBS_PCS_LOS;
          }
          break;

        case VLBS_PCS_RXED_SYNC3:
        case VLBS_PCS_RXED_BYTE:
          if (sym_is_idle) {
            pcs_state = VLBS_PCS_RXED_SYNC3;
          } else {
            if (nibble_h >= 0 && nibble_l >= 0) {
              last_byte = (nibble_h << 4) | nibble_l;
              rxed = true;
              pcs_state = VLBS_PCS_RXED_BYTE;
            } else {
              pcs_state = VLBS_PCS_ERROR;
            }
          }
          break;

        default: break;
      }
    }

    // step decoder phase
    if (pcs_phase < (VLCFG_SYMBOL_BITS * 2 - 1)) {
      pcs_phase++;
    } else {
      pcs_phase = 0;
    }
  }

  return rxed;
}

static uint16_t vlbs_u16log2(uint16_t x) {
  if (x == 0) return 0;

  uint16_t ret = 0xc000;
  if (x & 0xf000) {
    if (x & 0xc000) {
      x >>= 2;
      ret += 0x2000;
    }
    if (x & 0x2000) {
      x >>= 1;
      ret += 0x1000;
    }
  } else {
    if (!(x & 0xffc0)) {
      x <<= 6;
      ret -= 0x6000;
    }
    if (!(x & 0xfe00)) {
      x <<= 3;
      ret -= 0x3000;
    }
    if (!(x & 0xf800)) {
      x <<= 2;
      ret -= 0x2000;
    }
    if (!(x & 0xf000)) {
      x <<= 1;
      ret -= 0x1000;
    }
  }

  int index = (x >> 8) & 0xf;
  uint16_t a = VLBS_U16LOG2_TABLE[index];
  uint16_t b = VLBS_U16LOG2_TABLE[index + 1];
  uint16_t q = x & 0xff;
  uint16_t p = 256 - q;
  ret += (a * p + b * q) >> 4;

  return ret;
}

#endif

}  // namespace vlcfg

#endif  // VLBS_RX_PHY_HPP
