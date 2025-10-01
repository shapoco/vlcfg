#ifndef VLCFG_RECEIVER_HPP
#define VLCFG_RECEIVER_HPP

#include "vlcfg/common.hpp"
#include "vlcfg/rx_cdr.hpp"
#include "vlcfg/rx_core.hpp"
#include "vlcfg/rx_pcs.hpp"

namespace vlcfg {

class Receiver {
  // todo: private:
 public:
  RxCdr cdr;
  RxPcs pcs;
  RxCore core;
  bool last_bit;
  uint8_t last_byte;

 public:
  inline Receiver(int rx_buff_size = 256, ConfigEntry *entries = nullptr,
                  uint8_t num_entries = 0)
      : core(rx_buff_size) {
    init(entries, num_entries);
  }
  void init(ConfigEntry *entries, uint8_t num_entries);
  Result update(uint16_t adc_val, RxState *rx_state);
  inline bool signal_detected() const { return cdr.signal_detected(); }
  inline PcsState get_pcs_state() const { return pcs.get_state(); }
  inline bool get_last_bit() const { return last_bit; }
  inline uint8_t get_last_byte() const { return last_byte; }
};  // class

#ifdef VLCFG_IMPLEMENTATION

void Receiver::init(ConfigEntry *entries, uint8_t num_entries) {
  cdr.init();
  pcs.init();
  core.init(entries, num_entries);
}

Result Receiver::update(uint16_t adc_val, RxState *rx_state) {
  Result ret;

  CdrOutput cdrOut;
  ret = cdr.update(adc_val, &cdrOut);
  if (ret != Result::SUCCESS) return ret;
  if (cdrOut.rxed) last_bit = cdrOut.rx_bit;

  PcsOutput pcsOut;
  ret = pcs.update(&cdrOut, &pcsOut);
  if (ret != Result::SUCCESS) return ret;
  if (pcsOut.rxed) last_byte = pcsOut.rx_byte;

  return core.update(&pcsOut, rx_state);
}

#endif

}  // namespace vlcfg

#endif  // VLBS_RX_PHY_HPP
