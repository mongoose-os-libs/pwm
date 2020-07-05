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
 * View this file on GitHub:
 * [mgos_pwm_rgb_led.c](https://github.com/mongoose-os-libs/pwm/blob/master/src/mgos_pwm_rgb_led.c)
 */

#include "mgos_pwm_rgb_led.h"

#include <string.h>

#include "mgos.h"
#include "mgos_pwm.h"
#include "soc/timer_group_reg.h"
#include "soc/timer_group_struct.h"

TaskHandle_t mgos_pwm_rgb_led_xHandle;


typedef struct params_s
{
    uint16_t time_on, time_off; // ms
    uint16_t freq;
    uint8_t led1_gpio; // 0 if unused
    uint8_t led2_gpio; // 0 if unused
    uint8_t led3_gpio; // 0 if unused
    bool common_cathode; // COMMON CATHODE (ie they have a common ground ). They turn off with PWM at 0
    float led1_pct, led2_pct, led3_pct; /* 0-1 values */
} params_t, *p_params_t;

static void vLEDPWMTask(void* pvParameters) {
    p_params_t pParams = (p_params_t)pvParameters;

    /*
    TIMERG0.wdt_wprotect = TIMG_WDT_WKEY_VALUE;
    TIMERG0.wdt_feed = 1;
    TIMERG0.wdt_wprotect = 0;

    // feed dog 1
    TIMERG1.wdt_wprotect = TIMG_WDT_WKEY_VALUE;  // write enable
    TIMERG1.wdt_feed = 1;                        // feed dog
    TIMERG1.wdt_wprotect = 0;                    // write protect
    */

    bool on = true;
    float offDuty = 0;
    if (!pParams->common_cathode) {
        // COMMON CATHODE (ie they have a common ground ). They turn off with PWM at 0
        // COMMON ANODE means the LEDs share a common voltage source / VIN. They turn off with PWM at 1
        offDuty = 1;
    }

    // Set sensible limits for frequency
    if (pParams->freq < 100) {
        printf("vLEDPWMTask frequency too low, setting to 100 \n");
        pParams->freq = 100;
    } else if (pParams->freq > 25000) {
        printf("vLEDPWMTask frequency too high, setting to 25000 \n");
        pParams->freq = 25000;
    }

    // Initialise the xLastWakeTime variable with the current time.
    TickType_t xLastWakeTime;
    // Note xFrequency is in ticks, not ms. So we convert from ms by dividing by portTICK_PERIOD_MS
    //const TickType_t xFrequency = pParams->time / portTICK_PERIOD_MS;

    const TickType_t xFrequency = 1000 / portTICK_PERIOD_MS;
    xLastWakeTime = xTaskGetTickCount();

    // turn them all off to avoid weird colors
    if (pParams->led1_gpio){
        mgos_pwm_set(pParams->led1_gpio, 0, offDuty); // 0 frequency clears PWM
        // mgos_pwm_set does mgos_gpio_write(pin, 0);
        // This is which is super annoying if you have a common anode LED that needs 1 to turn off
        mgos_gpio_write(pParams->led1_gpio, offDuty ? 1 : 0);
    }
    if (pParams->led2_gpio){
        mgos_pwm_set(pParams->led2_gpio, 0, offDuty);
        mgos_gpio_write(pParams->led3_gpio, offDuty ? 1 : 0);
    }
    if (pParams->led3_gpio){
        mgos_pwm_set(pParams->led3_gpio, 0, offDuty);
        mgos_gpio_write(pParams->led3_gpio, offDuty ? 1 : 0);
    }
    
    LOG(LL_DEBUG, ("LEDPWMTASK int r %g, g %g, b %g freq %d", pParams->led1_pct, pParams->led2_pct, pParams->led3_pct, pParams->freq));

    for (;;) {
        on = !on;

        if (pParams->led1_gpio)
            mgos_pwm_set(pParams->led1_gpio, pParams->freq, on ? pParams->led1_pct : offDuty);
        if (pParams->led2_gpio)
            mgos_pwm_set(pParams->led2_gpio, pParams->freq, on ? pParams->led2_pct : offDuty);
        if (pParams->led3_gpio)
            mgos_pwm_set(pParams->led3_gpio, pParams->freq, on ? pParams->led3_pct : offDuty);
        //vTaskDelayUntil(&xLastWakeTime, xFrequency);

        //vTaskDelay(pdMS_TO_TICKS(10));
        vTaskDelay(pParams->time / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}


static void mgos_pwm_rgb_fade_set(int chan, float pct, int ms, bool common_cathode) {   

    LOG(LL_DEBUG, ("mgos_pwm_rgb_fade_set chan %d br %g  ms %d", chan, pct, ms));

    float duty = pct * (float)((1 << LEDC_TIMER_10_BIT) - 1);

    LOG(LL_DEBUG, ("mgos_pwm_rgb_fade_set duty2 %g ", duty));

    ledc_set_fade_with_time(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)chan, (int)duty, ms);

    ledc_fade_start(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)chan, LEDC_FADE_NO_WAIT);

    LOG(LL_DEBUG, ("mgos_pwm_rgb_fade_set after set fade "));
}

static bool mgos_pwm_rgb_led_apply(struct mgos_pwm_rgb_led *led) {
    // We need to convert to 0-1 as fraction
    float brv = led->br / 255.0f;

    float rv = led->r / 255.0f;
    float gv = led->g / 255.0f;
    float bv = led->b / 255.0f;

    LOG(LL_DEBUG, ("Input LEDR %d G %d B %d BR %d", led->r, led->g, led->b, led->br));

    // Apply brightness, if its zero/off it won't change
    rv *= brv;
    gv *= brv;
    bv *= brv;

    if (led->common_cathode){
        // COMMON CATHODE (ie they have a common ground )
        LOG(LL_DEBUG, ("CATHODE apply r %g, g %g, b %g | brightness %g", rv, gv, bv, brv));
    } else {
        // COMMON ANODE means the LEDs share a common voltage source / VIN
        // User supplies (0 = off, 1 = fully on)
        // If common anode, we need to flip the values as a 0 output = fully on / 1 = off
        rv = 1.0f - rv;
        gv = 1.0f - gv;
        bv = 1.0f - bv;

        LOG(LL_DEBUG, ("ANODE apply r %g, g %g, b %g ", rv, gv, bv));
    }

    // Save it so we can call apply then use the resulting values elsewhere, ie pwm blinking
    led->gpio_r_pct = rv;
    led->gpio_g_pct = gv;
    led->gpio_b_pct = bv;

    LOG(LL_DEBUG, ("Apply saving pct r %g, g %g, b %g ", led->gpio_r_pct, led->gpio_g_pct, led->gpio_b_pct));

    if (led->fade_direction > FADE_OFF){
        LOG(LL_DEBUG, ("Calling fade set"));
      if (led->r && led->gpio_r_chan > -1)
          mgos_pwm_rgb_fade_set(led->gpio_r_chan, rv, led->time_on, led->common_cathode);
      if (led->g && led->gpio_g_chan > -1)
          mgos_pwm_rgb_fade_set(led->gpio_g_chan, gv, led->time_on, led->common_cathode);
      if (led->b && led->gpio_b_chan > -1)
          mgos_pwm_rgb_fade_set(led->gpio_b_chan, bv, led->time_on, led->common_cathode);

      return true;
    } else {
      return (mgos_pwm_set(led->gpio_r, led->freq, rv) && mgos_pwm_set(led->gpio_g, led->freq, gv) && mgos_pwm_set(led->gpio_b, led->freq, bv));
    }

}

void mgos_pwm_rgb_channel_update_cb(int ev, void* ev_data, void* userdata) {
    // Note, after calling the first mgos_pwm_set you should wait ~300ms for the channels to update and fire the event

    struct mgos_pwm_rgb_led* ledLocal = (struct mgos_pwm_rgb_led*)userdata;
    struct mgos_pwm_channel_data* data = (struct mgos_pwm_channel_data*)ev_data;

    if ((int)data->channel >= 0) {

        LOG(LL_DEBUG, ("pwm_channel_update_cb: pin %d is on chan %d", data->pin, data->channel));

        if (data->pin == ledLocal->gpio_r) {
            ledLocal->gpio_r_chan = data->channel;
        } else if (data->pin == ledLocal->gpio_g) {
            ledLocal->gpio_g_chan = data->channel;
        } else if (data->pin == ledLocal->gpio_b) {
            ledLocal->gpio_b_chan = data->channel;
        }
    }

    (void)ev;
}

static void ledc_fade_cb(void* ledArg) {
    static bool fade_inverted = true; // direction of last fade

    struct mgos_pwm_rgb_led* led = (struct mgos_pwm_rgb_led*)ledArg;

    fade_inverted = !fade_inverted;

    led->br = fade_inverted ? led->fade_max : led->fade_min;

    if (fade_inverted) {
        LOG(LL_DEBUG, ("LEDC fade timer inverted  target %d", led->br));
    } else {
        LOG(LL_DEBUG, ("LEDC fade timer normal way target %d", led->br));
    }
    mgos_pwm_rgb_led_apply(led);

}

void mgos_pwm_rgb_blink_task(void* ledArg) {
    // Dereference it otherwise the memory footprint grows with events etc etc
    struct mgos_pwm_rgb_led* led = (struct mgos_pwm_rgb_led*)ledArg;

    LOG(LL_DEBUG, ("LEDC Blinking LED via pwm rgb blink task, time %d ", led->time_on));

    // Very simple, we just need to call our function then delay however long we require
    // If you just use the 'best effort' timers the blinking is interrupted by other timers running in a single thread
    while (1) {
        ledc_fade_cb(&led);
        vTaskDelay(led->time_on / portTICK_PERIOD_MS);
    }
}

void mgos_pwm_rgb_blink_stop(struct mgos_pwm_rgb_led* led){
    if (led->xHandle != NULL){
        LOG(LL_DEBUG, ("LEDC stopping blink task"));
        vTaskDelete( led->xHandle );
        led->xHandle = NULL;
    }
}

void mgos_pwm_rgb_blink_start(struct mgos_pwm_rgb_led* led, int ms_on, int ms_off){
    LOG(LL_INFO, ("LEDC blink_start ms: %d", ms));
    mgos_pwm_rgb_fade_stop(led); // stop any fading

    led->fade_max = 255;
    led->fade_min = 0;
    led->fade_direction = FADE_BLINK;
    // Some sensible limits
    if (ms_on < 50){
        ms_on = 50;
    } else if (ms_on > 100000) {
        ms_on = 100000;
    }
    if (ms_off < 50) {
        ms_off = 50;
    } else if (ms_off > 100000) {
        ms_off = 100000;
    }
    led->time_on = ms_on;
    led->time_off = ms_off;

    LOG(LL_INFO, ("LEDC blink_start time on: %d off: %d", led->time_on, led->time_off));

    // Stop it as it may have different parameters
    mgos_pwm_rgb_blink_stop(led);

    p_params_t pParams1 = (p_params_t)malloc(sizeof(params_t));
    pParams1->led1_gpio = led->gpio_r;
    pParams1->led2_gpio = led->gpio_g;
    pParams1->led3_gpio = led->gpio_b;
    pParams1->led1_pct = led->gpio_r_pct;
    pParams1->led2_pct = led->gpio_g_pct;
    pParams1->led3_pct = led->gpio_b_pct;
    pParams1->time_on = led->time_on;
    pParams1->time_off = led->time_off;
    pParams1->freq = led->freq;
    pParams1->common_cathode = led->common_cathode;
    LOG(LL_DEBUG,
        ("LEDPWMTASK params r %g, g %g, b %g time %d / %d", pParams1->led1_pct,
         pParams1->led2_pct, pParams1->led3_pct, led->time_on, led->time_off));

    BaseType_t xReturned;
    xReturned = xTaskCreate(
        vLEDPWMTask, /* Function that implements the task. */
        "LEDPWMTASK", /* Text name for the task. */
        4096, /* Stack size in words, not bytes. */
        pParams1, /* Parameter passed into the task. */
        tskIDLE_PRIORITY + 1, /* Priority at which the task is created. */
        &led->xHandle); /* Used to pass out the created task's handle. */
}


void mgos_pwm_rgb_fade_start(struct mgos_pwm_rgb_led* led, int ms, enum mgos_pwm_fade_direction direction,
    bool resetToStartingPosition, uint8_t max, uint8_t min) {

    mgos_pwm_rgb_blink_stop(led); // stop any blinking task

    led->time_on = ms;
    led->fade_direction = direction;
    led->fade_max = max;
    led->fade_min = min;

    if (!led->fade_installed){
        LOG(LL_DEBUG, ("LEDC fade about to be installed"));

        //ret = ledc_fade_func_install(ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_SHARED);
        ESP_ERROR_CHECK(ledc_fade_func_install(0));

        led->fade_installed = true;
    }

    LOG(LL_DEBUG, ("LEDC fade installed. time %d, direction %d", led->time_on, led->fade_direction));

    if (direction == FADE_UP) {
        if (resetToStartingPosition) {
            LOG(LL_DEBUG, ("LEDC UP trying to set brightness with led & min %u ", min));
            mgos_pwm_rgb_led_set_brightness(led, min);
        }
        
        led->br = led->fade_max;
        mgos_pwm_rgb_led_apply(led);

    }
    else if (direction == FADE_DOWN) {
        if (resetToStartingPosition) {
            LOG(LL_DEBUG, ("LEDC DOWN trying to set brightness with led & max %u ", max));
            mgos_pwm_rgb_led_set_brightness(led, max);
        }
        
        led->br = led->fade_min;
        mgos_pwm_rgb_led_apply(led);
    }

    else { // fade and blink
        mgos_clear_timer(led->led_timer_id); // otherwise with multiple fades we end up with multiple timers!
        led->led_timer_id = mgos_set_timer(led->time_on + 1 /* so we know it finished */, MGOS_TIMER_REPEAT, ledc_fade_cb, led);
    }
}

void mgos_pwm_rgb_fade_stop(struct mgos_pwm_rgb_led* led) {
    if (led->fade_installed){
        ledc_fade_func_uninstall(0);
        led->fade_installed = false;
        LOG(LL_DEBUG, ("LEDC fade uninstalled"));
    } else {
        LOG(LL_DEBUG, ("LEDC fade uninstall skipped"));
    }

}

bool mgos_pwm_rgb_led_init(struct mgos_pwm_rgb_led *led, int gpio_r, int gpio_g,
                           int gpio_b, int freq, bool common_cathode) {
  memset(led, 0, sizeof(*led));

  led->gpio_r = gpio_r;
  led->gpio_g = gpio_g;
  led->gpio_b = gpio_b;
  led->gpio_r_chan = -1;
  led->gpio_g_chan = -1;
  led->gpio_b_chan = -1;
  led->common_cathode = common_cathode;
  led->freq = (freq > 0) ? freq : MGOS_PWM_RGB_LED_DEFAULT_FREQ;
  led->br = common_cathode ? 0 : 255; // Default to off
  led->led_timer_id = MGOS_INVALID_TIMER_ID;

  led->xHandle = NULL;

  mgos_pwm_rgb_led_xHandle = NULL;

  return mgos_pwm_rgb_led_apply(led);
}


void mgos_pwm_rgb_led_set(struct mgos_pwm_rgb_led *led, uint8_t r, uint8_t g,
                          uint8_t b, uint8_t br) {
  led->r = r;
  led->g = g;
  led->b = b;
  led->br = br;
  mgos_pwm_rgb_led_apply(led);
}

void mgos_pwm_rgb_led_set_color(struct mgos_pwm_rgb_led *led, uint8_t r,
                                uint8_t g, uint8_t b) {
  mgos_pwm_rgb_led_set(led, r, g, b, led->br);
}

void mgos_pwm_rgb_led_set_color_rgb(struct mgos_pwm_rgb_led *led,
                                    uint32_t rgb) {
  mgos_pwm_rgb_led_set(led, (rgb >> 16) & 0xff, (rgb >> 8) & 0xff, (rgb & 0xff),
                       led->br);
}

void mgos_pwm_rgb_led_set_brightness(struct mgos_pwm_rgb_led *led, uint8_t br) {
  mgos_pwm_rgb_led_set(led, led->r, led->g, led->b, br);
}

bool mgos_pwm_rgb_led_set_freq(struct mgos_pwm_rgb_led *led, int freq) {
  led->freq = freq;
  return mgos_pwm_rgb_led_apply(led);
}

void mgos_pwm_rgb_led_deinit(struct mgos_pwm_rgb_led *led) {

  if (led->xHandle != NULL){
      vTaskDelete( led->xHandle );
      led->xHandle = NULL;
  }

  mgos_pwm_set(led->gpio_r, 0, 0);
  mgos_pwm_set(led->gpio_g, 0, 0);
  mgos_pwm_set(led->gpio_b, 0, 0);
}
