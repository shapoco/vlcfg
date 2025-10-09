#pragma once

#include <hardware/adc.h>
#include <hardware/gpio.h>
#include <pico/cyw43_arch.h>
#include <pico/stdlib.h>

#include <atomic>

#include <stdio.h>

#include "vlcfg/receiver.hpp"

static constexpr int OPT_SENSOR_ADC_CH = 2;
static constexpr int OPT_SENSOR_PORT = 28;

static constexpr int DISPLAY_I2C_HOST = 1;
static constexpr int DISPLAY_SDA_PORT = 18;
static constexpr int DISPLAY_SCL_PORT = 19;
static constexpr int DISPLAY_WIDTH = 128;
static constexpr int DISPLAY_HEIGHT = 64;
static constexpr int DISPLAY_ROWS = (DISPLAY_HEIGHT + 7) / 8;

static constexpr int MONITOR_BUTTON_PORT = 20;
static constexpr int RESTART_BUTTON_PORT = 21;

extern vlcfg::ConfigEntry configEntries[];
