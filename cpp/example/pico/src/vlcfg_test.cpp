#include <atomic>
#include <cmath>

#include <hardware/clocks.h>
#include <pico/multicore.h>

#include "ssd1306/ssd1306.hpp"
#include "vlcfg/receiver.hpp"

#include "vlcfg_test.hpp"

Config config;

ssd1306::Display display(DISPLAY_I2C_HOST, DISPLAY_SDA_PORT, DISPLAY_SCL_PORT,
                         DISPLAY_WIDTH, DISPLAY_HEIGHT, 2);
ssd1306::Bitmap canvas(DISPLAY_WIDTH, DISPLAY_HEIGHT);
ssd1306::Bitmap lastCanvas(DISPLAY_WIDTH, DISPLAY_HEIGHT);

std::atomic<bool> displayBusy = false;
std::atomic<bool> ledOn = false;

char ssidBuff[32 + 1];
char passBuff[32 + 1];
char ipBuff[6];
char netMaskBuff[6];
char gatewayBuff[6];
vlcfg::ConfigEntry configEntries[] = {
    {"s", ssidBuff, vlcfg::ValueType::TEXT_STR, sizeof(ssidBuff), 0},
    {"p", passBuff, vlcfg::ValueType::TEXT_STR, sizeof(passBuff), 0},
    {"i", ipBuff, vlcfg::ValueType::BYTE_STR, sizeof(ipBuff), 0},
    {"n", netMaskBuff, vlcfg::ValueType::BYTE_STR, sizeof(netMaskBuff), 0},
    {"g", gatewayBuff, vlcfg::ValueType::BYTE_STR, sizeof(gatewayBuff), 0},
    {nullptr, nullptr, vlcfg::ValueType::NONE, 0, 0}, // terminator
};

vlcfg::Receiver receiver(256);
vlcfg::RxState rxState = vlcfg::RxState::IDLE;
vlcfg::Result rxLastError = vlcfg::Result::SUCCESS;

char lastChar = '0';
char lastStr[32] = "";
int lastStrPos = 0;

void core0_main();
void core1_main();

int main() {
  core0_main();
  return 0;
}

void core0_main() {
  // set_sys_clock_khz(250000, true);
  // sleep_ms(100);

  stdio_init_all();
  sleep_ms(1000);

  adc_init();
  adc_gpio_init(OPT_SENSOR_PORT);
  adc_select_input(OPT_SENSOR_ADC_CH);

  display.i2cBusReset();
  display.init();

  receiver.init(configEntries);

  config.country = CYW43_COUNTRY_JAPAN;
  cyw43_arch_init_with_country(config.country);

  multicore_launch_core1(core1_main);

  uint64_t nextSampleTimeMs = to_ms_since_boot(get_absolute_time());
  uint16_t adcVal = 0;
  uint16_t adcLog[DISPLAY_WIDTH] = {0};
  while (true) {
    uint64_t nowMs = to_ms_since_boot(get_absolute_time());

    if (nowMs >= nextSampleTimeMs) {
      nextSampleTimeMs += 10;

      adcVal = vlcfg::median3(adc_read(), adc_read(), adc_read());

      auto ret = receiver.update(adcVal, &rxState);
      if (ret != vlcfg::Result::SUCCESS) {
        rxLastError = ret;
      }

      for (int i = 0; i < DISPLAY_WIDTH - 1; i++) {
        adcLog[i] = adcLog[i + 1];
      }
      adcLog[DISPLAY_WIDTH - 1] = std::log2(adcVal) * 4096;

      if (!displayBusy.load()) {
#if 0
        int32_t min = 0xFFFF;
        int32_t max = 0;
        for (int i = 0; i < DISPLAY_WIDTH; i++) {
          if (adcLog[i] < min) min = adcLog[i];
          if (adcLog[i] > max) max = adcLog[i];
        }
        int32_t range = max - min;
        if (range < 4096) {
          min -= (4096 - range) / 2;
          range = 4096;
        }
#else
        int32_t min = 0;
        int32_t max = 0xFFFF;
        int32_t range = max - min;
#endif

        canvas.clear();

        {
          bool sd = receiver.signal_detected();
          const char* s = sd ? "ACT" : "LOS";

          if (sd) {
            canvas.fillRect(0, 0, 24, 11, true);
          }
          ssd1306::font6x11.drawStringTo(canvas, s, 2, 0, !sd);

          if (sd) {
            if (receiver.get_last_bit()) {
              canvas.fillRect(28, 0, 8, 11, true);
            } else {
              canvas.drawRect(28, 0, 7, 10, true);
            }
          }
        }

        {
          auto pcsState = receiver.get_pcs_state();
          const char* s;
          switch (pcsState) {
            case vlcfg::PcsState::LOS: s = "LOS"; break;
            case vlcfg::PcsState::RXED_SYNC1: s = "SYNC1"; break;
            case vlcfg::PcsState::RXED_SYNC2: s = "SYNC2"; break;
            case vlcfg::PcsState::RXED_SOF: s = "SOF"; break;
            case vlcfg::PcsState::RXED_BYTE: s = "RXED"; break;
            case vlcfg::PcsState::RXED_EOF: s = "EOF"; break;
            default: s = "(unk)"; break;
          }
          ssd1306::font6x11.drawStringTo(canvas, s, 40, 0, true);
        }

        {
          const char* s;
          switch (rxState) {
            case vlcfg::RxState::IDLE: s = "IDLE"; break;
            case vlcfg::RxState::RECEIVING: s = "RECV"; break;
            case vlcfg::RxState::COMPLETED: s = "CMPL"; break;
            case vlcfg::RxState::ERROR: s = "ERR"; break;
            default: s = "(unk)"; break;
          }
          ssd1306::font6x11.drawStringTo(canvas, s, 80, 0, true);
        }

        {
          char buff[8];
          snprintf(buff, sizeof(buff), "%1d", static_cast<int>(rxLastError));
          ssd1306::font6x11.drawStringTo(canvas, buff, 114, 0, true);
        }

        {
          char buff[sizeof(ssidBuff) + 8];
          snprintf(buff, sizeof(buff), "SSID: '%s'", ssidBuff);
          ssd1306::font6x11.drawStringTo(canvas, buff, 0, 12, true);
        }

        {
          char buff[sizeof(passBuff) + 8];
          snprintf(buff, sizeof(buff), "PASS: '%s'", passBuff);
          ssd1306::font6x11.drawStringTo(canvas, buff, 0, 24, true);
        }

        for (int x = 0; x < DISPLAY_WIDTH; x++) {
          int h = 1 + (adcLog[x] - min) * 24 / range;
          int y = DISPLAY_HEIGHT - h;
          canvas.fillRect(x, y, 1, h, true);
        }

        displayBusy.store(true);
      }
    }

    ledOn = receiver.get_pcs_state() != vlcfg::PcsState::LOS;
  }
}

void core1_main() {
  bool lastLedOn = false;
  while (true) {
    bool currLedOn = ledOn.load();
    if (currLedOn != lastLedOn) {
      cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, currLedOn);
    }

    if (displayBusy.load()) {
      for (int row = 0; row < DISPLAY_ROWS; row++) {
        int diffMin = DISPLAY_WIDTH + 1;
        int diffMax = -1;
        for (int col = 0; col < DISPLAY_WIDTH; col++) {
          int idx = row * DISPLAY_WIDTH + col;
          if (canvas.data[idx] != lastCanvas.data[idx]) {
            if (col < diffMin) diffMin = col;
            if (col > diffMax) diffMax = col;
          }
        }
        if (diffMax < diffMin) {
          continue;
        }
        int diffSize = diffMax - diffMin + 1;
        display.setWindow(diffMin, row * 8, diffSize, 8);
        display.writePixels(&canvas.data[row * DISPLAY_WIDTH + diffMin],
                            diffSize);
      }
      canvas.copyTo(lastCanvas);
      displayBusy.store(false);
    }

    sleep_us(100);
  }
}
