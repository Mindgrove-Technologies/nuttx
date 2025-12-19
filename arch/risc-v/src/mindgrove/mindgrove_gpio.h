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

#ifndef SHAKTI_GPIO_H
#define SHAKTI_GPIO_H

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
#define GPIO_BUFFER_CTRL_OFFSET    0x40
#define GPIO_BUFFER_STATUS_OFFSET  0x48

#define GPIO_BUFFER_2_CLK_OFFSET   0x50
#define GPIO_BUFFER_4_CLK_OFFSET   0x58
#define GPIO_BUFFER_8_CLK_OFFSET   0x60
#define GPIO_BUFFER_12_CLK_OFFSET  0x68

#define GPIO_BUFFER_2_DATA_OFFSET  0x70
#define GPIO_BUFFER_4_DATA_OFFSET  0x78
#define GPIO_BUFFER_8_DATA_OFFSET  0x80
#define GPIO_BUFFER_12_DATA_OFFSET 0x88




int mg_go_read(FAR struct gpio_dev_s *dev, FAR bool *value);
int mg_go_write(FAR struct gpio_dev_s *dev, bool value);
int mg_go_attach(FAR struct gpio_dev_s *dev, pin_interrupt_t cb);
int mg_go_enable(FAR struct gpio_dev_s *dev, bool enable);
int mg_go_setpintype(FAR struct gpio_dev_s *dev, enum gpio_pintype_e pintype);
int mindgrove_gpio_init(void);



#endif
