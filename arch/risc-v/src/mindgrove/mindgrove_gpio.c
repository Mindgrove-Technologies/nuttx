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

#define MINDGROVE_NGPIO        3
#define GPIO_VALID(p)          ((p) < MINDGROVE_NGPIO)

#define GPIO_LOWER(x)          ((uint32_t)((x) & 0xffffffffULL))
#define GPIO_UPPER(x)          ((uint16_t)(((x) >> 32) & 0x1fffULL))

#define GPIO_IRQ(p)            (MINDGROVE_PLIC_START + GPIO0_IRQn + (p))

struct mindgrove_gpio_s
{
  struct gpio_dev_s gpio;
  pin_interrupt_t cb;
  int irq;
  uint8_t id;
  uint8_t type;
};

static int mg_read(FAR struct gpio_dev_s *dev, FAR bool *value);
static int mg_write(FAR struct gpio_dev_s *dev, bool value);
static int mg_attach(FAR struct gpio_dev_s *dev, pin_interrupt_t cb);
static int mg_enable(FAR struct gpio_dev_s *dev, bool enable);
static int mg_settype(FAR struct gpio_dev_s *dev,
                      enum gpio_pintype_e type);

static const struct gpio_operations_s g_gpioops =
{
  .go_read       = mg_read,
  .go_write      = mg_write,
  .go_setpintype = mg_settype,
  .go_attach     = mg_attach,
  .go_enable     = mg_enable,
};

static struct mindgrove_gpio_s g_gpio[MINDGROVE_NGPIO];

/* Configure direction: false=input, true=output */

static int mg_cfg(bool output, uint64_t pins)
{
  uint32_t reg;
  uint32_t low  = GPIO_LOWER(pins);
  uint16_t high = GPIO_UPPER(pins);

  if ((pins >> MINDGROVE_NGPIO) != 0)
    return -EINVAL;

  reg = getreg32(GPIO_BASE + GPIO_DIRECTION_OFFSET);
  reg = output ? (reg | low) : (reg & ~low);
  putreg32(reg, GPIO_BASE + GPIO_DIRECTION_OFFSET);

  reg = getreg32(GPIO_PINMUX_BASE + GPIO_DIRECTION_OFFSET);
  reg = output ? (reg | high) : (reg & ~high);
  putreg32(reg, GPIO_PINMUX_BASE + GPIO_DIRECTION_OFFSET);

  return OK;
}

static int mg_read(FAR struct gpio_dev_s *dev, FAR bool *value)
{
  FAR struct mindgrove_gpio_s *priv = (FAR struct mindgrove_gpio_s *)dev;
  uint32_t mask;
  uint32_t reg;

  if (!priv || !value || !GPIO_VALID(priv->id))
    return -EINVAL;

  if (priv->id < 32)
    {
      mask = 1u << priv->id;
      reg  = getreg32(GPIO_BASE + GPIO_DATA_OFFSET);
    }
  else
    {
      mask = 1u << (priv->id - 32);
      reg  = getreg32(GPIO_PINMUX_BASE + GPIO_DATA_OFFSET);
    }

  *value = ((reg & mask) != 0);
  return OK;
}

static int mg_write(FAR struct gpio_dev_s *dev, bool value)
{
  FAR struct mindgrove_gpio_s *priv = (FAR struct mindgrove_gpio_s *)dev;
  uint32_t mask;

  if (!priv || !GPIO_VALID(priv->id))
    return -EINVAL;

  if (priv->id < 32)
    {
      mask = 1u << priv->id;
      putreg32(mask, GPIO_BASE +
               (value ? GPIO_SET_OFFSET : GPIO_CLEAR_OFFSET));
    }
  else
    {
      mask = 1u << (priv->id - 32);
      putreg32(mask, GPIO_PINMUX_BASE +
               (value ? GPIO_SET_OFFSET : GPIO_CLEAR_OFFSET));
    }

  return OK;
}

static int mg_attach(FAR struct gpio_dev_s *dev, pin_interrupt_t cb)
{
  FAR struct mindgrove_gpio_s *priv = (FAR struct mindgrove_gpio_s *)dev;
  uint64_t pins;
  uint32_t reg;
  uint32_t low;
  uint16_t high;
  bool high_trig;

  if (!priv || !GPIO_VALID(priv->id))
    return -EINVAL;

  priv->cb = cb;
  if (!cb)
    return OK;

  switch (priv->type)
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

  pins = 1ULL << priv->id;
  low  = GPIO_LOWER(pins);
  high = GPIO_UPPER(pins);

  if (low)
    {
      reg = getreg32(GPIO_BASE + GPIO_INTR_OFFSET);
      reg = high_trig ? (reg | low) : (reg & ~low);
      putreg32(reg, GPIO_BASE + GPIO_INTR_OFFSET);
    }

  if (high)
    {
      reg = getreg32(GPIO_PINMUX_BASE + GPIO_INTR_OFFSET);
      reg = high_trig ? (reg | high) : (reg & ~high);
      putreg32(reg, GPIO_PINMUX_BASE + GPIO_INTR_OFFSET);
    }

  return OK;
}

static int mg_isr(int irq, FAR void *context, FAR void *arg)
{
  FAR struct mindgrove_gpio_s *priv = arg;

  if (priv && priv->cb)
    priv->cb(&priv->gpio, priv->id);

  return OK;
}

static int mg_enable(FAR struct gpio_dev_s *dev, bool enable)
{
  FAR struct mindgrove_gpio_s *priv = (FAR struct mindgrove_gpio_s *)dev;
  int ret;

  if (!priv || !GPIO_VALID(priv->id))
    return -EINVAL;

  if (enable)
    {
      ret = irq_attach(priv->irq, mg_isr, priv);
      if (ret < 0)
        return ret;

      up_enable_irq(priv->irq);
    }
  else
    {
      up_disable_irq(priv->irq);
      irq_detach(priv->irq);
    }

  return OK;
}

static int mg_settype(FAR struct gpio_dev_s *dev,
                      enum gpio_pintype_e type)
{
  FAR struct mindgrove_gpio_s *priv = (FAR struct mindgrove_gpio_s *)dev;

  if (!priv || !GPIO_VALID(priv->id))
    return -EINVAL;

  priv->type = type;
  dev->gp_pintype = type;

  switch (type)
    {
      case GPIO_OUTPUT_PIN:
        return mg_cfg(true, 1ULL << priv->id);

      case GPIO_INPUT_PIN:
      case GPIO_INTERRUPT_RISING_PIN:
      case GPIO_INTERRUPT_FALLING_PIN:
      case GPIO_INTERRUPT_HIGH_PIN:
      case GPIO_INTERRUPT_LOW_PIN:
        return mg_cfg(false, 1ULL << priv->id);

      default:
        return -EINVAL;
    }
}

int mindgrove_gpio_init(void)
{
  int i;
  int ret;

  for (i = 0; i < MINDGROVE_NGPIO; i++)
    {
      g_gpio[i].gpio.gp_ops     = &g_gpioops;
      g_gpio[i].gpio.gp_pintype = GPIO_INPUT_PIN;
      g_gpio[i].id              = i;
      g_gpio[i].type            = GPIO_INPUT_PIN;
      g_gpio[i].irq             = GPIO_IRQ(i);
      g_gpio[i].cb              = NULL;

      ret = gpio_pin_register(&g_gpio[i].gpio, i);
      if (ret < 0)
        return ret;

      ret = mg_cfg(false, 1ULL << i);
      if (ret < 0)
        return ret;
    }

  return OK;
}

#endif