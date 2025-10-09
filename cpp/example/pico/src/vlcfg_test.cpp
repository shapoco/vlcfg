#include <hardware/clocks.h>

#include "button.hpp"
#include "monitor.hpp"
#include "vlcfg_test.hpp"

const char *KEY_TEXT = "t";
const char *KEY_PASS = "p";
const char *KEY_NUMBER = "n";
const char *KEY_IP_ADDR = "i";
const char *KEY_LED_ON = "l";
char text_buff[32 + 1];
char pass_buff[32 + 1];
int32_t number_buff;
uint8_t ip_buff[6];
uint8_t bool_buff;
vlcfg::ConfigEntry configEntries[] = {
    {KEY_TEXT, text_buff, vlcfg::ValueType::TEXT_STR, sizeof(text_buff)},
    {KEY_PASS, pass_buff, vlcfg::ValueType::TEXT_STR, sizeof(pass_buff)},
    {KEY_NUMBER, &number_buff, vlcfg::ValueType::INT, sizeof(number_buff)},
    {KEY_IP_ADDR, ip_buff, vlcfg::ValueType::BYTE_STR, sizeof(ip_buff)},
    {KEY_LED_ON, &bool_buff, vlcfg::ValueType::BOOLEAN, sizeof(bool_buff)},
    {nullptr, nullptr, vlcfg::ValueType::NONE, 0},  // terminator
};
bool received = false;

vlcfg::Receiver receiver(256);

Button restart_button(RESTART_BUTTON_PORT);

void core0_main();
void on_received();

int main() {
  core0_main();
  return 0;
}

void core0_main() {
  // set_sys_clock_khz(250000, true);
  // sleep_ms(100);

  stdio_init_all();
  sleep_ms(1000);

  restart_button.init();

  adc_init();
  adc_gpio_init(OPT_SENSOR_PORT);
  adc_select_input(OPT_SENSOR_ADC_CH);

  gpio_init(LED_PORT);
  gpio_set_dir(LED_PORT, GPIO_OUT);
  gpio_put(LED_PORT, false);

  receiver.init(configEntries);

  cyw43_arch_init_with_country(CYW43_COUNTRY_JAPAN);
  cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);

  monitor_init();

  uint64_t next_sample_time_ms = to_ms_since_boot(get_absolute_time());
  uint16_t adc_val = 0;
  while (true) {
    uint64_t now_ms = to_ms_since_boot(get_absolute_time());

    if (now_ms >= next_sample_time_ms) {
      next_sample_time_ms += 10;

      restart_button.update();
      if (restart_button.on_clicked()) {
        received = false;
        receiver.init(configEntries);
      }

      adc_val = vlcfg::median3(adc_read(), adc_read(), adc_read());

      vlcfg::RxState rx_state;
      auto ret = receiver.update(adc_val, &rx_state);
      if (!received) {
        if (rx_state == vlcfg::RxState::COMPLETED) {
          on_received();
          received = true;
        } else if (rx_state == vlcfg::RxState::ERROR) {
          received = true;
        }
      }

      monitor_update(adc_val, ret, receiver);
    }
  }
}

void on_received() {
  vlcfg::ConfigEntry *e;

  e = receiver.entry_from_key(KEY_TEXT);
  printf("Text: ");
  if (e->was_received()) {
    printf("'%s'\r\n", text_buff);
  } else {
    printf("(none)\r\n");
  }

  e = receiver.entry_from_key(KEY_PASS);
  printf("Pass: ");
  if (e->was_received()) {
    printf("'%s'\r\n", pass_buff);
  } else {
    printf("(none)\r\n");
  }

  e = receiver.entry_from_key(KEY_NUMBER);
  printf("Number: ");
  if (e->was_received()) {
    printf("%d\r\n", (int)number_buff);
  } else {
    printf("(none)\r\n");
  }

  e = receiver.entry_from_key(KEY_IP_ADDR);
  printf("IP Addr: ");
  if (e->was_received()) {
    printf("%d.%d.%d.%d\r\n", (int)ip_buff[0], (int)ip_buff[1], (int)ip_buff[2],
           (int)ip_buff[3]);
  } else {
    printf("(none)\r\n");
  }

  e = receiver.entry_from_key(KEY_LED_ON);
  printf("LED On: ");
  if (e->was_received()) {
    bool led_on = (bool_buff != 0);
    printf(led_on ? "true\r\n" : "false\r\n");
    gpio_put(LED_PORT, led_on);
  } else {
    printf("(none)\r\n");
  }
}
