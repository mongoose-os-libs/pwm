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
 * [mgos_pwm.c](https://github.com/mongoose-os-libs/pwm/blob/master/src/mgos_pwm.c)
 */

#include "mgos_pwm.h"

#include <stdbool.h>

bool mgos_pwm_init(void) {
  mgos_event_register_base(MGOS_PWM_BASE, "mgos_PWM");

  return true;
}
