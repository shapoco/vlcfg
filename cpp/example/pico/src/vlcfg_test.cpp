#include <atomic>
#include <cmath>

#include <hardware/clocks.h>
#include <pico/multicore.h>

#include "ssd1306/ssd1306.hpp"
#include "vlcfg/rx_core.hpp"

#include "vlcfg_test.hpp"

Config config;

ssd1306::Display display(DISPLAY_I2C_HOST, DISPLAY_SDA_PORT, DISPLAY_SCL_PORT,
                         DISPLAY_WIDTH, DISPLAY_HEIGHT, 2);
ssd1306::Bitmap canvas(DISPLAY_WIDTH, DISPLAY_HEIGHT);
ssd1306::Bitmap lastCanvas(DISPLAY_WIDTH, DISPLAY_HEIGHT);

std::atomic<bool> displayBusy = false;
std::atomic<bool> ledOn = false;

vlcfg::RxPhy rxPhy;
vlcfg::RxCore rxCore(1024);

char ssidBuff[32 + 1];
char passBuff[64 + 1];
vlcfg::ConfigEntry configEntries[] = {
    {"s", ssidBuff, vlcfg::CborMajorType::TEXT_STR, (uint16_t)sizeof(ssidBuff),
     vlcfg::ConfigEntryFlags::NONE},
    {"p", passBuff, vlcfg::CborMajorType::TEXT_STR, (uint16_t)sizeof(passBuff),
     vlcfg::ConfigEntryFlags::NONE},
};
constexpr int NUM_CONFIG_ENTRIES =
    sizeof(configEntries) / sizeof(configEntries[0]);

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
  set_sys_clock_khz(250000, true);
  sleep_ms(100);

  stdio_init_all();
  sleep_ms(1000);
  // printf("Hello, VL-CFG!\n");

  adc_init();
  // constexpr int ADC_FREQ = 100000;
  // constexpr int ADC_CLKDIV = 48000000 / ADC_FREQ;
  // adc_set_clkdiv(ADC_CLKDIV);
  adc_gpio_init(OPT_SENSOR_PORT);
  adc_select_input(OPT_SENSOR_ADC_CH);

  display.i2cBusReset();
  display.init();

  rxCore.init(configEntries, NUM_CONFIG_ENTRIES);

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

      {
        constexpr int FILTER_SIZE = 5;
        uint16_t medianBuff[FILTER_SIZE];
        for (int i = 0; i < FILTER_SIZE; i++) {
          medianBuff[i] = adc_read();
        }
        // insertion sort
        for (int i = 1; i < FILTER_SIZE; i++) {
          uint16_t v = medianBuff[i];
          int j = i - 1;
          while (j >= 0 && medianBuff[j] > v) {
            medianBuff[j + 1] = medianBuff[j];
            j--;
          }
          medianBuff[j + 1] = v;
        }
        adcVal = medianBuff[FILTER_SIZE / 2];
      }

      {
        uint8_t rxByte;
        bool rxed = rxPhy.update(adcVal, &rxByte);
        if (rxed) {
          lastChar = rxByte;
          if (lastStrPos < (int)sizeof(lastStr) - 1) {
            lastStr[lastStrPos++] = lastChar;
            lastStr[lastStrPos] = '\0';
          }
        } else if (rxPhy.get_pcs_state() == vlcfg::PcsState::LOS) {
          lastChar = '\0';
          lastStr[0] = '\0';
          lastStrPos = 0;
        }
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
          bool sd = rxPhy.signal_detected();
          const char* s = sd ? "DET" : "LOS";
          char buff[32];
          snprintf(buff, sizeof(buff), "cdr:%s", s);
          // snprintf(buff, sizeof(buff), "cdr:%d", rxPhy.cdr.sample_phase);
          ssd1306::font6x11.drawStringTo(canvas, buff, 0, 0, true);

          if (sd && rxPhy.get_last_bit()) {
            canvas.fillRect(56, 0, 4, 8, true);
          }
        }

        {
          const char* s;
          switch (rxPhy.get_pcs_state()) {
            case vlcfg::PcsState::LOS: s = "LOS"; break;
            case vlcfg::PcsState::RXED_SYNC1: s = "SYNC1"; break;
            case vlcfg::PcsState::RXED_SYNC2: s = "SYNC2"; break;
            case vlcfg::PcsState::RXED_SYNC3: s = "SYNC3"; break;
            case vlcfg::PcsState::RXED_BYTE: s = "RXED"; break;
            case vlcfg::PcsState::ERROR: s = "ERROR"; break;
            default: s = "(unk)"; break;
          }
          char buff[32];
          snprintf(buff, sizeof(buff), "pcs:%s", s);
          ssd1306::font6x11.drawStringTo(canvas, buff, 64, 0, true);
        }

        {
          char buff[32];
          if (lastChar != 0) {
            snprintf(buff, sizeof(buff), "chr:'%c' (0x%02x)\n", lastChar,
                     (int)lastChar);
          } else {
            snprintf(buff, sizeof(buff), "chr:'\\0' (0x00)\n");
          }
          ssd1306::font6x11.drawStringTo(canvas, buff, 0, 12, true);
        }

        {
          char buff[64];
          snprintf(buff, sizeof(buff), "str:\"%s\"", lastStr);
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

    ledOn = rxPhy.get_pcs_state() != vlcfg::PcsState::LOS;
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
