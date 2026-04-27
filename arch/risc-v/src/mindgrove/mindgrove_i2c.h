/* Register Offsets */
#define MG_I2C_S2_OFFSET        0x00   /* S2 register offset (own address register) */
#define MG_I2C_CTRL_OFFSET      0x08   /* Control register offset */
#define MG_I2C_S0_OFFSET        0x10   /* S0 register offset (data shift register) */
#define MG_I2C_STATUS_OFFSET    0x18   /* Status register offset */
#define MG_I2C_SCL_OFFSET       0x38   /* SCL (clock) register offset */

/* Control Register Bit Positions */
#define MG_I2C_CTRL_ACK_BIT     (0U)   /* Acknowledge bit position */
#define MG_I2C_CTRL_STO_BIT     (1U)   /* Stop condition bit position */
#define MG_I2C_CTRL_STA_BIT     (2U)   /* Start condition bit position */
#define MG_I2C_CTRL_ENI_BIT     (3U)   /* Enable interrupt bit position */
#define MG_I2C_CTRL_ESO_BIT     (6U)   /* Enable serial output bit position */
#define MG_I2C_CTRL_PIN_BIT     (7U)   /* Pending interrupt (PIN) bit position */

/* Control Register Bit Masks */
#define MG_I2C_CTRL_ACK         (1U << MG_I2C_CTRL_ACK_BIT)   /* ACK: Send acknowledge after byte received */
#define MG_I2C_CTRL_STO         (1U << MG_I2C_CTRL_STO_BIT)   /* STO: Generate stop condition on bus */
#define MG_I2C_CTRL_STA         (1U << MG_I2C_CTRL_STA_BIT)   /* STA: Generate start condition on bus */
#define MG_I2C_CTRL_ENI         (1U << MG_I2C_CTRL_ENI_BIT)   /* ENI: Enable interrupt on transfer complete */
#define MG_I2C_CTRL_ESO         (1U << MG_I2C_CTRL_ESO_BIT)   /* ESO: Enable serial data output */
#define MG_I2C_CTRL_PIN         (1U << MG_I2C_CTRL_PIN_BIT)   /* PIN: Clears status register; initiates transfer */

/* Status Register Bit Positions */
#define MG_I2C_STATUS_BB_BIT        (0U)   /* Bus busy bit position */
#define MG_I2C_STATUS_LAB_BIT       (1U)   /* Lost arbitration bit position */
#define MG_I2C_STATUS_AD0_LRB_BIT   (3U)   /* Address 0 / last received bit position */
#define MG_I2C_STATUS_BER_BIT       (4U)   /* Bus error bit position */
#define MG_I2C_STATUS_STS_BIT       (7U)   /* Externally generated stop bit position */

/* Status Register Bit Masks */
#define MG_I2C_STATUS_BB        (1U << MG_I2C_STATUS_BB_BIT)        /* BB:     Bus is busy (active transfer in progress) */
#define MG_I2C_STATUS_LAB       (1U << MG_I2C_STATUS_LAB_BIT)       /* LAB:    Lost arbitration; another master won the bus */
#define MG_I2C_STATUS_AD0_LRB   (1U << MG_I2C_STATUS_AD0_LRB_BIT)  /* AD0/LRB: Address 0 match or last received bit value */
#define MG_I2C_STATUS_BER       (1U << MG_I2C_STATUS_BER_BIT)       /* BER:    Bus error detected (misplaced start/stop) */
#define MG_I2C_STATUS_STS       (1U << MG_I2C_STATUS_STS_BIT)       /* STS:    Externally generated stop detected */

/* I2C Controller Base Addresses */
#define MG_I2C0_BASE    0x44000   /* Base address of I2C controller 0 */
#define MG_I2C1_BASE    0x44100   /* Base address of I2C controller 1 */

/* I2C Clock Configuration */
#define MG_I2C_PRESCALER    1U              /* Clock prescaler value for SCL frequency */
#define MG_I2C_SCL_MASK     0xFFFFFFFFU    /* Mask for SCL clock divider register (all bits) */

/* I2C Bus Condition Flags (used internally to track transfer state) */
#define MG_I2C_START_BIT    (1U << 1U)   /* Flag: start condition is pending */
#define MG_I2C_STOP_BIT     (1U << 2U)   /* Flag: stop condition is pending */

/* I2C Control Register Command Words */
#define MG_I2C_IDLE         (MG_I2C_CTRL_ESO | MG_I2C_CTRL_ACK)
/* IDLE: Bus idle state — serial output enabled, ACK enabled, no start/stop */

#define MG_I2C_NACK         (MG_I2C_CTRL_ESO)
/* NACK: Send negative acknowledge — serial output enabled, ACK not set */

#define MG_I2C_START        (MG_I2C_CTRL_PIN | MG_I2C_CTRL_ESO | MG_I2C_CTRL_STA | MG_I2C_CTRL_ACK)
/* START: Generate start condition — clears status (PIN), enables output (ESO), asserts STA and ACK */

#define MG_I2C_STOP         (MG_I2C_CTRL_PIN | MG_I2C_CTRL_ESO | MG_I2C_CTRL_STO | MG_I2C_CTRL_ACK)
/* STOP: Generate stop condition — clears status (PIN), enables output (ESO), asserts STO and ACK */

#define MG_I2C_REPSTART     (MG_I2C_CTRL_ESO | MG_I2C_CTRL_STA | MG_I2C_CTRL_ACK)
/* REPSTART: Generate repeated start — enables output (ESO), asserts STA and ACK without clearing status */

#ifndef __ARCH_RISCV_SRC_MINDGROVE_MINDGROVE_I2C_H
#define __ARCH_RISCV_SRC_MINDGROVE_MINDGROVE_I2C_H

#include <nuttx/i2c/i2c_master.h>


FAR struct i2c_master_s *mg_i2c_initialize(int bus);

#endif /* __ARCH_RISCV_SRC_MINDGROVE_MINDGROVE_I2C_H */