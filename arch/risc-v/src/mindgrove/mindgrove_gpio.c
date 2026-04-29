#include <nuttx/config.h>
#include <sys/types.h>
#include <errno.h>
#include <arch/irq.h>
#include <debug.h>
#include <nuttx/irq.h>
#include <nuttx/ioexpander/gpio.h>

#if defined(CONFIG_DEV_GPIO)

#include "mindgrove_gpio.h"
#include "secure_iot_reg.h"
#include "mindgrove_irq.h"

/* GPIO layout:
 * 0..31  -> GPIO_BASE
 * 32..44 -> GPIO_PINMUX_BASE
 */

#define MINDGROVE_NGPIO        45
#define GPIO_VALID(p)          ((p) < MINDGROVE_NGPIO)

/* Returns the MMIO base and bit position for a given pin */

#define GPIO_BANK(p)           ((p) < 32 ? GPIO_BASE       : GPIO_PINMUX_BASE)
#define GPIO_BIT(p)            ((p) < 32 ? (p)             : (p) - 32)
#define GPIO_MASK(p)           (1u << GPIO_BIT(p))

/* IRQ is a pure function of pin id — not stored in the struct */

#define GPIO_IRQ(p)            (MINDGROVE_PLIC_START + GPIO0_IRQn + (p))

struct mindgrove_gpio_s
{
  struct gpio_dev_s gpio;
  pin_interrupt_t   cb;
  uint8_t           id;
};

static int mg_read(FAR struct gpio_dev_s *dev, FAR bool *value);
static int mg_write(FAR struct gpio_dev_s *dev, bool value);
static int mg_attach(FAR struct gpio_dev_s *dev, pin_interrupt_t cb);
static int mg_enable(FAR struct gpio_dev_s *dev, bool enable);
static int mg_settype(FAR struct gpio_dev_s *dev, enum gpio_pintype_e type);

static const struct gpio_operations_s g_gpioops =
{
  .go_read       = mg_read,
  .go_write      = mg_write,
  .go_setpintype = mg_settype,
  .go_attach     = mg_attach,
  .go_enable     = mg_enable,
};

static struct mindgrove_gpio_s g_gpio[MINDGROVE_NGPIO];

static int mg_read(FAR struct gpio_dev_s *dev, FAR bool *value)
{
  FAR struct mindgrove_gpio_s *priv = (FAR struct mindgrove_gpio_s *)dev;

  if (!priv  || !value || !GPIO_VALID(priv->id))
    return -EINVAL;

  *value = (getreg32(GPIO_BANK(priv->id) + GPIO_DATA_OFFSET) &
            GPIO_MASK(priv->id)) != 0;
  return OK;
}

static int mg_write(FAR struct gpio_dev_s *dev, bool value)
{
  FAR struct mindgrove_gpio_s *priv = (FAR struct mindgrove_gpio_s *)dev;

  if (!priv || !GPIO_VALID(priv->id))
    return -EINVAL;

  putreg32(GPIO_MASK(priv->id),
           GPIO_BANK(priv->id) + (value ? GPIO_SET_OFFSET : GPIO_CLEAR_OFFSET));
  return OK;
}

static int mg_isr(int irq, FAR void *context, FAR void *arg)
{
  FAR struct mindgrove_gpio_s *priv = arg;

  if (priv && priv->cb)
    priv->cb(&priv->gpio, priv->id);

  return OK;
}

/* mg_attach: store callback and program interrupt polarity.
 * mg_enable: arm or disarm the IRQ line.
 * Callers must attach before enabling. */

static int mg_attach(FAR struct gpio_dev_s *dev, pin_interrupt_t cb)
{
  FAR struct mindgrove_gpio_s *priv = (FAR struct mindgrove_gpio_s *)dev;
  uint32_t reg;
  bool     high_trig;

  if (!priv || !GPIO_VALID(priv->id))
    return -EINVAL;

  priv->cb = cb;
  if (!cb)
    return OK;

  switch (dev->gp_pintype)
    {
      case GPIO_INTERRUPT_RISING_PIN:
      case GPIO_INTERRUPT_HIGH_PIN:
        high_trig = true;
        break;

      case GPIO_INTERRUPT_FALLING_PIN:
      case GPIO_INTERRUPT_LOW_PIN:
        high_trig = false;
        break;

      default:
        return -EINVAL;
    }

  reg = getreg32(GPIO_BANK(priv->id) + GPIO_INTR_OFFSET);
  reg = high_trig ? (reg | GPIO_MASK(priv->id))
                  : (reg & ~GPIO_MASK(priv->id));
  putreg32(reg, GPIO_BANK(priv->id) + GPIO_INTR_OFFSET);

  return OK;
}

static int mg_enable(FAR struct gpio_dev_s *dev, bool enable)
{
  FAR struct mindgrove_gpio_s *priv = (FAR struct mindgrove_gpio_s *)dev;
  unsigned int irq;
  int ret;

  if (!priv || !GPIO_VALID(priv->id))
    return -EINVAL;
  irq = GPIO_IRQ(priv->id);
  if (enable)
    {
      ret = irq_attach(irq, mg_isr, priv);
      if (ret < 0)
        return ret;

      up_enable_irq(irq);
    }
  else
    {
      up_disable_irq(irq);
      irq_detach(irq);
    }

  return OK;
}

static int mg_settype(FAR struct gpio_dev_s *dev, enum gpio_pintype_e type)
{
  FAR struct mindgrove_gpio_s *priv = (FAR struct mindgrove_gpio_s *)dev;
  uint32_t reg;
  bool     output;

  if (!priv || !GPIO_VALID(priv->id))
    return -EINVAL;

  switch (type)
    {
      case GPIO_OUTPUT_PIN:
        output = true;
        break;

      case GPIO_INPUT_PIN:
      case GPIO_INTERRUPT_RISING_PIN:
      case GPIO_INTERRUPT_FALLING_PIN:
      case GPIO_INTERRUPT_HIGH_PIN:
      case GPIO_INTERRUPT_LOW_PIN:
        output = false;
        break;

      default:
        return -EINVAL;
    }

  reg = getreg32(GPIO_BANK(priv->id) + GPIO_DIRECTION_OFFSET);
  reg = output ? (reg | GPIO_MASK(priv->id))
               : (reg & ~GPIO_MASK(priv->id));
  putreg32(reg, GPIO_BANK(priv->id) + GPIO_DIRECTION_OFFSET);

  dev->gp_pintype = type;
  return OK;
}

int mindgrove_gpio_init(void)
{
  int i;
  int ret;

  for (i = 0; i < MINDGROVE_NGPIO; i++)
    {
      g_gpio[i].gpio.gp_ops = &g_gpioops;
      g_gpio[i].id          = i;
      g_gpio[i].cb          = NULL;

      ret = gpio_pin_register(&g_gpio[i].gpio, i);
      if (ret < 0)
        return ret;

      ret = mg_settype(&g_gpio[i].gpio, GPIO_INPUT_PIN);
      if (ret < 0)
        return ret;
    }

  return OK;
}

#endif