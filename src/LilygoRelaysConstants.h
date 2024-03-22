/**
 *
 * @license MIT License
 *
 * Copyright (c) 2024 Matthew Sargent
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * @file      LilygoRelaysConstants.h
 * @author    Matthew Sargent (matthew.c.sargent@gmail.com)
 * @date      2024-03-12
 *
 */

#pragma once

// 4 RELAY BOARD
#define LILYGORELAY4_LED_PIN 25
#define LILYGORELAY4_RELAY1_PIN 21
#define LILYGORELAY4_RELAY2_PIN 19
#define LILYGORELAY4_RELAY3_PIN 18
#define LILYGORELAY4_RELAY4_PIN 5

// 8 RELAY BOARD
#define LILYGORELAY4_LED_PIN 25
#define LILYGORELAY8_RELAY1_PIN 33
#define LILYGORELAY8_RELAY2_PIN 32
#define LILYGORELAY8_RELAY3_PIN 13
#define LILYGORELAY8_RELAY4_PIN 12
#define LILYGORELAY8_RELAY5_PIN 21
#define LILYGORELAY8_RELAY6_PIN 19
#define LILYGORELAY8_RELAY7_PIN 18
#define LILYGORELAY8_RELAY8_PIN 5


//6 RELAY BOARD
#define LILYGORELAY6_BANKS_MIN 1
#define LILYGORELAY6_BANKS_MAX 3

//RTC	SDA	SCL	IRQ
//GPIO	16	17	15
#define LILYGORELAY6_RTC_SDA_PIN 16
#define LILYGORELAY6_RTC_SCL_PIN 17
#define LILYGORELAY6_RTC_IRQ_PIN 15

//Shift register	DATA	CLOCK	LATCH
//GPIO			    7		5		6
#define LILYGORELAY6_SHIFT_DATA_PIN 7
#define LILYGORELAY6_SHIFT_CLOCK_PIN 5
#define LILYGORELAY6_SHIFT_LATCH_PIN 6

#define LILYGORELAY6_GLED_POS   6
#define LILYGORELAY6_RLED_POS   7
#define LILYGORELAY6_RELAY1_POS 0
#define LILYGORELAY6_RELAY2_POS 1
#define LILYGORELAY6_RELAY3_POS 2
#define LILYGORELAY6_RELAY4_POS 3
#define LILYGORELAY6_RELAY5_POS 4
#define LILYGORELAY6_RELAY6_POS 5

