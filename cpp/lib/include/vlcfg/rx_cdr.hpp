#ifndef VLCFG_RX_CDR_HPP
#define VLCFG_RX_CDR_HPP

#include "vlcfg/common.hpp"

namespace vlcfg {

static const uint16_t PHASE_PERIOD = VLBS_RX_SAMPLES_PER_BIT;
static const uint16_t ADC_AVE_PERIOD = PHASE_PERIOD * SYMBOL_BITS * 2;
static const uint8_t ADC_BITS = 12;

class RxCdr {
  // todo: private:
 public:
  bool amp_det;
  uint16_t sig_det_count;
  bool sig_det;
  uint32_t adc_sum_value;
  uint8_t adc_sum_count;
  uint16_t adc_max_value;
  uint16_t adc_min_value;
  uint16_t adc_ave;
  uint8_t last_digital_level;
  uint8_t phase;
  uint8_t sample_phase;
  uint16_t edge_level[PHASE_PERIOD];

 public:
  inline RxCdr() { init(); }
  void init();
  void update(uint16_t adc_val, CdrOutput& out);
  inline bool signal_detected() const { return sig_det; }
};

#ifdef VLCFG_IMPLEMENTATION
static uint16_t u16log2(uint16_t x);

static const uint16_t U16LOG2_TABLE[17] = {
    0,   22,  44,  63,  82,  100, 118, 134, 150,
    165, 179, 193, 207, 220, 232, 244, 256,
};

void RxCdr::init() {
  sig_det_count = 0;
  amp_det = false;
  sig_det = false;
  adc_ave = 2048;
  last_digital_level = false;
  phase = 0;
  sample_phase = PHASE_PERIOD * 3 / 4;
  adc_min_value = 9999;
  adc_max_value = 0;
}

// clock data recovery
void RxCdr::update(uint16_t adc_val, CdrOutput& out) {
  out.rxed = false;

  adc_sum_count++;
  bool sd_trig = (adc_sum_count >= ADC_AVE_PERIOD);
  if (sd_trig) adc_sum_count = 0;

  if (adc_val > adc_max_value) adc_max_value = adc_val;
  if (adc_val < adc_min_value) adc_min_value = adc_val;
  if (sd_trig) {
    amp_det = (adc_max_value - adc_min_value) >= (1 << (ADC_BITS - 6));
    adc_max_value = adc_val;
    adc_min_value = adc_val;
  }

  bool los = !amp_det;

  uint16_t adc_val_lg2 = u16log2(adc_val);

  // calc ADC average level
  adc_sum_value += adc_val_lg2;
  if (sd_trig) {
    adc_ave = adc_sum_value / ADC_AVE_PERIOD;
    adc_sum_value = 0;
  }

  // hysteresis threshold
  int16_t thresh = adc_ave;
  if (last_digital_level == 0) {
    thresh += 0x100;
  } else {
    thresh -= 0x100;
  }

  // level/edge detection
  bool digital_level = adc_val_lg2 >= thresh;
  bool edge = (digital_level != last_digital_level);
  last_digital_level = digital_level;

  // edge logging
  if (edge) {
    if (edge_level[phase] < PHASE_PERIOD * 2) {
      edge_level[phase] += PHASE_PERIOD;
    }
  } else {
    if (edge_level[phase] > 0) {
      edge_level[phase]--;
    }
  }

  // data phase detection
  if (edge) {
    uint8_t edge_max_level = 0;
    int edge_max_phase = 0;
    for (int i = 0; i < PHASE_PERIOD; i++) {
      if (edge_level[i] > edge_max_level) {
        edge_max_level = edge_level[i];
        edge_max_phase = i;
      }
    }
    auto last_phase = sample_phase;
    sample_phase = edge_max_phase + PHASE_PERIOD / 2;
    if (sample_phase >= PHASE_PERIOD) {
      sample_phase -= PHASE_PERIOD;
    }

#if 0
    // ディスプレイの点滅のジッタがかなり大きいので
    // 位相変化のチェックはしない
    int8_t phase_diff = sample_phase - last_phase;
    if (phase_diff < -PHASE_PERIOD / 2) {
      phase_diff += PHASE_PERIOD;
    } else if (phase_diff > PHASE_PERIOD / 2) {
      phase_diff -= PHASE_PERIOD;
    }
    constexpr int8_t TOL = PHASE_PERIOD / 4;
    los |= (phase_diff < -TOL || TOL < phase_diff);
#endif
  }

  // signal detection
  if (los) {
    sig_det_count = 0;
    sig_det = false;
  } else if (sig_det_count < PHASE_PERIOD * 4) {
    sig_det_count++;
    sig_det = false;
  } else {
    sig_det = true;
  }
  out.signal_detected = sig_det;

  // data recovery
  if (sig_det && phase == sample_phase) {
    const uint8_t tol = (PHASE_PERIOD + 4) / 5;
    const uint8_t mn = PHASE_PERIOD - tol;
    const uint8_t mx = PHASE_PERIOD + tol;
    out.rxed = true;
    out.rx_bit = digital_level;
  }

  // step CDR phase
  if (phase < PHASE_PERIOD - 1) {
    phase++;
  } else {
    phase = 0;
  }
}

static uint16_t u16log2(uint16_t x) {
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
  uint16_t a = U16LOG2_TABLE[index];
  uint16_t b = U16LOG2_TABLE[index + 1];
  uint16_t q = x & 0xff;
  uint16_t p = 256 - q;
  ret += (a * p + b * q) >> 4;

  return ret;
}

#endif

}  // namespace vlcfg

#endif