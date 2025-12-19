/* mindgrove_irq.h */
#ifndef __MINDGROVE_IRQ_H
#define __MINDGROVE_IRQ_H

#include <nuttx/irq.h>
#include <nuttx/config.h>

#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <debug.h>

#include <nuttx/arch.h>
#include <nuttx/board.h>
#include <arch/irq.h>
#include <arch/board/board.h>

#include "riscv_internal.h"
// #include "riscv_arch.h"

#include "mindgrove.h"
#include "plic.h"
void up_enable_irq(int irq);
void up_disable_irq(int irq);
irqstate_t up_irq_enable(void);

#endif /* __MINDGROVE_IRQ_H */
