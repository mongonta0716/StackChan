/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal.h"
#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_timer.h>
#include <led_strip.h>
#include <mooncake_log.h>
#include <string_view>

static const std::string_view _tag = "HAL-AGENT-RGB";

static constexpr gpio_num_t kAgentRgbGpio = GPIO_NUM_9;
static constexpr uint32_t kAgentRgbLedsPerSide = 9;
static constexpr uint32_t kAgentRgbLedCount = kAgentRgbLedsPerSide * 2;
static constexpr uint8_t kAgentRgbBrightness = 24;
static constexpr uint16_t kAgentRgbHueStep = 18;
static constexpr uint64_t kAgentRgbIntervalUs = 25 * 1000;

static led_strip_handle_t _agent_rgb_strip = nullptr;
static esp_timer_handle_t _agent_rgb_timer = nullptr;
static uint16_t _agent_rgb_hue = 0;
static uint32_t _agent_rgb_frame = 0;

static void _agent_rgb_timer_callback(void*)
{
    if (_agent_rgb_strip == nullptr) {
        return;
    }

    for (uint32_t i = 0; i < kAgentRgbLedsPerSide; i++) {
        uint16_t hue = (_agent_rgb_hue + i * 360 / kAgentRgbLedsPerSide) % 360;
        led_strip_set_pixel_hsv(_agent_rgb_strip, i, hue, 255, kAgentRgbBrightness);

        uint32_t mirrored_index = kAgentRgbLedCount - 1 - i;
        led_strip_set_pixel_hsv(_agent_rgb_strip, mirrored_index, hue, 255, kAgentRgbBrightness);
    }
    led_strip_refresh(_agent_rgb_strip);

    _agent_rgb_hue = (_agent_rgb_hue + kAgentRgbHueStep) % 360;
    _agent_rgb_frame++;
}

void Hal::agent_rgb_init()
{
    mclog::tagInfo(_tag, "init gpio {}, leds {}", static_cast<int>(kAgentRgbGpio), kAgentRgbLedCount);

    led_strip_config_t strip_config = {};
    strip_config.strip_gpio_num = kAgentRgbGpio;
    strip_config.max_leds = kAgentRgbLedCount;
    strip_config.led_model = LED_MODEL_WS2812;
    strip_config.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;

    led_strip_rmt_config_t rmt_config = {};
    rmt_config.resolution_hz = 10 * 1000 * 1000;

    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &_agent_rgb_strip);
    if (ret != ESP_OK) {
        mclog::tagError(_tag, "failed to create led strip: {}", esp_err_to_name(ret));
        _agent_rgb_strip = nullptr;
        return;
    }

    led_strip_clear(_agent_rgb_strip);

    esp_timer_create_args_t timer_args = {};
    timer_args.callback = _agent_rgb_timer_callback;
    timer_args.name = "agent_rgb";

    ret = esp_timer_create(&timer_args, &_agent_rgb_timer);
    if (ret != ESP_OK) {
        mclog::tagError(_tag, "failed to create timer: {}", esp_err_to_name(ret));
        led_strip_del(_agent_rgb_strip);
        _agent_rgb_strip = nullptr;
        return;
    }
}

void Hal::setAgentRgbRainbowEnabled(bool enabled)
{
    if (_agent_rgb_strip == nullptr || _agent_rgb_timer == nullptr) {
        return;
    }

    esp_timer_stop(_agent_rgb_timer);

    if (enabled) {
        _agent_rgb_hue = 0;
        _agent_rgb_frame = 0;
        _agent_rgb_timer_callback(nullptr);
        esp_timer_start_periodic(_agent_rgb_timer, kAgentRgbIntervalUs);
    } else {
        led_strip_clear(_agent_rgb_strip);
    }
}
