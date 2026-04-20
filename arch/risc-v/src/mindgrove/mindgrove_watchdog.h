/****************************************************************************
 * drivers/timers/mg_wdt.h
 *
 * SPDX-License-Identifier: Apache-2.0
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
 * Project                   : Secure IoT SoC (NuttX Port)
 * @copyright Copyright (c) 2026 Mindgrove Technologies. All rights reserved.
 * @file mg_wdt.h
 * @brief NuttX lower-half watchdog driver header for Mindgrove WDT.
 * @version 1.0
 *
 ****************************************************************************/

#ifndef __DRIVERS_TIMERS_MG_WDT_H
#define __DRIVERS_TIMERS_MG_WDT_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/timers/watchdog.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef CONFIG_WATCHDOG

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* WDT Hardware Register Layout *********************************************/

/* Base address — defined in your board/SoC memory map.
 * Override via Kconfig: CONFIG_MG_WDT_BASE
 */

#ifndef CONFIG_MG_WDT_BASE
#define CONFIG_MG_WDT_BASE 0x40500
#endif

/* Clock frequency used to convert milliseconds → hardware cycles.
 * Override via Kconfig: CONFIG_MG_WDT_CLOCK_FREQUENCY_HZ
 * Default: 40 000 000 Hz (40 MHz) — adjust to your SoC clock.
 */

#ifndef CONFIG_MG_WDT_CLOCK_FREQUENCY_HZ
#  define CONFIG_MG_WDT_CLOCK_FREQUENCY_HZ  30000000UL
#endif

/* Derived: clock ticks per millisecond */

#define MG_WDT_TICKS_PER_MS \
    (CONFIG_MG_WDT_CLOCK_FREQUENCY_HZ / 1000UL)

/* Default timeout in milliseconds (5 000 ms = 5 s) */

#define MG_WDT_DEFAULT_TIMEOUT_MS   5000U

/* WDT Register Offsets *****************************************************/

#define MG_WDT_CTRL     CONFIG_MG_WDT_BASE+ 0x08U   /* Control register          */
#define MG_WDT_CYCLES    CONFIG_MG_WDT_BASE+0x00U   /* Reload-cycle register     */
#define MG_WDT_ACTIVE    CONFIG_MG_WDT_BASE+0x18U   /* Active / keepalive latch  */
#define MG_WDT_RESET_CYCLES     CONFIG_MG_WDT_BASE+0x10U



/* WDT_CTRL bit fields */

#define MG_WDT_CTRL_ENABLE      (1U << 0U)  /* 1 = WDT enabled          */
#define MG_WDT_CTRL_INTR_MODE   (0U << 1U)  /* Interrupt mode (no reset)*/
#define MG_WDT_CTRL_HARD_RESET  (1U << 1U)  /* Hard-reset on expiry     */
#define MG_WDT_CTRL_SOFT_RESET  (1U << 2U)  /* Soft-reset on expiry     */

/* Reset-mode selectors (mirrors bare-metal macros) */

#define MG_WDT_MODE_HARD_RESET  false
#define MG_WDT_MODE_SOFT_RESET  true

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* Memory-mapped register block — must match the hardware layout exactly.   */



// typedef volatile struct
// {
//   uint32_t WDT_CYCLES;        /* 0x0000 */
//   uint8_t  reserved0[4];      /* 0x0004 – 4 bytes to reach 0x0008  */
//   uint16_t WDT_CTRL;          /* 0x0008 */
//   uint8_t  reserved1[6];      /* 0x000A – 6 bytes to reach 0x0010  */
//   uint16_t WDT_RESET_CYCLES;  /* 0x0010 */
//   uint8_t  reserved2[6];      /* 0x0012 – 6 bytes to reach 0x0018  */
//   uint32_t WDT_ACTIVE;        /* 0x0018 */
// } __attribute__((packed)) MG_WDT_Type;
/* Lower-half private state *************************************************/

struct mg_wdt_lowerhalf_s
{
  /* Must be first — the upper half casts FAR struct watchdog_lowerhalf_s *
   * to FAR struct mg_wdt_lowerhalf_s * via the ops pointer.               */

  struct watchdog_lowerhalf_s  wdg;         /* NuttX lower-half base      */

  /* Hardware handle */

  // FAR MG_WDT_Type             *regs;        /* MMIO base pointer          */

  /* Driver state */

  bool                         started;     /* true after start()         */
  bool                         soft_reset;  /* Reset mode in use          */
  uint32_t                     timeout_ms;  /* Current timeout (ms)       */
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Name: mg_wdt_initialize
 *
 * Description:
 *   Create a NuttX watchdog device for the Mindgrove WDT peripheral and
 *   register it under the given devpath (e.g. "/dev/watchdog0").
 *
 * Input Parameters:
 *   devpath    - Full pseudo-filesystem path for the device node.
 *   soft_reset - false → hard reset on timeout; true → soft reset.
 *
 * Returned Value:
 *   OK (0) on success; a negated errno on failure.
 *
 ****************************************************************************/

int mg_wdt_initialize(FAR const char *devpath, bool soft_reset);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_WATCHDOG */
#endif /* __DRIVERS_TIMERS_MG_WDT_H */