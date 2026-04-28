/************************************************************************
* Project           		:  shakti devt board
* Name of the file	     	:  gpio.h
* Brief Description of file     :  header file for gpio_applns
* Name of Author    	        :  Sathya Narayanan N
* Email ID                      :  sathya281@gmail.com

Copyright (C) 2019  IIT Madras. All rights reserved.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
***************************************************************************/
/**
 * @file  gpio.h
 * @brief  header file for gpio_applns
 */


#include <stdint.h>
#include <nuttx/ioexpander/gpio.h>

#ifndef __ARCH_RISCV_SRC_CHIP_MINDGROVE_GPIO_H
#define __ARCH_RISCV_SRC_CHIP_MINDGROVE_GPIO_H

#define GPIO_HIGH 1
#define GPIO_LOW 0

#define GPIO_OUTPUT_MODE 1
#define GPIO_INPUT_MODE 0

#define GPIO_DIRECTION_CNTRL_REG (uint32_t*) (GPIO_START  + (0 * GPIO_OFFSET ))
#define GPIO_DATA_REG (uint32_t*) (GPIO_START + (1 * GPIO_OFFSET ))
#define GPIO_INTERRUPT_CONFIG_REG (uint32_t*) (GPIO_START + (6 * GPIO_OFFSET ))
#define GPIO_DIRECTION_OFFSET      0x00
#define GPIO_DATA_OFFSET           0x08
#define GPIO_SET_OFFSET            0x10
#define GPIO_CLEAR_OFFSET          0x18
#define GPIO_TOGGLE_OFFSET         0x20
#define GPIO_INTR_OFFSET           0x30
#define GPIO_PULLUP_OFFSET         0x38

#endif
