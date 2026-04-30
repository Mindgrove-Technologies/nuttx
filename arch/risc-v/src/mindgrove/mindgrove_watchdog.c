/****************************************************************************
 * drivers/timers/mg_wdt.c
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
 * @file mg_wdt.c
 * @brief NuttX lower-half watchdog driver for the Mindgrove WDT peripheral.
 * @details
 *   This file implements the NuttX watchdog "lower-half" interface
 *   (struct watchdog_ops_s) on top of the Mindgrove bare-metal WDT hardware.
 *
 *   The upper-half driver (drivers/timers/watchdog.c) handles all
 *   character-device bookkeeping (open/close/ioctl dispatch).  This file
 *   only needs to implement the hardware-specific callbacks.
 *
 *   Implemented ops
 *   ---------------
 *   start()       – program cycles register, set mode bits, assert ACTIVE
 *   stop()        – clear control register, zero ACTIVE
 *   keepalive()   – pulse ACTIVE to reset the down-counter ("pet the dog")
 *   getstatus()   – report ACTIVE/RESET flags and configured timeout
 *   settimeout()  – recalculate cycle count and restart the timer
 *   capture()     – not supported (returns -ENOSYS; hardware has no IRQ path
 *                   in hard-reset mode; wire an ISR if you add soft-reset IRQ)
 *
 * @version 1.0
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

 /*The driver behaves correctly when stepping through the instructions in debug
mode—the hart resets as expected when the control register is written. However, 
during normal execution, the same operation results in a dmcontrol/dmstatus
error instead of triggering the reset. */

#include <nuttx/config.h>

#include <nuttx/arch.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <debug.h>
#include "riscv_internal.h"

#include <nuttx/irq.h>
#include <nuttx/arch.h>
#include <nuttx/timers/watchdog.h>

#include "mindgrove_watchdog.h"

#ifdef CONFIG_WATCHDOG

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Convenience cast from the NuttX generic lower-half pointer to our private
 * structure.  The ops pointer is the very first field, so the cast is safe.
 */

#define TO_MG_WDT(lower) \
    ((FAR struct mg_wdt_lowerhalf_s *)(lower))


#define WD_ENABLE       (1U << 0U)
#define WD_DISABLE      (0U << 0U)
#define EN_INTR_MODE    (0U << 1U)
#define HARD_RESET_MODE (1U << 1U)
#define SOFT_RESET_MODE (1U << 2U)

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/* watchdog_ops_s callbacks */

static int mg_wdt_start(FAR struct watchdog_lowerhalf_s *lower);
static int mg_wdt_stop(FAR struct watchdog_lowerhalf_s *lower);
static int mg_wdt_keepalive(FAR struct watchdog_lowerhalf_s *lower);
static int mg_wdt_getstatus(FAR struct watchdog_lowerhalf_s *lower,
                            FAR struct watchdog_status_s    *status);
static int mg_wdt_settimeout(FAR struct watchdog_lowerhalf_s *lower,
                             uint32_t timeout_ms);
static xcpt_t mg_wdt_capture(FAR struct watchdog_lowerhalf_s *lower,
                             CODE xcpt_t handler);

/* Internal helpers */

static uint32_t mg_wdt_ms_to_cycles(uint32_t ms);

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Operation-table singleton — shared by all instances (we only have one). */

static const struct watchdog_ops_s g_mg_wdt_ops =
{
  .start      = mg_wdt_start,
  .stop       = mg_wdt_stop,
  .keepalive  = mg_wdt_keepalive,
  .getstatus  = mg_wdt_getstatus,
  .settimeout = mg_wdt_settimeout,
  .capture    = mg_wdt_capture,
  .ioctl      = NULL,           /* No vendor-specific ioctls */
};

/* Single WDT instance state */

static struct mg_wdt_lowerhalf_s g_mg_wdt_priv;

/****************************************************************************
 * Private Helper: ms → hardware cycles
 *
 * Converts a millisecond timeout to the raw 32-bit reload value written
 * into WDT_CYCLES.  The hardware counts down from this value at the
 * peripheral clock rate (CONFIG_MG_WDT_CLOCK_FREQUENCY_HZ).
 *
 * Returns 0 if the product would overflow uint32_t (caller must check).
 ****************************************************************************/

static uint32_t mg_wdt_ms_to_cycles(uint32_t ms)
{
  uint64_t cycles = (uint64_t)ms * (uint64_t)MG_WDT_TICKS_PER_MS;
  if (cycles > UINT32_MAX)
    {
      return 0;   /* Signal overflow to caller */
    }

  return (uint32_t)cycles;
}

/****************************************************************************
 * Lower-Half Operation: start
 *
 * Programs the cycle-count register with the current timeout, selects the
 * reset mode, enables the WDT, then asserts the ACTIVE latch so the
 * hardware actually starts counting.
 ****************************************************************************/

static int mg_wdt_start(FAR struct watchdog_lowerhalf_s *lower)
{
  FAR struct mg_wdt_lowerhalf_s *priv = TO_MG_WDT(lower);
  uint32_t cycles;
  uint32_t ctrl;
  irqstate_t flags;

  DEBUGASSERT(priv != NULL && priv->regs != NULL);

  /* Convert timeout (ms) → watchdog cycles */
  cycles = mg_wdt_ms_to_cycles(priv->timeout_ms);
  if (cycles == 0)
    {
      wderr("ERROR: timeout %lu ms overflows WDT cycle register\n",
            (unsigned long)priv->timeout_ms);
      return -EINVAL;
    }

  /* Enter critical section */
  flags = enter_critical_section();

  /* 1. Load watchdog counter */
  putreg32(cycles, MG_WDT_CYCLES);

  /* Small delay (if HW requires write settling) */
  for (volatile int i = 0; i < 8; i++)
    {
      __asm__ volatile ("nop");
    }

  /* 2. Configure control register */
  if (!priv->soft_reset)
    {
      ctrl = MG_WDT_CTRL_HARD_RESET | MG_WDT_CTRL_ENABLE;
    }
  else
    {
      ctrl = MG_WDT_CTRL_SOFT_RESET |
             MG_WDT_CTRL_INTR_MODE |
             MG_WDT_CTRL_ENABLE;
    }

  putreg32(ctrl, MG_WDT_CTRL);

  /* 3. Start watchdog */
  putreg32(1, MG_WDT_ACTIVE);

  priv->started = true;

  /* Leave critical section */
  leave_critical_section(flags);

  wdinfo("WDT started: timeout=%lu ms, cycles=%lu, mode=%s\n",
         (unsigned long)priv->timeout_ms,
         (unsigned long)cycles,
         priv->soft_reset ? "soft-reset" : "hard-reset");

  return OK;
}

/****************************************************************************
 * Lower-Half Operation: stop
 *
 * Disables the WDT.  Mirrors bare-metal WDT_Disable().
 ****************************************************************************/

static int mg_wdt_stop(FAR struct watchdog_lowerhalf_s *lower)
{
  FAR struct mg_wdt_lowerhalf_s *priv = TO_MG_WDT(lower);
  irqstate_t flags;

  flags = enter_critical_section();

  priv->started = false;

  leave_critical_section(flags);

  wdinfo("WDT stopped\n");
  return OK;
}

/****************************************************************************
 * Lower-Half Operation: keepalive  ("pet the dog")
 *
 * Re-asserts the ACTIVE latch so the hardware restarts its down-count from
 * WDT_CYCLES, preventing a timeout reset.
 ****************************************************************************/

static int mg_wdt_keepalive(FAR struct watchdog_lowerhalf_s *lower)
{
  FAR struct mg_wdt_lowerhalf_s *priv = TO_MG_WDT(lower);

  // DEBUGASSERT(priv != NULL && priv->regs != NULL);

printf("inside keepalive\n\r");

  if (!priv->started)
    {
      wderr("ERROR: keepalive called but WDT is not running\n");
      return -EPERM;
    }

  /* A write of 1 to WDT_ACTIVE reloads the counter.  No critical section
   * needed — this is a single 32-bit MMIO write and is inherently atomic
   * on the target architecture.
   */

  // priv->regs->WDT_ACTIVE = 1U;

  wdinfo("WDT keepalive (pet)\n");
  return OK;
}

/****************************************************************************
 * Lower-Half Operation: getstatus
 *
 * Fills in the watchdog_status_s struct for the WDIOC_GETSTATUS ioctl.
 * The hardware does not expose a live countdown register, so timeleft is
 * reported as 0 (unknown).
 ****************************************************************************/

static int mg_wdt_getstatus(FAR struct watchdog_lowerhalf_s *lower,
                            FAR struct watchdog_status_s    *status)
{
  FAR struct mg_wdt_lowerhalf_s *priv = TO_MG_WDT(lower);
printf("inside get status\n\r");
  DEBUGASSERT(priv != NULL && status != NULL);

  memset(status, 0, sizeof(*status));

  status->timeout  = priv->timeout_ms;
  status->timeleft = 0;   /* Hardware has no readable countdown register */

  if (priv->started)
    {
      status->flags |= WDFLAGS_ACTIVE;

      /* Report the active reset mode */

      if (!priv->soft_reset)
        {
          status->flags |= WDFLAGS_RESET;
        }
    }

  return OK;
}

/****************************************************************************
 * Lower-Half Operation: settimeout
 *
 * Changes the timeout and immediately restarts the timer so the new value
 * takes effect at once.  Mirrors the NuttX convention that WDIOC_SETTIMEOUT
 * also resets the running counter.
 *
 * Input Parameters:
 *   timeout_ms - New timeout in milliseconds.
 ****************************************************************************/

static int mg_wdt_settimeout(FAR struct watchdog_lowerhalf_s *lower,
                             uint32_t timeout_ms)
{
  FAR struct mg_wdt_lowerhalf_s *priv = TO_MG_WDT(lower);
  uint32_t cycles;

  DEBUGASSERT(priv != NULL);
printf("inside wdt set timeout\n\r");
  if (timeout_ms == 0)
    {
      wderr("ERROR: zero timeout is not allowed\n");
      return -EINVAL;
    }

  cycles = mg_wdt_ms_to_cycles(timeout_ms);
  if (cycles == 0)
    {
      wderr("ERROR: timeout %lu ms overflows WDT cycle register\n",
            (unsigned long)timeout_ms);
      return -EINVAL;
    }

  priv->timeout_ms = timeout_ms;

  wdinfo("WDT timeout set to %lu ms (%lu cycles)\n",
         (unsigned long)timeout_ms, (unsigned long)cycles);

  /* If the WDT is already running, restart it with the new period so the
   * counter reloads immediately.
   */

  if (priv->started)
    {
      return mg_wdt_start(lower);
    }

  return OK;
}

/****************************************************************************
 * Lower-Half Operation: capture
 *
 * The Mindgrove WDT in hard-reset mode fires a hardware reset directly —
 * there is no pre-timeout interrupt to intercept.  Soft-reset mode could
 * in principle raise an IRQ, but connecting that IRQ handler is board-
 * specific work outside this driver's scope.
 *
 * Return -ENOSYS to tell the upper half that capture is not supported.
 * The upper half will propagate this gracefully to user space.
 ****************************************************************************/

static xcpt_t mg_wdt_capture(FAR struct watchdog_lowerhalf_s *lower,
                             CODE xcpt_t handler)
{
  /* Intentionally not implemented.
   * To add IRQ support:
   *   1. Attach irq_attach(MG_WDT_IRQ, your_isr, priv)
   *   2. Store 'handler' in priv and call it from your_isr
   *   3. Return the old handler pointer
   */
printf("inside wdt_capture\n\r");
  UNUSED(lower);
  UNUSED(handler);
  wdwarn("WARNING: WDT capture (pre-timeout IRQ) not supported\n");
  return (xcpt_t)(intptr_t)(-ENOSYS);
}

/****************************************************************************
 * Public Function: mg_wdt_initialize
 *
 * Description:
 *   One-time initialisation called from the board bring-up code
 *   (e.g. board_late_initialize()).  Creates the lower-half state, maps the
 *   MMIO registers, and hands everything to watchdog_register() which
 *   creates the character device visible to user space.
 *
 * Input Parameters:
 *   devpath    - Device node path, e.g. "/dev/watchdog0"
 *   soft_reset - false → hard reset on expiry; true → soft reset
 *
 * Returned Value:
 *   OK on success; negated errno on failure.
 *
 ****************************************************************************/

int mg_wdt_initialize(FAR const char *devpath, bool soft_reset)
{
  FAR struct mg_wdt_lowerhalf_s *priv = &g_mg_wdt_priv;
  FAR void *handle;
printf("insiode init");
  DEBUGASSERT(devpath != NULL);

  /* Initialise private state */

  memset(priv, 0, sizeof(*priv));
  priv->wdg.ops      = &g_mg_wdt_ops;
  // priv->regs         = (FAR MG_WDT_Type *)CONFIG_MG_WDT_BASE;
  priv->soft_reset   = soft_reset;
  priv->timeout_ms   = MG_WDT_DEFAULT_TIMEOUT_MS;
  priv->started      = false;

  /* Make sure the hardware is in a clean disabled state on boot */


putreg32(0,MG_WDT_CYCLES);
putreg16(0,MG_WDT_CTRL);
putreg16(0,MG_WDT_RESET_CYCLES);
putreg32(0,MG_WDT_ACTIVE);
// priv->regs->WDT_CYCLES       = 0;       /* 0x0000 */
// priv->regs->WDT_RESET_CYCLES = 100;          /* 0x0010 - reset pulse width */
// priv->regs->WDT_CTRL         =0;         /* 0x0008 */
// priv->regs->WDT_ACTIVE       = 1U;  

  /* Register with the NuttX upper-half driver */

  handle = watchdog_register(devpath,
                             (FAR struct watchdog_lowerhalf_s *)priv);
  if (handle == NULL)
    {
      wderr("ERROR: watchdog_register() failed for %s\n", devpath);
      return -ENODEV;
    }

  wdinfo("Mindgrove WDT registered at %s (base=0x%08lx, mode=%s)\n",
         devpath,
         (unsigned long)CONFIG_MG_WDT_BASE,
         soft_reset ? "soft-reset" : "hard-reset");

  return OK;
}

#endif /* CONFIG_WATCHDOG */