/****************************************************************************
 * chip/mindgrove_pwm.c
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/arch.h>
#include <nuttx/timers/pwm.h>
#include <stdio.h>
#include <errno.h>

#include "riscv_internal.h"
#include "mindgrove_pwm.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define MG_PWM_CLOCK_FREQ  35000000
#define MG_PWM_PERIOD_MAX  65535

/* Register accessors using priv->base */

#define PWM_CLOCK_CTRL_OFFSET     0x0000
#define PWM_CTRL_OFFSET           0x0004
#define PWM_PERIOD_OFFSET         0x0008
#define PWM_DUTY_CYCLE_OFFSET     0x000C
#define PWM_DEADBAND_DELAY_OFFSET 0x0010

#define PWM_CLOCK_CTRL(base)     ((base) + PWM_CLOCK_CTRL_OFFSET)
#define PWM_CTRL(base)           ((base) + PWM_CTRL_OFFSET)
#define PWM_PERIOD(base)         ((base) + PWM_PERIOD_OFFSET)
#define PWM_DUTY_CYCLE(base)     ((base) + PWM_DUTY_CYCLE_OFFSET)
#define PWM_DEADBAND_DELAY(base) ((base) + PWM_DEADBAND_DELAY_OFFSET)

/* PWM base addresses */

#define PWM0_BASE   0x00030000
#define PWM1_BASE   0x00030100
#define PWM2_BASE   0x00030200
#define PWM3_BASE   0x00030300
#define PWM4_BASE   0x00030400
#define PWM5_BASE   0x00030500
#define PWM6_BASE   0x00030600
#define PWM7_BASE   0x00030700
#define PWM8_BASE   0x00030800
#define PWM9_BASE   0x00030900
#define PWM10_BASE  0x00030A00
#define PWM11_BASE  0x00030B00
#define PWM12_BASE  0x00030C00
#define PWM13_BASE  0x00030D00

/* PWM control register bits */

#define BIT(x)                      (1U << (x))
#define PWM_CTRL_EN                 BIT(0)
#define PWM_CTRL_START              BIT(1)
#define PWM_CTRL_OUTPUT_EN          BIT(2)
#define PWM_CTRL_OUTPUT_POLARITY    BIT(3)
#define PWM_CTRL_COUNTER_RESET      BIT(4)
#define PWM_CTRL_HALFPERIOD_INTR_EN BIT(6)
#define PWM_CTRL_FALL_INTR_EN       BIT(7)
#define PWM_CTRL_RISE_INTR_EN       BIT(8)
#define PWM_CTRL_HALFPERIOD_INTR    BIT(9)
#define PWM_CTRL_FALL_INTR          BIT(10)
#define PWM_CTRL_RISE_INTR          BIT(11)
#define PWM_CTRL_UPDATE_EN          BIT(12)

/* Pinmux base and mux registers */

#define PINMUX_BASE  0x00040400
#define PINMUX_MUX0  (PINMUX_BASE + 0x0000)
#define PINMUX_MUX1  (PINMUX_BASE + 0x0004)
#define PINMUX_MUX2  (PINMUX_BASE + 0x0008)
#define PINMUX_MUX3  (PINMUX_BASE + 0x000C)
#define PINMUX_MUX4  (PINMUX_BASE + 0x0010)
#define PINMUX_MUX5  (PINMUX_BASE + 0x0014)
#define PINMUX_MUX6  (PINMUX_BASE + 0x0018)
#define PINMUX_MUX7  (PINMUX_BASE + 0x001C)
#define PINMUX_MUX8  (PINMUX_BASE + 0x0020)
#define PINMUX_MUX9  (PINMUX_BASE + 0x0024)
#define PINMUX_MUX10 (PINMUX_BASE + 0x0028)
#define PINMUX_MUX11 (PINMUX_BASE + 0x002C)
#define PINMUX_MUX12 (PINMUX_BASE + 0x0030)
#define PINMUX_MUX13 (PINMUX_BASE + 0x0034)

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct mg_pwm_priv_s
{
  struct pwm_lowerhalf_s dev;  /* Must be first — NuttX lower-half base */
  uint32_t               base;
  uint16_t               channel;
  bool                   started;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int mg_pwm_setup(FAR struct pwm_lowerhalf_s *dev);
static int mg_pwm_shutdown(FAR struct pwm_lowerhalf_s *dev);

#ifdef CONFIG_PWM_PULSECOUNT
static int mg_pwm_start(FAR struct pwm_lowerhalf_s *dev,
                        FAR const struct pwm_info_s *info,
                        FAR void *handle);
#else
static int mg_pwm_start(FAR struct pwm_lowerhalf_s *dev,
                        FAR const struct pwm_info_s *info);
#endif

static int mg_pwm_stop(FAR struct pwm_lowerhalf_s *dev);

static const struct pwm_ops_s g_pwm_ops =
{
  .setup    = mg_pwm_setup,
  .shutdown = mg_pwm_shutdown,
  .start    = mg_pwm_start,
  .stop     = mg_pwm_stop,
  .ioctl    = NULL,
};


#define MG_PWM_INIT(n) \
  { .dev = { .ops = &g_pwm_ops }, .base = PWM##n##_BASE, .channel = n, .started = false }

static struct mg_pwm_priv_s pwm0  = MG_PWM_INIT(0);
static struct mg_pwm_priv_s pwm1  = MG_PWM_INIT(1);
static struct mg_pwm_priv_s pwm2  = MG_PWM_INIT(2);
static struct mg_pwm_priv_s pwm3  = MG_PWM_INIT(3);
static struct mg_pwm_priv_s pwm4  = MG_PWM_INIT(4);
static struct mg_pwm_priv_s pwm5  = MG_PWM_INIT(5);
static struct mg_pwm_priv_s pwm6  = MG_PWM_INIT(6);
static struct mg_pwm_priv_s pwm7  = MG_PWM_INIT(7);
static struct mg_pwm_priv_s pwm8  = MG_PWM_INIT(8);
static struct mg_pwm_priv_s pwm9  = MG_PWM_INIT(9);
static struct mg_pwm_priv_s pwm10 = MG_PWM_INIT(10);
static struct mg_pwm_priv_s pwm11 = MG_PWM_INIT(11);
static struct mg_pwm_priv_s pwm12 = MG_PWM_INIT(12);
static struct mg_pwm_priv_s pwm13 = MG_PWM_INIT(13);

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static inline int pinmux_pwm(uint8_t num, bool enable)
{
  uint32_t mux_val = enable ? 1U : 0U;
  uint32_t mux;

  switch (num)
    {
      case 0:  mux = PINMUX_MUX0;  break;
      case 1:  mux = PINMUX_MUX1;  break;
      case 2:  mux = PINMUX_MUX2;  break;
      case 3:  mux = PINMUX_MUX3;  break;
      case 4:  mux = PINMUX_MUX4;  break;
      case 5:  mux = PINMUX_MUX5;  break;
      case 6:  mux = PINMUX_MUX6;  break;
      case 7:  mux = PINMUX_MUX7;  break;
      case 8:  mux = PINMUX_MUX8;  break;
      case 9:  mux = PINMUX_MUX9;  break;
      case 10: mux = PINMUX_MUX10; break;
      case 11: mux = PINMUX_MUX11; break;
      case 12: mux = PINMUX_MUX12; break;
      case 13: mux = PINMUX_MUX13; break;
      default:
        printf("mindgrove_pwm: invalid PWM channel %u (max 13)\n", num);
        return -ENODEV;
    }
  /*Write pin mux register*/
  putreg32(mux_val, mux);

  return EXIT_SUCCESS;
}

/****************************************************************************
 * Name: mg_pwm_setup
 ****************************************************************************/

static int mg_pwm_setup(FAR struct pwm_lowerhalf_s *dev)
{
  FAR struct mg_pwm_priv_s *priv = (FAR struct mg_pwm_priv_s *)dev;

  return pinmux_pwm(priv->channel, true);
}

/****************************************************************************
 * Name: mg_pwm_shutdown
 ****************************************************************************/

static int mg_pwm_shutdown(FAR struct pwm_lowerhalf_s *dev)
{
  FAR struct mg_pwm_priv_s *priv = (FAR struct mg_pwm_priv_s *)dev;

  /* Stop PWM output */

  modifyreg32(PWM_CTRL(priv->base),
              PWM_CTRL_START | PWM_CTRL_OUTPUT_EN | PWM_CTRL_EN,
              0);

  /* Disable pinmux for this channel */

  pinmux_pwm(priv->channel, false);

  priv->started = false;
  return EXIT_SUCCESS;
}

/****************************************************************************
 * Name: mg_pwm_start
 ****************************************************************************/

#ifdef CONFIG_PWM_PULSECOUNT
static int mg_pwm_start(FAR struct pwm_lowerhalf_s *dev,
                        FAR const struct pwm_info_s *info,
                        FAR void *handle)
#else
static int mg_pwm_start(FAR struct pwm_lowerhalf_s *dev,
                        FAR const struct pwm_info_s *info)
#endif
{
  FAR struct mg_pwm_priv_s *priv = (FAR struct mg_pwm_priv_s *)dev;

  uint32_t freq      = info->frequency;
  uint32_t prescaler = 0;
  uint32_t period;
  uint32_t duty;

  if (freq == 0)
    {
      return -EINVAL;
    }

  /* Calculate prescaler and period */

  period = MG_PWM_CLOCK_FREQ / freq;

  while (period > MG_PWM_PERIOD_MAX && prescaler < 0xFFFF)
    {
      prescaler++;
      period = MG_PWM_CLOCK_FREQ / (freq * (prescaler + 1));
    }

  /* Convert NuttX duty cycle (16-bit fraction, 0x0000–0xFFFF) to ticks */

#ifdef CONFIG_PWM_MULTICHAN
  duty = (period * info->channels[0].duty) >> 16;
#else
  duty = (period * info->duty) >> 16;
#endif

  /* Program hardware registers */

  putreg32(prescaler << 1,PWM_CLOCK_CTRL(priv->base));
  putreg32(period,         PWM_PERIOD(priv->base));
  putreg32(duty,           PWM_DUTY_CYCLE(priv->base));

  modifyreg32(PWM_CTRL(priv->base),
              0,
              PWM_CTRL_EN        |
              PWM_CTRL_START     |
              PWM_CTRL_OUTPUT_EN |
              PWM_CTRL_UPDATE_EN);

  priv->started = true;
  return EXIT_SUCCESS;
}

/****************************************************************************
 * Name: mg_pwm_stop
 ****************************************************************************/

static int mg_pwm_stop(FAR struct pwm_lowerhalf_s *dev)
{
  FAR struct mg_pwm_priv_s *priv = (FAR struct mg_pwm_priv_s *)dev;

  modifyreg32(PWM_CTRL(priv->base),
              PWM_CTRL_START | PWM_CTRL_OUTPUT_EN | PWM_CTRL_EN,
              0);

  modifyreg32(PWM_CTRL(priv->base), 0, PWM_CTRL_COUNTER_RESET);

  priv->started = false;
  return EXIT_SUCCESS;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

FAR struct pwm_lowerhalf_s *mg_pwm_initialize(int pwm)
{
  FAR struct mg_pwm_priv_s *priv;

  switch (pwm)
    {
      case 0:  priv = &pwm0;  break;
      case 1:  priv = &pwm1;  break;
      case 2:  priv = &pwm2;  break;
      case 3:  priv = &pwm3;  break;
      case 4:  priv = &pwm4;  break;
      case 5:  priv = &pwm5;  break;
      case 6:  priv = &pwm6;  break;
      case 7:  priv = &pwm7;  break;
      case 8:  priv = &pwm8;  break;
      case 9:  priv = &pwm9;  break;
      case 10: priv = &pwm10; break;
      case 11: priv = &pwm11; break;
      case 12: priv = &pwm12; break;
      case 13: priv = &pwm13; break;
      default:
        return NULL;
    }

  priv->started = false;

  return &priv->dev;
}