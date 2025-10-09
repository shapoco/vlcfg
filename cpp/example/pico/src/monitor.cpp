#include <cmath>

#include <pico/multicore.h>

#include "ssd1306/ssd1306.hpp"

#include "button.hpp"
#include "monitor.hpp"

enum class MonitorMode {
  INTERNAL_STATE,
  ENTRY_LIST,
  MODE_COUNT,
};

ssd1306::Display display(DISPLAY_I2C_HOST, DISPLAY_SDA_PORT, DISPLAY_SCL_PORT,
                         DISPLAY_WIDTH, DISPLAY_HEIGHT, 2);
ssd1306::Bitmap work_screen(DISPLAY_WIDTH, DISPLAY_HEIGHT);
ssd1306::Bitmap last_screen(DISPLAY_WIDTH, DISPLAY_HEIGHT);

Button monitor_button(MONITOR_BUTTON_PORT);

std::atomic<bool> display_busy = false;
std::atomic<bool> led_on = false;

uint16_t adc_log[DISPLAY_WIDTH] = {0};

vlcfg::Result rx_last_error = vlcfg::Result::SUCCESS;

static constexpr int RX_LOG_SIZE = DISPLAY_WIDTH / 7 + 1;
char rx_log[RX_LOG_SIZE] = {' '};

static void core1_main();
static void render_internal_state(vlcfg::Receiver &receiver);
static void render_entry_list(vlcfg::Receiver &receiver);

static MonitorMode mode = MonitorMode::INTERNAL_STATE;

void monitor_init() {
  monitor_button.init();
  display.i2cBusReset();
  display.init();
  multicore_launch_core1(core1_main);
}

void monitor_update(uint16_t adc_val, vlcfg::Result error,
                    vlcfg::Receiver &receiver) {
  monitor_button.update();
  if (monitor_button.on_clicked()) {
    mode = static_cast<MonitorMode>((static_cast<int>(mode) + 1) %
                                    static_cast<int>(MonitorMode::MODE_COUNT));
  }

  for (int i = 0; i < DISPLAY_WIDTH - 1; i++) {
    adc_log[i] = adc_log[i + 1];
  }
  adc_log[DISPLAY_WIDTH - 1] = std::log2(adc_val) * 4096;

  if (error != vlcfg::Result::SUCCESS) {
    rx_last_error = error;
  }

  int8_t pcs_symbol = receiver.pcs.dbg_rxed_symbol;
  if (pcs_symbol != vlcfg::SYMBOL_NONE) {
    for (int i = 0; i < RX_LOG_SIZE - 2; i++) {
      rx_log[i] = rx_log[i + 1];
    }
    char log_c;
    switch (pcs_symbol) {
      case vlcfg::SYMBOL_SOF: log_c = 's'; break;
      case vlcfg::SYMBOL_EOF: log_c = 'e'; break;
      case vlcfg::SYMBOL_SYNC: log_c = 'y'; break;
      case vlcfg::SYMBOL_CTRL: log_c = '\\'; break;
      case vlcfg::SYMBOL_INVALID: log_c = 'x'; break;
      default:
        if (0 <= pcs_symbol && pcs_symbol < 10) {
          log_c = '0' + pcs_symbol;
        } else if (10 <= pcs_symbol && pcs_symbol < 16) {
          log_c = 'A' + (pcs_symbol - 10);
        } else {
          log_c = '?';
        }
        break;
    }
    rx_log[RX_LOG_SIZE - 2] = log_c;
  }

  if (!display_busy.load()) {
    switch (mode) {
      case MonitorMode::INTERNAL_STATE: render_internal_state(receiver); break;
      case MonitorMode::ENTRY_LIST: render_entry_list(receiver); break;
      default: break;
    }
    display_busy.store(true);
  }
}

static void render_internal_state(vlcfg::Receiver &receiver) {
#if 0
  int32_t min = 0xFFFF;
  int32_t max = 0;
  for (int i = 0; i < DISPLAY_WIDTH; i++) {
    if (adc_log[i] < min) min = adc_log[i];
    if (adc_log[i] > max) max = adc_log[i];
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

  work_screen.clear();

  constexpr int TEXT_HEIGHT = 12;

  // Graph
  {
    int h = DISPLAY_HEIGHT - TEXT_HEIGHT * 2 - 2;
    for (int x = 0; x < DISPLAY_WIDTH; x++) {
      int gh = 1 + (adc_log[x] - min) * h / range;
      int gy = h - gh;
      work_screen.fillRect(x, gy, 1, gh, true);
    }
  }

  {
    int y = DISPLAY_HEIGHT - TEXT_HEIGHT * 2;

    // CDR State
    {
      bool sd = receiver.signal_detected();
      const char *s = sd ? "ACT" : "LOS";

      if (sd) {
        work_screen.fillRect(0, y, 24, TEXT_HEIGHT - 1, true);
      }
      ssd1306::font6x11.drawStringTo(work_screen, s, 2, y, !sd);

      if (sd) {
        if (receiver.get_last_bit()) {
          work_screen.fillRect(28, y, 8, TEXT_HEIGHT - 1, true);
        } else {
          work_screen.drawRect(28, y, 7, TEXT_HEIGHT - 2, true);
        }
      }
    }

    // PCS State
    {
      auto pcs_state = receiver.get_pcs_state();
      const char *s;
      switch (pcs_state) {
        case vlcfg::PcsState::LOS: s = "LOS"; break;
        case vlcfg::PcsState::RXED_SYNC1: s = "SYNC1"; break;
        case vlcfg::PcsState::RXED_SYNC2: s = "SYNC2"; break;
        case vlcfg::PcsState::RXED_SOF: s = "SOF"; break;
        case vlcfg::PcsState::RXED_BYTE: s = "RXED"; break;
        case vlcfg::PcsState::RXED_EOF: s = "EOF"; break;
        default: s = "(unk)"; break;
      }
      ssd1306::font6x11.drawStringTo(work_screen, s, 40, y, true);
    }

    // Decoder State
    {
      auto rx_state = receiver.get_decoder_state();

      const char *s;
      switch (rx_state) {
        case vlcfg::RxState::IDLE: s = "IDLE"; break;
        case vlcfg::RxState::RECEIVING: s = "RECV"; break;
        case vlcfg::RxState::COMPLETED: s = "CMPL"; break;
        case vlcfg::RxState::ERROR: s = "ERR"; break;
        default: s = "(unk)"; break;
      }
      ssd1306::font6x11.drawStringTo(work_screen, s, 80, y, true);
    }

    // Last Error Code
    {
      char buff[8];
      snprintf(buff, sizeof(buff), "%1d", static_cast<int>(rx_last_error));
      ssd1306::font6x11.drawStringTo(work_screen, buff, 114, y, true);
    }
  }

  // Rx Log
  {
    int y = DISPLAY_HEIGHT - TEXT_HEIGHT;
    rx_log[RX_LOG_SIZE - 1] = '\0';
    ssd1306::font6x11.drawStringTo(work_screen, rx_log, 0, y, true);
  }
}

static void render_entry_list(vlcfg::Receiver &receiver) {
  work_screen.clear();
  int y = 0;
  for (int i = 0; i < vlcfg::MAX_ENTRY_COUNT; i++) {
    int x = 0;
    vlcfg::ConfigEntry *e = &configEntries[i];
    if (e->key == nullptr) break;
    x += ssd1306::font6x11.drawStringTo(work_screen, e->key, x, y, true);
    x += ssd1306::font6x11.drawStringTo(work_screen, ":", x, y, true);
    if (e->was_received()) {
      switch (e->type) {
        case vlcfg::ValueType::TEXT_STR: {
          char buff[128];
          snprintf(buff, sizeof(buff), "'%s'", (const char *)e->buffer);
          x += ssd1306::font6x11.drawStringTo(work_screen, buff, x, y, true);
        } break;

        case vlcfg::ValueType::BYTE_STR: {
          uint8_t *b = (uint8_t *)e->buffer;
          char buff[4];
          for (int i = 0; i < e->received; i++) {
            snprintf(buff, sizeof(buff), "%02X", b[i]);
            x += ssd1306::font6x11.drawStringTo(work_screen, buff, x, y, true);
            x += 2;
          }
        } break;

        case vlcfg::ValueType::UINT: {
          uint64_t v = 0;
          switch (e->capacity) {
            case 1: v = *(uint8_t *)e->buffer; break;
            case 2: v = *(uint16_t *)e->buffer; break;
            case 4: v = *(uint32_t *)e->buffer; break;
            case 8: v = *(uint64_t *)e->buffer; break;
            default: break;
          }
          char buff[32];
          snprintf(buff, sizeof(buff), "%llu", v);
          x += ssd1306::font6x11.drawStringTo(work_screen, buff, x, y, true);
        } break;

        case vlcfg::ValueType::INT: {
          int64_t v = 0;
          switch (e->capacity) {
            case 1: v = *(int8_t *)e->buffer; break;
            case 2: v = *(int16_t *)e->buffer; break;
            case 4: v = *(int32_t *)e->buffer; break;
            case 8: v = *(int64_t *)e->buffer; break;
            default: break;
          }
          char buff[32];
          snprintf(buff, sizeof(buff), "%lld", v);
          x += ssd1306::font6x11.drawStringTo(work_screen, buff, x, y, true);
        } break;

        case vlcfg::ValueType::BOOLEAN: {
          bool v = false;
          if (e->capacity == 1) {
            v = (*(uint8_t *)e->buffer) != 0;
          }
          x += ssd1306::font6x11.drawStringTo(work_screen, v ? "true" : "false",
                                              x, y, true);
        } break;

        default: break;
      }
    } else {
      x += ssd1306::font6x11.drawStringTo(work_screen, "(none)", x, y, true);
    }
    y += 12;
  }
}

static void core1_main() {
  bool last_led_on = false;
  while (true) {
    bool curr_led_on = led_on.load();
    if (curr_led_on != last_led_on) {
      cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, curr_led_on);
    }

    if (display_busy.load()) {
      for (int row = 0; row < DISPLAY_ROWS; row++) {
        int change_x_start = DISPLAY_WIDTH + 1;
        int change_x_end = -1;
        for (int col = 0; col < DISPLAY_WIDTH; col++) {
          int idx = row * DISPLAY_WIDTH + col;
          if (work_screen.data[idx] != last_screen.data[idx]) {
            if (col < change_x_start) change_x_start = col;
            if (col > change_x_end) change_x_end = col;
          }
        }
        if (change_x_end < change_x_start) {
          continue;
        }
        int change_width = change_x_end - change_x_start + 1;
        display.setWindow(change_x_start, row * 8, change_width, 8);
        display.writePixels(
            &work_screen.data[row * DISPLAY_WIDTH + change_x_start],
            change_width);
      }
      work_screen.copyTo(last_screen);
      display_busy.store(false);
    }

    sleep_us(100);
  }
}
