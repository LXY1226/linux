#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __ASM_SPARSEMEM_H
#define __ASM_SPARSEMEM_H

#ifdef CONFIG_SPARSEMEM
#define MAX_PHYSMEM_BITS	48
#if defined(MY_ABC_HERE)
#define SECTION_SIZE_BITS	28
#elif defined(MY_DEF_HERE)
#define SECTION_SIZE_BITS	29
#else /* MY_ABC_HERE / MY_DEF_HERE */
#define SECTION_SIZE_BITS	30
#endif /* MY_ABC_HERE / MY_DEF_HERE */
#endif

#endif
