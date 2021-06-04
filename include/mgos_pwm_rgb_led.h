/*
 * Copyright (c) 2014-2018 Cesanta Software Limited
 * All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the ""License"");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an ""AS IS"" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * PWM-controlled RGB LED.
 * Example:
 *   struct mgos_pwm_rgb_led led;
 *   mgos_pwm_rgb_led_init(&led, 16, 17, 18, LED_FREQ, COMMON_CATHODE);
 *   mgos_pwm_rgb_led_set(&led, 255, 255, 255, 255);  // White, max brightness
 *   mgos_pwm_rgb_led_set(&led, 255,   0,   0, 127);  // Red, half brightness
 */

#ifndef MGOS_PWM_RGB_LED_h
#define MGOS_PWM_RGB_LED_h
#pragma once

#include "driver/ledc.h"
#include "mgos_freertos.h"
#include "mgos_timers.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

enum mgos_pwm_fade_direction
{
    FADE_BLINK = 0,
    FADE_BLINKRAPID,
    FADE_OFF,
    FADE_UP,
    FADE_DOWN,
    FADE_LOOP,
    

};
enum mgos_pwm_fade_colors
{
    RED = 0,
    GREEN,
    BLUE,
    ALL

};

struct mgos_pwm_rgb_led {
  int freq, chan;
  uint8_t r, g, b, br; /* Current values or R, G, B and brightness. */
  int gpio_r, gpio_g, gpio_b;
  int gpio_r_chan, gpio_g_chan, gpio_b_chan; /* what ledc channel these become associated with */
  float gpio_r_pct, gpio_g_pct, gpio_b_pct; /* _apply function calculated 0-1 brightness result */
  bool common_cathode;
  bool fade_installed;
  enum mgos_pwm_fade_direction fade_direction;
  int time_on, time_off; // off is only for blink
  int fade_max, fade_min; /* time in ms, fade max/min in 0-255 */
  TaskHandle_t xHandle;
  mgos_timer_id led_timer_id;
}; //  mgos_pwm_rgb_led_t, *p_mgos_pwm_rgb_led_t;

//struct mgos_pwm_rgb_led led;

#define MGOS_PWM_RGB_LED_DEFAULT_FREQ 400 /* Hz */

/* 
 * Init the LED pins. 
 * Common Cathode the diodes arrow symbol point to a shared GND. 
 * Common Anode the diodes arrow symbol point to a shared VIN. 
*/
bool mgos_pwm_rgb_led_init(struct mgos_pwm_rgb_led *led, int gpio_r, int gpio_g,
                           int gpio_b, int freq, bool common_cathode);

/* Set color and brightenss, 1-255. */
void mgos_pwm_rgb_led_set(struct mgos_pwm_rgb_led *led, uint8_t r, uint8_t g,
                          uint8_t b, uint8_t br);

/* Set color. */
void mgos_pwm_rgb_led_set_color(struct mgos_pwm_rgb_led *led, uint8_t r,
                                uint8_t g, uint8_t b);

/* Set color as a 24-bit RGB value. */
void mgos_pwm_rgb_led_set_color_rgb(struct mgos_pwm_rgb_led *led, uint32_t rgb);

/* Set brightness. */
void mgos_pwm_rgb_led_set_brightness(struct mgos_pwm_rgb_led *led, uint8_t br);

/* Set PWM frequency. */
bool mgos_pwm_rgb_led_set_freq(struct mgos_pwm_rgb_led *led, int freq);

/* 
* Register a system event handler to maintain a mapping between pin and ledc channel
* Whereever you initialised the led object, add this:
* mgos_event_add_handler(MGOS_PWM_CHANNEL, mgos_pwm_rgb_channel_update_cb, &led); 
*/
void mgos_pwm_rgb_channel_update_cb(int ev, void* ev_data, void* userdata);

/* Enable LEDC hardware fading 
* resetToStartingPosition flags whether the led should be set to the logical brightness that the direction would start at
* ie 0 if fading up
*/
void mgos_pwm_rgb_fade_start(struct mgos_pwm_rgb_led* led, int ms, enum mgos_pwm_fade_direction direction,
  bool resetToStartingPosition, uint8_t max, uint8_t min);

/* Enable LEDC hardware fading */
void mgos_pwm_rgb_fade_stop(struct mgos_pwm_rgb_led* led);

/* Private TaskCreate process to avoid interruptions */
void mgos_pwm_rgb_blink_task(void* ledArg);

/* client callable function to start blink */
void mgos_pwm_rgb_blink_start(struct mgos_pwm_rgb_led* led, int ms_on, int ms_off);

void mgos_pwm_rgb_blink_stop(struct mgos_pwm_rgb_led* led);

/* Disable PWM and release the pins. */
void mgos_pwm_rgb_led_deinit(struct mgos_pwm_rgb_led *led);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif