#include <cmath>

#include <hardware/clocks.h>

#include "ssd1306/ssd1306.hpp"
#include "vlcfg/rx_core.hpp"

#include "vlcfg_test.hpp"

Config config;

ssd1306::Display display(DISPLAY_I2C_HOST, DISPLAY_SDA_PORT, DISPLAY_SCL_PORT,
                         DISPLAY_WIDTH, DISPLAY_HEIGHT, 2);
ssd1306::Bitmap canvas(DISPLAY_WIDTH, DISPLAY_HEIGHT);

vlcfg::RxPhy rxPhy;
vlcfg::RxCore rxCore(1024);

char ssidBuff[32 + 1];
char passBuff[64 + 1];
vlcfg::ConfigEntry configEntries[] = {
    {"s", ssidBuff, vlcfg::CborMajorType::TEXT_STR, (uint16_t)sizeof(ssidBuff),
     vlcfg::VLCFG_ENTRY_FLAG_DUMMY},
    {"p", passBuff, vlcfg::CborMajorType::TEXT_STR, (uint16_t)sizeof(passBuff),
     vlcfg::VLCFG_ENTRY_FLAG_DUMMY},
};
constexpr int NUM_CONFIG_ENTRIES =
    sizeof(configEntries) / sizeof(configEntries[0]);

char lastChar = '0';
char lastStr[32] = "";
int lastStrPos = 0;

int main() {
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

  display.init();
  // display.i2cBusReset();

  rxCore.init(configEntries, NUM_CONFIG_ENTRIES);

  config.country = CYW43_COUNTRY_JAPAN;
  cyw43_arch_init_with_country(config.country);

  bool ledOn = false;
  uint64_t nextSampleTimeMs = to_ms_since_boot(get_absolute_time());
  uint16_t adcVal = 0;
  uint16_t adcLog[DISPLAY_WIDTH] = {0};
  int nextUpdateRow = 0;
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
        } else if (rxPhy.getPcsState() == vlcfg::VLBS_PCS_LOS) {
          lastChar = '\0';
          lastStr[0] = '\0';
          lastStrPos = 0;
        }
      }

      for (int i = 0; i < DISPLAY_WIDTH - 1; i++) {
        adcLog[i] = adcLog[i + 1];
      }
      adcLog[DISPLAY_WIDTH - 1] = std::log2(adcVal) * 4096;

      if (nextUpdateRow == 0) {
        uint16_t min = 0xFFFF;
        uint16_t max = 0;
        for (int i = 0; i < DISPLAY_WIDTH; i++) {
          if (adcLog[i] < min) min = adcLog[i];
          if (adcLog[i] > max) max = adcLog[i];
        }
        uint16_t range = max - min;
        if (range < 16) range = 16;

        canvas.clear();

        {
          const char* s;
          switch (rxPhy.getCdrState()) {
            case vlcfg::vlbs_cdr_state_t::VLBS_CDR_LOS: s = "LOS"; break;
            case vlcfg::vlbs_cdr_state_t::VLBS_CDR_IDLE: s = "IDLE"; break;
            case vlcfg::vlbs_cdr_state_t::VLBS_CDR_RXED_0: s = "RXED_0"; break;
            case vlcfg::vlbs_cdr_state_t::VLBS_CDR_RXED_1: s = "RXED_1"; break;
            default: s = "(unk)"; break;
          }
          ssd1306::font6x11.drawStringTo(canvas, s, 0, 0, true);
        }

        {
          const char* s;
          switch (rxPhy.getPcsState()) {
            case vlcfg::vlbs_pcs_state_t::VLBS_PCS_LOS: s = "LOS"; break;
            case vlcfg::vlbs_pcs_state_t::VLBS_PCS_RXED_SYNC1:
              s = "RXED_SYNC1";
              break;
            case vlcfg::vlbs_pcs_state_t::VLBS_PCS_RXED_SYNC2:
              s = "RXED_SYNC2";
              break;
            case vlcfg::vlbs_pcs_state_t::VLBS_PCS_RXED_SYNC3:
              s = "RXED_SYNC3";
              break;
            case vlcfg::vlbs_pcs_state_t::VLBS_PCS_RXED_BYTE:
              s = "RXED_BYTE";
              break;
            case vlcfg::vlbs_pcs_state_t::VLBS_PCS_ERROR: s = "ERROR"; break;
            default: s = "(unk)"; break;
          }
          ssd1306::font6x11.drawStringTo(canvas, s, 56, 0, true);
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
      }
      display.setWindow(0, nextUpdateRow * 8, DISPLAY_WIDTH, 8);
      display.writePixels(&canvas.data[nextUpdateRow * DISPLAY_WIDTH],
                          DISPLAY_WIDTH);
      nextUpdateRow = (nextUpdateRow + 1) % DISPLAY_ROWS;
    }

    bool lastLedOn = ledOn;
    // ledOn = (nowMs % 1000) < 500;
    ledOn = rxPhy.getPcsState() != vlcfg::VLBS_PCS_LOS;
    if (ledOn != lastLedOn) {
      cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, ledOn);
    }
    sleep_us(100);
  }

  return 0;
}
