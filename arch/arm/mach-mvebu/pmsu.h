#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 * Power Management Service Unit (PMSU) support for Armada 370/XP platforms.
 *
 * Copyright (C) 2012 Marvell
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef __MACH_MVEBU_PMSU_H
#define __MACH_MVEBU_PMSU_H

int armada_xp_boot_cpu(unsigned int cpu_id, void *phys_addr);
int mvebu_setup_boot_addr_wa(unsigned int crypto_eng_target,
                             unsigned int crypto_eng_attribute,
                             phys_addr_t resume_addr_reg);

void mvebu_v7_pmsu_idle_exit(void);
void armada_370_xp_cpu_resume(void);
#if defined(MY_ABC_HERE)
void armada_38x_mem_resume(void);
#endif /* MY_ABC_HERE */

int armada_370_xp_pmsu_idle_enter(unsigned long deepidle);
int armada_38x_do_cpu_suspend(unsigned long deepidle);
#if defined(MY_ABC_HERE)
void mvebu_v7_pmsu_disable_dfs_cpu(int hw_cpu);
#endif /* MY_ABC_HERE */
#endif	/* __MACH_370_XP_PMSU_H */
