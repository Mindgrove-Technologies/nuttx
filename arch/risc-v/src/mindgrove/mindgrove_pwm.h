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

FAR struct pwm_lowerhalf_s *mg_pwm_initialize(int pwm);