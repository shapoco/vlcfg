#ifndef MONITOR_HPP
#define MONITOR_HPP

#include "vlcfg_test.hpp"

void monitor_init();
void monitor_update(uint16_t adc_val, vlcfg::Result error,
                    vlcfg::Receiver& receiver);

#endif
