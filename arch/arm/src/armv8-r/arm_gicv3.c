/***************************************************************************
 * arch/arm/src/armv8-r/arm_gicv3.c
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
 ***************************************************************************/

/***************************************************************************
 * Included Files
 ***************************************************************************/

#include <nuttx/config.h>
#include <debug.h>
#include <assert.h>

#include <nuttx/arch.h>
#include <arch/armv8-r/cp15.h>
#include <arch/barriers.h>
#include <arch/irq.h>
#include <arch/chip/chip.h>
#include <sched/sched.h>

#include "arm_internal.h"
#include "arm_gic.h"

/***************************************************************************
 * Pre-processor Definitions
 ***************************************************************************/

#define MIN(_x,_y) ((_x<_y) ? _x : _y)

#define GICR_TYPER_NR_PPIS(r)                   \
  ({                                            \
    unsigned int __ppinum = ((r) >> 27) & 0x1f; \
    unsigned int __nr_ppis = 16;                \
    if (__ppinum == 1 || __ppinum == 2)         \
        {  __nr_ppis +=  __ppinum * 32;  }      \
    __nr_ppis;                                  \
  })

/* selects redistributor SGI_base for current core for PPI and SGI
 * selects distributor base for SPI
 * The macro translates to distributor base for GICv2 and GICv1
 */
#define GET_DIST_BASE(intid)  ((intid < GIC_SPI_INT_BASE) ?          \
                               (gic_get_rdist() + GICR_SGI_BASE_OFF) \
                               : GIC_DIST_BASE)

#define IGROUPR_VAL  0xFFFFFFFFU

/***************************************************************************
 * Private Data
 ***************************************************************************/

/* Redistributor base addresses for each core */

static unsigned long g_gic_rdists[CONFIG_SMP_NCPUS];
static volatile spinlock_t g_gic_lock = SP_UNLOCKED;

/***************************************************************************
 * Private Functions
 ***************************************************************************/

static inline void sys_set_bit(unsigned long addr, unsigned int bit)
{
  uint32_t temp;

  temp = getreg32(addr);
  temp = temp | (BIT(bit));
  putreg32(temp, addr);
}

static inline void sys_clear_bit(unsigned long addr, unsigned int bit)
{
  uint32_t temp;

  temp = getreg32(addr);
  temp = temp & ~(BIT(bit));
  putreg32(temp, addr);
}

static inline int sys_test_bit(unsigned long addr, unsigned int bit)
{
  uint32_t temp;

  temp = getreg32(addr);
  return (temp & BIT(bit));
}

static inline unsigned long gic_get_rdist(void)
{
  return g_gic_rdists[this_cpu()];
}

static inline uint32_t read_gicd_wait_rwp(void)
{
  uint32_t value;

  value = getreg32(GICD_CTLR);

  while (value & BIT(GICD_CTLR_RWP))
    {
      value = getreg32(GICD_CTLR);
    }

  return value;
}

/* Wait for register write pending
 * TODO: add timed wait
 */

static int gic_wait_rwp(uint32_t intid)
{
  uint32_t      rwp_mask;
  unsigned long base;

  if (intid < GIC_SPI_INT_BASE)
    {
      base        = (gic_get_rdist() + GICR_CTLR);
      rwp_mask    = BIT(GICR_CTLR_RWP);
    }
  else
    {
      base        = GICD_CTLR;
      rwp_mask    = BIT(GICD_CTLR_RWP);
    }

  while (getreg32(base) & rwp_mask)
    {
    }

  return 0;
}

static inline void arm_gic_write_irouter(uint64_t val, unsigned int intid)
{
  unsigned long addr = IROUTER(GET_DIST_BASE(intid), intid);

  /* Use two putreg32 instead of one putreg64, because when the neon option
   * is enabled, the compiler may optimize putreg64 to the neon vstr
   * instruction, which will cause a data abort.
   */

  putreg32((uint32_t)val, addr);
  putreg32((uint32_t)(val >> 32) , addr + 4);
}

void arm_gic_irq_set_priority(unsigned int intid, unsigned int prio,
                                uint32_t flags)
{
  uint32_t      mask  = BIT(intid & (GIC_NUM_INTR_PER_REG - 1));
  uint32_t      idx   = intid / GIC_NUM_INTR_PER_REG;
  uint32_t      shift;
  uint32_t      val;
  unsigned long base = GET_DIST_BASE(intid);
  irqstate_t    irq_flags;

  /* Disable the interrupt */

  putreg32(mask, ICENABLER(base, idx));
  gic_wait_rwp(intid);

  /* PRIORITYR registers provide byte access */

  putreg8(prio & GIC_PRI_MASK, IPRIORITYR(base, intid));

  /* Interrupt type config */

  if (!GIC_IS_SGI(intid))
    {
      idx     = intid / GIC_NUM_CFG_PER_REG;
      shift   = (intid & (GIC_NUM_CFG_PER_REG - 1)) * 2;

      /* GICD_ICFGR requires full 32-bit RMW operations.
       * Each interrupt uses 2 bits; thus updates must be synchronized
       * to avoid losing configuration in concurrent environments.
       */

      irq_flags = spin_lock_irqsave(&g_gic_lock);

      val = getreg32(ICFGR(base, idx));
      val &= ~(GICD_ICFGR_MASK << shift);
      if (flags & IRQ_TYPE_EDGE)
        {
          val |= (GICD_ICFGR_TYPE << shift);
        }

      putreg32(val, ICFGR(base, idx));
      spin_unlock_irqrestore(&g_gic_lock, irq_flags);
    }
}

/***************************************************************************
 * Name: arm_gic_irq_trigger
 *
 * Description:
 *   Set the trigger type for the specified IRQ source and the current CPU.
 *
 *   Since this API is not supported on all architectures, it should be
 *   avoided in common implementations where possible.
 *
 * Input Parameters:
 *   irq   - The interrupt request to modify.
 *   flags - irq type, IRQ_TYPE_EDGE or IRQ_TYPE_LEVEL
 *           Default is IRQ_TYPE_LEVEL
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno value is returned on any failure.
 *
 ***************************************************************************/

int arm_gic_irq_trigger(unsigned int intid, uint32_t flags)
{
  uint32_t      idx  = intid / GIC_NUM_INTR_PER_REG;
  uint32_t      shift;
  uint32_t      val;
  unsigned long base = GET_DIST_BASE(intid);
  irqstate_t    irq_flags;

  if (!GIC_IS_SGI(intid))
    {
      idx   = intid / GIC_NUM_CFG_PER_REG;
      shift = (intid & (GIC_NUM_CFG_PER_REG - 1)) * 2;

      /* GICD_ICFGR requires full 32-bit RMW operations.
       * Each interrupt uses 2 bits; thus updates must be synchronized
       * to avoid losing configuration in concurrent environments.
       */

      irq_flags = spin_lock_irqsave(&g_gic_lock);
      val = getreg32(ICFGR(base, idx));
      val &= ~(GICD_ICFGR_MASK << shift);
      if (flags & IRQ_TYPE_EDGE)
        {
          val |= (GICD_ICFGR_TYPE << shift);
        }

      putreg32(val, ICFGR(base, idx));
      spin_unlock_irqrestore(&g_gic_lock, irq_flags);
      return OK;
    }

  return -EINVAL;
}

void arm_gic_irq_enable(unsigned int intid)
{
  uint32_t mask = BIT(intid & (GIC_NUM_INTR_PER_REG - 1));
  uint32_t idx  = intid / GIC_NUM_INTR_PER_REG;

  /* Affinity routing is enabled for Non-secure state (GICD_CTLR.ARE_NS
   * is set to '1' when GIC distributor is initialized) ,so need to set
   * SPI's affinity, now set it to be the PE on which it is enabled.
   */

  if (GIC_IS_SPI(intid))
    {
      arm_gic_write_irouter(up_cpu_index(), intid);
    }

  putreg32(mask, ISENABLER(GET_DIST_BASE(intid), idx));
}

void arm_gic_irq_disable(unsigned int intid)
{
  uint32_t mask = BIT(intid & (GIC_NUM_INTR_PER_REG - 1));
  uint32_t idx  = intid / GIC_NUM_INTR_PER_REG;

  putreg32(mask, ICENABLER(GET_DIST_BASE(intid), idx));

  /* poll to ensure write is complete */

  gic_wait_rwp(intid);
}

bool arm_gic_irq_is_enabled(unsigned int intid)
{
  uint32_t mask = BIT(intid & (GIC_NUM_INTR_PER_REG - 1));
  uint32_t idx  = intid / GIC_NUM_INTR_PER_REG;
  uint32_t val;

  val = getreg32(ISENABLER(GET_DIST_BASE(intid), idx));

  return (val & mask) != 0;
}

#ifdef CONFIG_ARCH_HIPRI_INTERRUPT
void arm_gic_set_group(unsigned int intid, unsigned int group)
{
  uint32_t mask = BIT(intid & (GIC_NUM_INTR_PER_REG - 1));
  uint32_t idx  = intid / GIC_NUM_INTR_PER_REG;
  unsigned long base = GET_DIST_BASE(intid);
  uint32_t igroupr_val;
  uint32_t igroupmodr_val;

  igroupr_val = getreg32(IGROUPR(base, idx));
  igroupmodr_val = getreg32(IGROUPMODR(base, idx));
  if (group == 0)
    {
      igroupr_val &= ~mask;
      igroupmodr_val &= ~mask;
    }
  else
    {
      igroupr_val |= mask;
      igroupmodr_val |= mask;
    }

  putreg32(igroupr_val, IGROUPR(base, idx));
  putreg32(igroupmodr_val, IGROUPMODR(base, idx));
}

static unsigned int arm_gic_get_active_group0(void)
{
  int intid;

  /* (Pending -> Active / AP) or (AP -> AP)
   * Read a Group 0 INTID on an interrupt acknowledge.
   */

  intid = CP15_GET(ICC_IAR0);

  return intid;
}

static void arm_gic_eoi_group0(unsigned int intid)
{
  /* Interrupt request deassertion from peripheral to GIC happens
   * by clearing interrupt condition by a write to the peripheral
   * register. It is desired that the write transfer is complete
   * before the core tries to change GIC state from 'AP/Active' to
   * a new state on seeing 'EOI write'.
   * Since ICC interface writes are not ordered against Device
   * memory writes, a barrier is required to ensure the ordering.
   * The dsb will also ensure *completion* of previous writes with
   * DEVICE nGnRnE attribute.
   */

  UP_DSB();

  /* (AP -> Pending) Or (Active -> Inactive) or (AP to AP) nested case
   * Write a Group 0 interrupt completion
   */

  CP15_SET(ICC_EOIR0, intid);
}
#endif

unsigned int arm_gic_get_active(void)
{
  int intid;

  /* (Pending -> Active / AP) or (AP -> AP) */

  intid = CP15_GET(ICC_IAR1);

  return intid;
}

void arm_gic_eoi(unsigned int intid)
{
  /* Interrupt request deassertion from peripheral to GIC happens
   * by clearing interrupt condition by a write to the peripheral
   * register. It is desired that the write transfer is complete
   * before the core tries to change GIC state from 'AP/Active' to
   * a new state on seeing 'EOI write'.
   * Since ICC interface writes are not ordered against Device
   * memory writes, a barrier is required to ensure the ordering.
   * The dsb will also ensure *completion* of previous writes with
   * DEVICE nGnRnE attribute.
   */

  UP_DSB();

  /* (AP -> Pending) Or (Active -> Inactive) or (AP to AP) nested case */

  CP15_SET(ICC_EOIR1, intid);
}

static int arm_gic_send_sgi(unsigned int sgi_id, uint64_t target_aff,
                              uint16_t target_list)
{
  uint32_t aff3;
  uint32_t aff2;
  uint32_t aff1;
  uint64_t sgi_val;

  ASSERT(GIC_IS_SGI(sgi_id));

  /* Extract affinity fields from target */

  aff1 = MPIDR_AFFLVL(target_aff, 1);
  aff2 = MPIDR_AFFLVL(target_aff, 2);
  aff3 = 0;

  sgi_val = GICV3_SGIR_VALUE(aff3, aff2, aff1, sgi_id, SGIR_IRM_TO_AFF,
                             target_list);

  UP_DSB();
  CP15_SET64(ICC_SGI1R, sgi_val);
  UP_ISB();

  return 0;
}

int arm_gic_raise_sgi(unsigned int sgi_id, uint16_t target_list)
{
  uint64_t pre_cluster_id = UINT64_MAX;
  uint64_t curr_cluster_id;
  uint64_t curr_mpidr;
  uint16_t tlist = 0;
  uint16_t cpu = 0;
  uint16_t i;

  while ((i = ffs(target_list)))
    {
      cpu += (i - 1);

      target_list >>= i;

      curr_mpidr = arm_get_mpid(cpu);
      curr_cluster_id = MPID_TO_CLUSTER_ID(curr_mpidr);

      if (pre_cluster_id != UINT64_MAX &&
          pre_cluster_id != curr_cluster_id)
        {
          arm_gic_send_sgi(sgi_id, pre_cluster_id, tlist);
        }

      tlist |= 1 << (curr_mpidr & MPIDR_AFFLVL_MASK);

      cpu += i;
      pre_cluster_id = curr_cluster_id;
    }

  arm_gic_send_sgi(sgi_id, pre_cluster_id, tlist);

  return 0;
}

/* Wake up GIC redistributor.
 * clear ProcessorSleep and wait till ChildAsleep is cleared.
 * ProcessSleep to be cleared only when ChildAsleep is set
 * Check if redistributor is not powered already.
 */

static void gicv3_rdist_enable(unsigned long rdist)
{
  if (!(getreg32(rdist + GICR_WAKER) & BIT(GICR_WAKER_CA)))
    {
      return;
    }

  /* Power up sequence of the Redistributors for GIC600/GIC700
   * please check GICR_PWRR define at trm of GIC600/GIC700
   */

  putreg32(0x2, rdist + GICR_PWRR);

  sys_clear_bit(rdist + GICR_WAKER, GICR_WAKER_PS);

  while (getreg32(rdist + GICR_WAKER) & BIT(GICR_WAKER_CA))
    {
    }
}

/* Initialize the cpu interface. This should be called by each core. */

static void gicv3_cpuif_init(void)
{
  uint32_t      icc_sre;
  uint32_t      intid;
  unsigned long base = gic_get_rdist() + GICR_SGI_BASE_OFF;

  /* Disable all sgi ppi */

  putreg32(BIT64_MASK(GIC_NUM_INTR_PER_REG), ICENABLER(base, 0));

  /* Any sgi/ppi intid ie. 0-31 will select GICR_CTRL */

  gic_wait_rwp(0);

  /* Clear pending */

  putreg32(BIT64_MASK(GIC_NUM_INTR_PER_REG), ICPENDR(base, 0));

  /* Configure all SGIs/PPIs as G1S or G1NS depending on Zephyr
   * is run in EL1S or EL1NS respectively.
   * All interrupts will be delivered as irq
   */

  putreg32(IGROUPR_VAL, IGROUPR(base, 0));
  putreg32(BIT64_MASK(GIC_NUM_INTR_PER_REG), IGROUPMODR(base, 0));

  /* Configure default priorities for SGI 0:15 and PPI 0:15. */

  for (intid = 0; intid < GIC_SPI_INT_BASE;
       intid += GIC_NUM_PRI_PER_REG)
    {
      putreg32(GIC_INT_DEF_PRI_X4, IPRIORITYR(base, intid));
    }

  /* Configure PPIs as level triggered */

  putreg32(0, ICFGR(base, 1));

  /* Check if system interface can be enabled.
   * 'icc_sre_el3' needs to be configured at 'EL3'
   * to allow access to 'icc_sre_el1' at 'EL1'
   * eg: z_arch_el3_plat_init can be used by platform.
   */

  icc_sre = CP15_GET(ICC_SRE);

  if (!(icc_sre & ICC_SRE_ELX_SRE_BIT))
    {
      icc_sre =
        (icc_sre | ICC_SRE_ELX_SRE_BIT | ICC_SRE_ELX_DIB_BIT |
         ICC_SRE_ELX_DFB_BIT);
      CP15_SET(ICC_SRE, icc_sre);
      icc_sre = CP15_GET(ICC_SRE);

      ASSERT(icc_sre & ICC_SRE_ELX_SRE_BIT);
    }

  CP15_SET(ICC_PMR, GIC_IDLE_PRIO);

#ifdef CONFIG_ARCH_HIPRI_INTERRUPT
  /* Allow group0 interrupts */

  CP15_SET(ICC_IGRPEN0, 1);
#endif

  /* Allow group1 interrupts */

  CP15_SET(ICC_IGRPEN1, 1);
}

static void gicv3_dist_init(void)
{
  unsigned int  num_ints;
  unsigned int  intid;
  unsigned int  idx;
  unsigned long base = GIC_DIST_BASE;

  num_ints  = getreg32(GICD_TYPER);
  num_ints  &= GICD_TYPER_ITLINESNUM_MASK;
  num_ints  = (num_ints + 1) << 5;

  /* Disable the distributor */

  putreg32(0, GICD_CTLR);
  gic_wait_rwp(GIC_SPI_INT_BASE);

#ifdef CONFIG_ARCH_SINGLE_SECURITY_STATE

  /* Before configuration, we need to check whether
   * the GIC single security state mode is supported.
   * Make sure GICD_CTRL_NS is 1.
   */

  sys_set_bit(GICD_CTLR, GICD_CTRL_DS);
  if (!sys_test_bit(GICD_CTLR, GICD_CTRL_DS))
    {
      sinfo("Current GIC does not support single security state\n");
      PANIC();
    }
#endif

  /* Default configuration of all SPIs */

  for (intid = GIC_SPI_INT_BASE; intid < num_ints;
       intid += GIC_NUM_INTR_PER_REG)
    {
      idx = intid / GIC_NUM_INTR_PER_REG;

      /* Disable interrupt */

      putreg32(BIT64_MASK(GIC_NUM_INTR_PER_REG),
               ICENABLER(base, idx));

      /* Clear pending */

      putreg32(BIT64_MASK(GIC_NUM_INTR_PER_REG),
               ICPENDR(base, idx));
      putreg32(IGROUPR_VAL, IGROUPR(base, idx));
      putreg32(BIT64_MASK(GIC_NUM_INTR_PER_REG),
               IGROUPMODR(base, idx));
    }

  /* wait for rwp on GICD */

  gic_wait_rwp(GIC_SPI_INT_BASE);

  /* Configure default priorities for all SPIs. */

  for (intid = GIC_SPI_INT_BASE; intid < num_ints;
       intid += GIC_NUM_PRI_PER_REG)
    {
      putreg32(GIC_INT_DEF_PRI_X4, IPRIORITYR(base, intid));
    }

  /* Configure all SPIs as active low, level triggered by default */

  for (intid = GIC_SPI_INT_BASE; intid < num_ints;
       intid += GIC_NUM_CFG_PER_REG)
    {
      idx = intid / GIC_NUM_CFG_PER_REG;
#ifdef CONFIG_ARMV8R_GIC_SPI_EDGE
      /* Configure all SPIs as edge-triggered by default */

      putreg32(0xaaaaaaaa, ICFGR(base, idx));
#else
      /* Configure all SPIs as level-sensitive by default */

      putreg32(0, ICFGR(base, idx));
#endif
    }

  /* TODO: Some arrch64 Cortex-A core maybe without security state
   * it has different GIC configure with standard arrch64 A or R core
   */

#ifdef CONFIG_ARCH_SINGLE_SECURITY_STATE
  /* For GIC single security state(ARMv8-R), the config means
   * the GIC is under single security state which has
   * only two groups:
   *  group 0 and group 1.
   * Then set GICD_CTLR_ARE and GICD_CTLR_ENABLE_G1 to enable Group 1
   * interrupt.
   * Since the GICD_CTLR_ARE and GICD_CTRL_ARE_S share BIT(4), and
   * similarly the GICD_CTLR_ENABLE_G1 and GICD_CTLR_ENABLE_G1NS share
   * BIT(1), we can reuse them.
   */

#ifdef CONFIG_ARCH_HIPRI_INTERRUPT
  putreg32(BIT(GICD_CTRL_ARE_S) | BIT(GICD_CTLR_ENABLE_G1NS) |
           BIT(GICD_CTLR_ENABLE_G0), GICD_CTLR);
#else
  putreg32(BIT(GICD_CTRL_ARE_S) | BIT(GICD_CTLR_ENABLE_G1NS), GICD_CTLR);
#endif /* CONFIG_ARCH_HIPRI_INTERRUPT */

#else
  /* Enable distributor with ARE */

#ifdef CONFIG_ARCH_HIPRI_INTERRUPT
  putreg32(BIT(GICD_CTRL_ARE_NS) | BIT(GICD_CTLR_ENABLE_G1NS) |
           BIT(GICD_CTLR_ENABLE_G0), GICD_CTLR);
#else
  putreg32(BIT(GICD_CTRL_ARE_NS) | BIT(GICD_CTLR_ENABLE_G1NS), GICD_CTLR);
#endif /* CONFIG_ARCH_HIPRI_INTERRUPT */

#endif

#ifdef CONFIG_SMP
  /* Attach SGI interrupt handlers. This attaches the handler to all CPUs. */

  DEBUGVERIFY(irq_attach(GIC_SMP_SCHED, arm64_smp_sched_handler, NULL));
  DEBUGVERIFY(irq_attach(GIC_SMP_CALL, nxsched_smp_call_handler, NULL));
#endif
}

void up_enable_irq(int irq)
{
  arm_gic_irq_enable(irq);
}

void up_disable_irq(int irq)
{
  arm_gic_irq_disable(irq);
}

/***************************************************************************
 * Name: up_prioritize_irq
 *
 * Description:
 *   Set the priority of an IRQ.
 *
 *   Since this API is not supported on all architectures, it should be
 *   avoided in common implementations where possible.
 *
 ***************************************************************************/

int up_prioritize_irq(int irq, int priority)
{
  unsigned long base = GET_DIST_BASE(irq);

  DEBUGASSERT(irq >= 0 && irq < NR_IRQS &&
              priority >= 0 && priority <= 255);

  /* Ignore invalid interrupt IDs */

  if (irq >= 0 && irq < NR_IRQS)
    {
      /* PRIORITYR registers provide byte access */

      putreg8(priority & GIC_PRI_MASK, IPRIORITYR(base, irq));
      return OK;
    }

  return -EINVAL;
}

/***************************************************************************
 * Name: up_affinity_irq
 *
 * Description:
 *   Set an IRQ affinity by software.
 *
 ***************************************************************************/

void up_affinity_irq(int irq, cpu_set_t cpuset)
{
  if (GIC_IS_SPI(irq))
    {
      /* Only support interrupt routing mode 0,
       * so routing to the first cpu in cpuset.
       */

      arm_gic_write_irouter(ffs(cpuset) - 1, irq);
    }
}

/***************************************************************************
 * Name: up_trigger_irq
 *
 * Description:
 *   Perform a Software Generated Interrupt (SGI).  If CONFIG_SMP is
 *   selected, then the SGI is sent to all CPUs specified in the CPU set.
 *   That set may include the current CPU.
 *
 *   If CONFIG_SMP is not selected, the cpuset is ignored and SGI is sent
 *   only to the current CPU.
 *
 * Input Parameters
 *   irq    - The SGI interrupt ID (0-15)
 *   cpuset - The set of CPUs to receive the SGI
 *
 ***************************************************************************/

void up_trigger_irq(int irq, cpu_set_t cpuset)
{
  uint32_t  mask  = BIT(irq & (GIC_NUM_INTR_PER_REG - 1));
  uint32_t  idx   = irq / GIC_NUM_INTR_PER_REG;

  if (GIC_IS_SGI(irq))
    {
      arm_gic_raise_sgi(irq, cpuset);
    }
  else if (irq >= 0 && irq < NR_IRQS)
    {
      /* Write '1' to the corresponding bit in the distributor Interrupt
       * Set-Pending (ISPENDR)
       * GICD_ISPENDRn: Interrupt Set-Pending Registers
       */

      putreg32(mask, ISPENDR(GET_DIST_BASE(irq), idx));
    }
}

#ifdef CONFIG_ARCH_HIPRI_INTERRUPT
/***************************************************************************
 * Name: arm_decodefiq
 *
 * Description:
 *   This function is called from the FIQ vector handler in arm_vectors.S.
 *   At this point, the interrupt has been taken and the registers have
 *   been saved on the stack.  This function simply needs to determine the
 *   the irq number of the interrupt and then to call arm_dofiq to dispatch
 *   the interrupt.
 *
 *  Input Parameters:
 *   regs - A pointer to the register save area on the stack.
 ***************************************************************************/

uint32_t *arm_decodefiq(uint32_t *regs)
{
  int fiq;

  /* Read the Group0 interrupt acknowledge register
   * and get the interrupt ID
   */

  fiq = arm_gic_get_active_group0();

  /* Ignore spurions FIQs.  ICCIAR will report 1023 if there is no pending
   * interrupt.
   */

  DEBUGASSERT(fiq < NR_IRQS || fiq == 1023);
  if (fiq < NR_IRQS)
    {
      /* Dispatch the fiq interrupt */

      regs = arm_dofiq(fiq, regs);
    }

  /* Write to Group0 the end-of-interrupt register */

  arm_gic_eoi_group0(fiq);

  return regs;
}
#endif

/***************************************************************************
 * Name: arm_decodeirq
 *
 * Description:
 *   This function is called from the IRQ vector handler in arm_vectors.S.
 *   At this point, the interrupt has been taken and the registers have
 *   been saved on the stack.  This function simply needs to determine the
 *   the irq number of the interrupt and then to call arm_doirq to dispatch
 *   the interrupt.
 *
 *  Input Parameters:
 *   regs - A pointer to the register save area on the stack.
 ***************************************************************************/

uint32_t * arm_decodeirq(uint32_t * regs)
{
  int irq;

  /* Read the interrupt acknowledge register and get the interrupt ID */

  irq = arm_gic_get_active();

  /* Ignore spurions IRQs.  ICCIAR will report 1023 if there is no pending
   * interrupt.
   */

  DEBUGASSERT(irq < NR_IRQS || irq == 1023);
  if (irq < NR_IRQS)
    {
      /* Dispatch the interrupt */

      regs = arm_doirq(irq, regs);
    }

  /* Write to the end-of-interrupt register */

  arm_gic_eoi(irq);

  return regs;
}

static int gic_validate_dist_version(void)
{
  uint32_t typer;
  bool     has_rss;
  uint32_t reg = getreg32(GICD_PIDR2) & GICD_PIDR2_ARCH_MASK;
  int      spis;
  int      espis;

  if (reg == GICD_PIDR2_ARCH_GICV3)
    {
      sinfo("GICv3 version detect\n");
    }
  else if (reg == GICD_PIDR2_ARCH_GICV4)
    {
      sinfo("GICv4 version detect\n");
    }
  else
    {
      sinfo("No GIC version detect\n");
      return -ENODEV;
    }

  /* Find out how many interrupts are supported. */

  typer = getreg32(GICD_TYPER);
  spis  = MIN(GICD_TYPER_SPIS(typer), 1020U) - 32;
  espis = GICD_TYPER_ESPIS(typer);

  sinfo("GICD_TYPER = 0x%" PRIu32 "\n", typer);
  sinfo("%d SPIs implemented\n", spis);
  sinfo("%d Extended SPIs implemented\n", espis);

  has_rss = !!(typer & GICD_TYPER_RSS);
  sinfo("Distributor has %sRange Selector support\n", has_rss ? "" : "no ");

  if (typer & GICD_TYPER_MBIS)
    {
      sinfo("MBIs is present, But No support\n");
    }

  return 0;
}

static int gic_validate_redist_version(void)
{
  uint64_t      typer;
  unsigned int  ppi_nr;
  bool          has_vlpis       = true;
  bool          has_direct_lpi  = true;
  uint32_t      reg;
  unsigned long redist_base = gic_get_rdist();

  ppi_nr    = (~0U);
  reg       = getreg32(redist_base +
             GICR_PIDR2) & GICR_PIDR2_ARCH_MASK;
  if (reg != GICR_PIDR2_ARCH_GICV3 &&
             reg != GICR_PIDR2_ARCH_GICV4)
    {
      sinfo("No redistributor present 0x%lx\n", redist_base);
      return -ENODEV;
    }

  /* In AArch32, use 32bits accesses GICR_TYPER, in case that nuttx
   * run as vm, and hypervisor doesn't emulation strd.
   * Just like linux and zephyr.
   */

  typer           = getreg32(redist_base + GICR_TYPER);
  typer          |= (uint64_t)getreg32(redist_base + GICR_TYPER + 4) << 32;
  has_vlpis      &= !!(typer & GICR_TYPER_VLPIS);
  has_direct_lpi &= !!(typer & GICR_TYPER_DIRECTLPIS);
  ppi_nr          = MIN(GICR_TYPER_NR_PPIS(typer), ppi_nr);

  if (ppi_nr == (~0U))
    {
      ppi_nr = 0;
    }

  sinfo("GICR_TYPER = 0x%"PRIx64"\n", typer);
  sinfo("%d PPIs implemented\n", ppi_nr);
  sinfo("%sVLPI support, %sdirect LPI support\n", !has_vlpis ? "no " : "",
        !has_direct_lpi ? "no " : "");

  return 0;
}

static void arm_gic_init(void)
{
  uint8_t   cpu;
  int       err;

  cpu               = this_cpu();
  g_gic_rdists[cpu] = CONFIG_GICR_BASE +
                      up_cpu_index() * CONFIG_GICR_OFFSET;

  err = gic_validate_redist_version();
  if (err)
    {
      sinfo("no redistributor detected, giving up ret=%d\n", err);
      return;
    }

  gicv3_rdist_enable(gic_get_rdist());

  gicv3_cpuif_init();

#ifdef CONFIG_SMP
  up_enable_irq(GIC_SMP_CALL);
  up_enable_irq(GIC_SMP_SCHED);
#endif
}

int arm_gic_initialize(void)
{
  int err;

  err = gic_validate_dist_version();
  if (err)
    {
      sinfo("no distributor detected, giving up ret=%d\n", err);
      return err;
    }

  gicv3_dist_init();

  arm_gic_init();

  return 0;
}

#ifdef CONFIG_SMP
void arm_gic_secondary_init(void)
{
  arm_gic_init();
}

#endif
