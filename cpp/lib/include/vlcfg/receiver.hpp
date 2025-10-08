#ifndef VLCFG_RECEIVER_HPP
#define VLCFG_RECEIVER_HPP

#include "vlcfg/common.hpp"
#include "vlcfg/rx_cdr.hpp"
#include "vlcfg/rx_decoder.hpp"
#include "vlcfg/rx_pcs.hpp"

namespace vlcfg {

class Receiver {
 public:
  RxCdr cdr;
  RxPcs pcs;
  RxDecoder decoder;

 private:
  bool last_bit;
  uint8_t last_byte;

 public:
  inline Receiver(int rx_buff_size = 256, ConfigEntry *entries = nullptr)
      : decoder(rx_buff_size) {
    init(entries);
  }

  void init(ConfigEntry *entries);
  Result update(uint16_t adc_val, RxState *rx_state);

  inline bool signal_detected() const { return cdr.signal_detected(); }
  inline PcsState get_pcs_state() const { return pcs.get_state(); }
  inline RxState get_decoder_state() const { return decoder.get_state(); }

  inline bool get_last_bit() const { return last_bit; }
  inline uint8_t get_last_byte() const { return last_byte; }

  inline ConfigEntry *entry_from_key(const char *key) const {
    return decoder.entry_from_key(key);
  }
};  // class

#ifdef VLCFG_IMPLEMENTATION

void Receiver::init(ConfigEntry *entries) {
  cdr.init();
  pcs.init();
  decoder.init(entries);
  VLCFG_PRINTF("Receiver initialized.\n");
}

Result Receiver::update(uint16_t adc_val, RxState *rx_state) {
  CdrOutput cdrOut;
  VLCFG_TRY(cdr.update(adc_val, &cdrOut));
  if (cdrOut.rxed) last_bit = cdrOut.rx_bit;

  PcsOutput pcsOut;
  VLCFG_TRY(pcs.update(&cdrOut, &pcsOut));
  if (pcsOut.rxed) last_byte = pcsOut.rx_byte;

  VLCFG_TRY(decoder.update(&pcsOut, rx_state));

  return Result::SUCCESS;
}

#endif

}  // namespace vlcfg

#endif
