/*
 * Copyright (c) 2014-2017 Cesanta Software Limited
 * All rights reserved
 */

/*
 * View this file on GitHub:
 * [mgos_pwm.h](https://github.com/mongoose-os-libs/pwm/blob/master/src/mgos_pwm.h)
 */

#ifndef CS_MOS_LIBS_PWM_SRC_MGOS_PWM_H_
#define CS_MOS_LIBS_PWM_SRC_MGOS_PWM_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 *
 *  Set and control the PWM.
 *
 *  Params:
 *  pin:    GPIO number.
 *  freq:   Frequency, in Hz. 0 disables PWM on the pin.
 *  duty:   Specifies which fraction of the cycle is spent in "1" state:
 *          0 is always off, 0.5 is a square wave, 1 is always on.
 *  Return:
 *  true - SUCCESS, false - FAIL.
 *
 *  Note:
 *  On esp32 we use 8 channels and 4 timers.
 *  Each `mgos_set_pwm` call with new pin number assigns a new channel.
 *  If we already have timer running at the specified frequency,
 *  we use it instead of assigning a new one.
 */
bool mgos_pwm_set(int pin, int freq, float duty);

bool mgos_pwm_init(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CS_MOS_LIBS_PWM_SRC_MGOS_PWM_H_ */
