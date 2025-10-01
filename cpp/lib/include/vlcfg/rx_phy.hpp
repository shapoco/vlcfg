#ifndef VLCFG_RX_PHY_HPP
#define VLCFG_RX_PHY_HPP

#include "vlcfg/common.hpp"
#include "vlcfg/rx_cdr.hpp"
#include "vlcfg/rx_pcs.hpp"

namespace vlcfg {

class RxPhy {
  // todo: private:
 public:
  RxCdr cdr;
  RxPcs pcs;
  bool last_bit;
  uint8_t last_byte;

 public:
  RxPhy();
  void init();
  bool update(uint16_t adc_val, uint8_t *rx_byte);
  inline bool signal_detected() const { return cdr.signal_detected(); }
  inline PcsState get_pcs_state() const { return pcs.get_state(); }
  inline bool get_last_bit() const { return last_bit; }
  inline uint8_t get_last_byte() const { return last_byte; }
};  // class

#ifdef VLCFG_IMPLEMENTATION

RxPhy::RxPhy() { init(); }

void RxPhy::init() { cdr.init(); }

bool RxPhy::update(uint16_t adc_val, uint8_t *rx_byte) {
  CdrOutput cdrOut;
  PcsOutput pcsOut;
  cdr.update(adc_val, &cdrOut);
  if (cdrOut.rxed) {
    last_bit = cdrOut.rx_bit;
  }
  pcs.update(&cdrOut, &pcsOut);
  if (pcsOut.rxed) {
    last_byte = pcsOut.rx_byte;
    if (rx_byte != nullptr) {
      *rx_byte = last_byte;
    }
  }
  return pcsOut.rxed;
}

#endif

}  // namespace vlcfg

#endif  // VLBS_RX_PHY_HPP
