#include <nuttx/config.h>
#include <sys/types.h>
#include <syslog.h>
#include <assert.h>
#include <debug.h>
#include <errno.h>
#include <nuttx/irq.h>
#include <arch/irq.h>
#include <nuttx/ioexpander/gpio.h>
#if defined(CONFIG_DEV_GPIO)

#include "mindgrove_gpio.h"
#include "secure_iot_reg.h"
#include "plic.h"
#include "riscv_internal.h"
#include "mindgrove_irq.h"
#ifndef OK
#  define OK 0
#endif

#define PWM_MASK_0_7    0x000000FF
#define PWM_MASK_17_22  0x007E0000
#define UART_MASK       0x8B00
#define SPI_MASK        0x003F
#define GPT_MASK        0x03C0
#define JTAG_MASK       0x1C00
uint8_t i,j;

unsigned int* pinmux_reg = (unsigned int* ) PINMUX0_BASE;

#define ENOINST  143 /*invalid uart instance*/

#define BOARD_NGPIO 3

#define MINDGROVE_IRQ_GPIO_INT0 (MINDGROVE_PLIC_START)

uint8_t configure_gpio(bool direction, uint64_t gpio_pins)
{
    uint32_t mask_lower = gpio_pins & 0xFFFFFFFFULL;
    uint16_t mask_upper = (gpio_pins >> 32) & 0x1FFFULL;
    uint32_t reg;
    
    if (gpio_pins >> 45) 
    {
        return ENOINST;
    }

    /* Configure lower pins: GPIO0–GPIO31 */
    if (mask_lower)
    {
        if (mask_lower & PWM_MASK_0_7)
        {
            for (i = 0; i <= 7; i++)
            {
                if (((mask_lower >> i) & 1) && (*(pinmux_reg + i) == 1))
                {
                    return EPERM;
                }
            }            
        }

        if (mask_lower & PWM_MASK_17_22)
        {
            for (i = 17; i < 22; i++)
            {
                if (((mask_lower >> i) & 1) && (*(pinmux_reg + i) == 1))
                {
                    return EPERM;
                }
            }
        }

        if (mask_lower & UART_MASK)
        {
            uint8_t uart_pins[] = {8, 9, 11, 15};
            for (j = 0; j < 4; j++)
            {
                i = uart_pins[j];
                if (((mask_lower >> i) & 1) && (*(pinmux_reg + i) == 0))
                {
                    return EPERM;
                }
            }
        }

        /* Update GPIO direction */
        reg = getreg32(GPIO_BASE + GPIO_DIRECTION_OFFSET);
        reg &= ~mask_lower;       // Clear bits
        if (direction)
            reg |= mask_lower;    // Set bits if output
        putreg32(reg, GPIO_BASE + GPIO_DIRECTION_OFFSET);
    }

    /* Configure upper pins: GPIO32–GPIO44 */
    if (mask_upper)
    {
        if (mask_upper & SPI_MASK)
        {
            for (i = 32; i <= 37; i++)
            {
                if (((gpio_pins >> i) & 1) && (*(pinmux_reg + i) == 0))
                    return EPERM;
            }
        }

        if (mask_upper & GPT_MASK)
        {
            for (i = 38; i <= 41; i++)
            {
                if (((gpio_pins >> i) & 1) && (*(pinmux_reg + i) == 1))
                    return EPERM;
            }
        }

        if (mask_upper & JTAG_MASK)
        {
            for (i = 42; i <= 44; i++)
            {
                if (((gpio_pins >> i) & 1) && (*(pinmux_reg + i) == 1))
                    return EPERM;
            }
        }

        /* Update GPIO pinmux direction */
        reg = getreg32(GPIO_PINMUX_BASE + GPIO_DIRECTION_OFFSET);
        reg &= ~mask_upper;
        if (direction)
            reg |= mask_upper;
        putreg32(reg, GPIO_PINMUX_BASE + GPIO_DIRECTION_OFFSET);
    }

    return OK;
}


/****************************************************************************
 * Private Types
 ****************************************************************************/

struct mindgrovegpio_dev_s
{
    struct gpio_dev_s gpio;   // NuttX GPIO device structure
    uint8_t id;               // Pin index (0 to NUM_GPIO_PINS-1)
    uint8_t pintype;          // Current pin type: input/output/interrupt
    int irq;
    pin_interrupt_t callback; // Interrupt callback (if used)
};

// Single array for all pins
static struct mindgrovegpio_dev_s g_mindgrove_gpio[BOARD_NGPIO];


/****************************************************************************
 * Private Data
 ****************************************************************************/
static const struct gpio_operations_s gpin_ops =
{
  .go_read       = mg_go_read,
  .go_write      = mg_go_write,
  .go_setpintype  = mg_go_setpintype,
  .go_attach     = mg_go_attach,
  .go_enable     = mg_go_enable,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

int mg_go_read(FAR struct gpio_dev_s *dev, FAR bool *value)
{
    FAR struct mindgrovegpio_dev_s *priv = (FAR struct mindgrovegpio_dev_s *)dev;
    uint32_t mask;
    uint32_t pin_status;

    if (!priv || !value || priv->id > 44)
        return -EINVAL;

    if (priv->id < 32)
    {
        mask = 1U << priv->id;
        pin_status = getreg32(GPIO_BASE + GPIO_DATA_OFFSET) & mask;
    }
    else
    {
        mask = 1U << (priv->id - 32);
        pin_status = getreg32(GPIO_PINMUX_BASE + GPIO_DATA_OFFSET) & mask;
    }

    *value = (pin_status != 0);
    return OK;
}
int mg_go_write(FAR struct gpio_dev_s *dev, bool value)
{
    FAR struct mindgrovegpio_dev_s *mindgrovegpio =
        (FAR struct mindgrovegpio_dev_s *)dev;

    uint32_t mask;

    if (mindgrovegpio->id > 44)
      {
        return -EINVAL;
      }

    /* GPIO 0–31 */
    if (mindgrovegpio->id < 32)
      {
        mask = (1u << mindgrovegpio->id);

        if (value)
          {
            putreg32(mask, GPIO_BASE + GPIO_SET_OFFSET);
          }
        else
          {
            putreg32(mask, GPIO_BASE + GPIO_CLEAR_OFFSET);
          }
      }
    /* GPIO 32–44 */
    else
      {
        mask = (1u << (mindgrovegpio->id - 32));

        if (value)
          {
            putreg32(mask, GPIO_PINMUX_BASE + GPIO_SET_OFFSET);
          }
        else
          {
            putreg32(mask, GPIO_PINMUX_BASE + GPIO_CLEAR_OFFSET);
          }
      }
      (void)getreg32(GPIO_BASE + GPIO_DATA_OFFSET);

    return OK;
}


int mg_go_attach(FAR struct gpio_dev_s *dev, pin_interrupt_t cb)
{
    FAR struct mindgrovegpio_dev_s *priv =
        (FAR struct mindgrovegpio_dev_s *)dev;

    priv->callback = cb;

    if (priv->id > 44)
        return -EINVAL;


    if (cb == NULL)
        return OK;

    uint64_t gpio_mask = (1ULL << priv->id);
    int low_ena;

    switch (priv->pintype)
    {
        case GPIO_INTERRUPT_FALLING_PIN:
        case GPIO_INTERRUPT_LOW_PIN:
            low_ena = 1;
            break;

        case GPIO_INTERRUPT_RISING_PIN:
        case GPIO_INTERRUPT_HIGH_PIN:
            low_ena = 0;
            break;

        default:
            return -EINVAL;
    }
    uint32_t mask_lower = gpio_mask & 0xFFFFFFFFULL;
    uint16_t mask_upper = (gpio_mask >> 32) & 0x1FFFULL;
    uint32_t  reg;
    if (mask_lower)
    {
        reg = getreg32(GPIO_BASE + GPIO_INTR_OFFSET);
        if (low_ena)
           reg &= ~mask_lower;
        else
            reg |= mask_lower;
        putreg32(reg, GPIO_BASE + GPIO_INTR_OFFSET);
    }

    if (mask_upper)
    {
        reg = getreg32(GPIO_PINMUX_BASE + GPIO_INTR_OFFSET);
        if (low_ena)
            reg &= ~mask_upper;
        else
            reg |= mask_upper;
        putreg32(reg, GPIO_PINMUX_BASE + GPIO_INTR_OFFSET);
    }

    return OK;
}

#include <nuttx/signal.h>

static int mg_gpio_isr(int irq, FAR void *context, FAR void *arg)
{
    FAR struct mindgrovegpio_dev_s *priv = arg;
     if (priv && priv->callback)
    {
        priv->callback(&priv->gpio, priv->id);
    }
    else
    {
        //printf(">>> No callback set!\n");
    }
    return OK;
}

int mg_go_enable(FAR struct gpio_dev_s *dev, bool enable)
{
    FAR struct mindgrovegpio_dev_s *priv =
        (FAR struct mindgrovegpio_dev_s *)dev;

    if (!priv || priv->id > 44)
        return -EINVAL;

    int irq = MINDGROVE_PLIC_START + GPIO0_IRQn + priv->id;

    if (enable)
    {
        irq_attach(irq, mg_gpio_isr, priv);
        up_enable_irq(irq);
    }
    else
    {
        up_disable_irq(irq);
        irq_detach(irq);
    }

    return OK;
}




int mg_go_setpintype(FAR struct gpio_dev_s *dev,
                            enum gpio_pintype_e pintype)
{
    FAR struct mindgrovegpio_dev_s *priv =
        (FAR struct mindgrovegpio_dev_s *)dev;

    DEBUGASSERT(priv);
    DEBUGASSERT(priv->id < BOARD_NGPIO);

    uint32_t dir;

    dir = getreg32(GPIO_BASE + GPIO_DIRECTION_OFFSET);

    priv->pintype = pintype;
    dev->gp_pintype   = pintype;

    switch (pintype)
    {
        case GPIO_INPUT_PIN:
            configure_gpio(false, (1ULL << priv->id));
            break;

        case GPIO_OUTPUT_PIN:
            configure_gpio(true, (1ULL << priv->id));
            break;

        case GPIO_INTERRUPT_RISING_PIN:
        case GPIO_INTERRUPT_FALLING_PIN:
        case GPIO_INTERRUPT_HIGH_PIN:
        case GPIO_INTERRUPT_LOW_PIN:
        /* Direction = input for interrupts */
        configure_gpio(false, (1ULL << priv->id));
        break;
        
        default:
        return -EINVAL;
      }
      dir = getreg32(GPIO_BASE + GPIO_DIRECTION_OFFSET);
      
      return OK;
    }
    
/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: mindgrove_gpio_init
 ****************************************************************************/
int mindgrove_gpio_init(void)
{
    for (int i = 0; i < BOARD_NGPIO; i++)
    {

        g_mindgrove_gpio[i].gpio.gp_ops     = &gpin_ops;
        g_mindgrove_gpio[i].gpio.gp_pintype = GPIO_INPUT_PIN;
        g_mindgrove_gpio[i].id              = i;

        gpio_pin_register(&g_mindgrove_gpio[i].gpio, i);

        configure_gpio(false, (1ULL << i)); /* default INPUT */
    }

    return OK;
}

 #endif/* CONFIG_DEV_GPIO && !CONFIG_mindgrove_gpio_LOWER_HALF */

