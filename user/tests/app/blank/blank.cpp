/*
 ******************************************************************************
 *  Copyright (c) 2015 Particle Industries, Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************
 */

 #include "application.h"

Serial1LogHandler dbg(115200, LOG_LEVEL_WARN);
SYSTEM_MODE(MANUAL);
STARTUP(System.buttonMirror(D1, FALLING, true));

void handler(system_event_t ev, int data) {
    LOG(WARN, "Click %d", data);
}

/* executes once at startup */
void setup() {
    System.on(button_click, handler);
}

/* executes continuously after setup() runs */
void loop() {
    auto pinmap = HAL_Pin_Map();
    pinMode(D1, INPUT_PULLDOWN);
    EXTI_GenerateSWInterrupt(pinmap[D1].gpio_pin);
    delay(300);
    pinMode(D1, INPUT_PULLUP);
    delay(1000);
}
