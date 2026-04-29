/****************************************************************************
 * boards/risc-v/mindgrove/secure-iot/mindgrove_boot.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <debug.h>

#include <nuttx/board.h>
#include <arch/board/board.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: mindgrove_boardinitialize
 *
 * Description:
 *   This entry point is called early in the initialization -- after all
 *   memory has been configured and mapped but before any devices have been
 *   initialized.
 *
 ****************************************************************************/
#define PINMUX0_BASE                0x00040400UL
#define PINMUX ((PINMUX_Type *)PINMUX0_BASE)

typedef struct {                                /*!< PINMUX0 Structure                                                         */
  volatile uint32_t  MUX0;                         /*!< Select between GPIO0 and PWM0. 0 - GPIO, 1 - PWM                          */
  volatile uint32_t  MUX1;                         /*!< Select between GPIO1 and PWM1. 0 - GPIO, 1 - PWM                          */
  volatile uint32_t  MUX2;                         /*!< Select between GPIO2 and PWM2. 0 - GPIO, 1 - PWM                          */
  volatile uint32_t  MUX3;                         /*!< Select between GPIO3 and PWM3. 0 - GPIO, 1 - PWM                          */
  volatile uint32_t  MUX4;                         /*!< Select between GPIO4 and PWM4. 0 - GPIO, 1 - PWM                          */
  volatile uint32_t  MUX5;                         /*!< Select between GPIO5 and PWM5. 0 - GPIO, 1 - PWM                          */
  volatile uint32_t  MUX6;                         /*!< Select between GPIO6 and PWM6. 0 - GPIO, 1 - PWM                          */
  volatile uint32_t  MUX7;                         /*!< Select between GPIO7 and PWM7. 0 - GPIO, 1 - PWM                          */
  volatile uint32_t  MUX8;                         /*!< Select between GPIO17 and PWM8. 0 - GPIO, 1 - PWM                         */
  volatile uint32_t  MUX9;                         /*!< Select between GPIO18 and PWM9. 0 - GPIO, 1 - PWM                         */
  volatile uint32_t  MUX10;                        /*!< Select between GPIO19 and PWM10. 0 - GPIO, 1 - PWM                        */
  volatile uint32_t  MUX11;                        /*!< Select between GPIO20 and PWM11. 0 - GPIO, 1 - PWM                        */
  volatile uint32_t  MUX12;                        /*!< Select between GPIO21 and PWM12. 0 - GPIO, 1 - PWM                        */
  volatile uint32_t  MUX13;                        /*!< Select between GPIO22 and PWM13. 0 - GPIO, 1 - PWM                        */
  volatile uint32_t  MUX14;                        /*!< Select between GPIOP0 and SPI2 MOSI. 1 - GPIO, 0 - SPI2 MOSI              */
  volatile uint32_t  MUX15;                        /*!< Select between GPIOP1 and SPI2 MISO. 1 - GPIO, 0 - SPI2 MISO              */
  volatile uint32_t  MUX16;                        /*!< Select between GPIOP2 and SPI2 SCLK. 1 - GPIO, 0 - SPI2 SCLK              */
  volatile uint32_t  MUX17;                        /*!< Select between GPIOP4 and SPI2 NCS. 1 - GPIO, 0 - SPI2 NCS                */
  volatile uint32_t  MUX18;                        /*!< Select between GPIOP5 and SPI3 MOSI. 1 - GPIO, 0 - SPI3 MOSI              */
  volatile uint32_t  MUX19;                        /*!< Select between GPIOP6 and SPI3 MISO. 1 - GPIO, 0 - SPI3 MISO              */
  volatile uint32_t  MUX20;                        /*!< Select between GPIOP7 and SPI3 SCLK. 1 - GPIO, 0 - SPI3 SCLK              */
  volatile uint32_t  MUX21;                        /*!< Select between GPIOP8 and SPI3 NCS. 1 - GPIO, 0 - SPI3 NCS                */
  volatile uint32_t  MUX22;                        /*!< Select between GPIOP9 and JTAG TDO. 1 - GPIO, 0 - JTAG TDO                */
  volatile uint32_t  MUX23;                        /*!< Select between GPIOP10 and JTAG TDI. 1 - GPIO, 0 - JTAG TDI               */
  volatile uint32_t  MUX24;                        /*!< Select between GPIOP11 and JTAG TMS. 1 - GPIO, 0 - JTAG TMS               */
  volatile uint32_t  MUX25;                        /*!< Select between GPIOP12 and JTAG TCLK. 1 - GPIO, 0 - JTAG TCLK             */
  volatile uint32_t  MUX26;                        /*!< Select between GPIOP13 and JTAG TRST. 1 - GPIO, 0 - JTAG TRST             */
  volatile uint32_t  MUX27;                        /*!< Select between GPIO8 and UART3 TX. 1 - GPIO, 0 - UART3 TX                 */
  volatile uint32_t  MUX28;                        /*!< Select between GPIO9 and UART3 RX. 1 - GPIO, 0 - UART3 RX                 */
  volatile uint32_t  MUX29;                        /*!< Select between GPIO11 and UART4 TX. 1 - GPIO, 0 - UART4 TX                */
  volatile uint32_t  MUX30;                        /*!< Select between GPIO15 and UART4 TX. 1 - GPIO, 0 - UART4 RX                */
} PINMUX_Type;                                 /*!< Size = 124 (0x7c)                                                         */

uint16_t PINMUX_Reset(void) {
    PINMUX->MUX14 = 1;  // GPIO32
    PINMUX->MUX15 = 1;  // GPIO33
    PINMUX->MUX16 = 1;  // GPIO34
    PINMUX->MUX17 = 1;  // GPIO35
    PINMUX->MUX18 = 1;  // GPIO36
    PINMUX->MUX19 = 1;  // GPIO37
    PINMUX->MUX24 = 1;  // GPIO38
    PINMUX->MUX25 = 1;  // GPIO39
    PINMUX->MUX26 = 1;  // GPIO40
    PINMUX->MUX27 = 1;  // GPIO41
    return 0;
}


void mindgrove_boardinitialize(void)
{
    PINMUX_Reset();
}
