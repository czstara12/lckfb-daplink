/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 */
/*
  This file is part of UFFS, the Ultra-low-cost Flash File System.
  
  Copyright (C) 2005-2009 Ricky Zheng <ricky_gz_zheng@yahoo.co.nz>

  UFFS is free software; you can redistribute it and/or modify it under
  the GNU Library General Public License as published by the Free Software 
  Foundation; either version 2 of the License, or (at your option) any
  later version.

  UFFS is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
  or GNU Library General Public License, as applicable, for more details.
 
  You should have received a copy of the GNU General Public License
  and GNU Library General Public License along with UFFS; if not, write
  to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA  02110-1301, USA.

  As a special exception, if other files instantiate templates or use
  macros or inline functions from this file, or you compile this file
  and link it with other works to produce a work based on this file,
  this file does not by itself cause the resulting work to be covered
  by the GNU General Public License. However the source code for this
  file must still be made available in accordance with section (3) of
  the GNU General Public License v2.
 
  This exception does not invalidate any other reasons why a work based
  on this file might be covered by the GNU General Public License.
*/

/**
 * \file uffs_os_posix.c
 * \brief Emulation on POSIX host. This is just a dumb implementation, does not really create semaphores.
 * \author Ricky Zheng
 */

#include "uffs_config.h"
#include "uffs/uffs_os.h"
#include "uffs/uffs_public.h"
//#include <time.h>

#define PFX "os  : "
#define UFFS_CCM_HEAP_SIZE (48U * 1024U)
#define UFFS_CCM_BLOCK_HEAD_SIZE RT_ALIGN(sizeof(struct uffs_ccm_block), RT_ALIGN_SIZE)

struct uffs_ccm_block
{
	rt_size_t size;
	rt_uint8_t used;
	struct uffs_ccm_block *next;
};

#if defined(__ICCARM__)
#pragma location = ".ccm.uffs"
static rt_uint8_t uffs_ccm_heap[UFFS_CCM_HEAP_SIZE];
#else
static rt_uint8_t uffs_ccm_heap[UFFS_CCM_HEAP_SIZE] __attribute__((section(".ccm.uffs"), aligned(8)));
#endif

static struct uffs_ccm_block *uffs_ccm_head = RT_NULL;

static void uffs_ccm_init(void)
{
	if (uffs_ccm_head != RT_NULL)
	{
		return;
	}

	uffs_ccm_head = (struct uffs_ccm_block *)uffs_ccm_heap;
	uffs_ccm_head->size = sizeof(uffs_ccm_heap) - UFFS_CCM_BLOCK_HEAD_SIZE;
	uffs_ccm_head->used = 0;
	uffs_ccm_head->next = RT_NULL;
}

static int uffs_ccm_contains(const void *ptr)
{
	rt_ubase_t addr = (rt_ubase_t)ptr;
	rt_ubase_t start = (rt_ubase_t)uffs_ccm_heap;
	rt_ubase_t end = start + sizeof(uffs_ccm_heap);

	return addr >= start && addr < end;
}

static void uffs_ccm_coalesce(void)
{
	struct uffs_ccm_block *block = uffs_ccm_head;

	while (block != RT_NULL && block->next != RT_NULL)
	{
		if (!block->used && !block->next->used)
		{
			block->size += UFFS_CCM_BLOCK_HEAD_SIZE + block->next->size;
			block->next = block->next->next;
			continue;
		}

		block = block->next;
	}
}

static void *uffs_ccm_malloc(rt_size_t size)
{
	struct uffs_ccm_block *block;

	if (size == 0)
	{
		return RT_NULL;
	}

	uffs_ccm_init();
	size = RT_ALIGN(size, RT_ALIGN_SIZE);

	for (block = uffs_ccm_head; block != RT_NULL; block = block->next)
	{
		if (!block->used && block->size >= size)
		{
			if (block->size >= size + UFFS_CCM_BLOCK_HEAD_SIZE + RT_ALIGN_SIZE)
			{
				struct uffs_ccm_block *next;

				next = (struct uffs_ccm_block *)((rt_uint8_t *)block +
				       UFFS_CCM_BLOCK_HEAD_SIZE + size);
				next->size = block->size - size - UFFS_CCM_BLOCK_HEAD_SIZE;
				next->used = 0;
				next->next = block->next;

				block->size = size;
				block->next = next;
			}

			block->used = 1;
			return (rt_uint8_t *)block + UFFS_CCM_BLOCK_HEAD_SIZE;
		}
	}

	return RT_NULL;
}

static URET uffs_ccm_free(void *ptr)
{
	struct uffs_ccm_block *block;

	if (ptr == RT_NULL)
	{
		return U_SUCC;
	}

	block = (struct uffs_ccm_block *)((rt_uint8_t *)ptr - UFFS_CCM_BLOCK_HEAD_SIZE);
	block->used = 0;
	uffs_ccm_coalesce();

	return U_SUCC;
}

int uffs_SemCreate(OSSEM *sem)
{
	static int count = 0;
	char name [RT_NAME_MAX+1];
	struct rt_mutex *mutex = RT_NULL;

	rt_snprintf(name, sizeof(name), "usem%d", count++);
	mutex = rt_mutex_create(name, RT_IPC_FLAG_FIFO);
	if (mutex != RT_NULL)
	{
		*sem = (OSSEM *)mutex;
		return 0;
	}
	uffs_Perror(UFFS_MSG_SERIOUS, "can't get a semphore");
	return -1;
}

int uffs_SemWait(OSSEM sem)
{
	return rt_mutex_take((struct rt_mutex *)sem, RT_WAITING_FOREVER);
}

int uffs_SemSignal(OSSEM sem)
{
	return rt_mutex_release((struct rt_mutex *)sem);
}

int uffs_SemDelete(OSSEM *sem)
{
	int ret = -1;
	
	if (sem) {
		ret = rt_mutex_delete((struct rt_mutex *)(*sem));
		if (ret == RT_EOK) {
			*sem = 0;
		}			
	}
	return ret;
}

int uffs_OSGetTaskId(void)
{
	//TODO: ... return current task ID ...
	return 0;
}

unsigned int uffs_GetCurDateTime(void)
{
	// FIXME: return system time, please modify this for your platform ! 
	//			or just return 0 if you don't care about file time.
#if 0
	time_t tvalue;

	tvalue = time(NULL);
	
	return (unsigned int)tvalue;
#endif
	return 0;
}

#if CONFIG_USE_SYSTEM_MEMORY_ALLOCATOR > 0
static void * sys_malloc(struct uffs_DeviceSt *dev, unsigned int size)
{
	void *ptr;
	rt_base_t level;

	dev = dev;
	level = rt_hw_interrupt_disable();
	ptr = uffs_ccm_malloc(size);
	rt_hw_interrupt_enable(level);
	if (ptr != RT_NULL)
	{
		uffs_Perror(UFFS_MSG_NORMAL, "CCM memory alloc %d bytes", size);
		return ptr;
	}

	uffs_Perror(UFFS_MSG_NORMAL, "CCM memory alloc %d bytes failed, fallback system", size);
	return rt_malloc(size);
}

static URET sys_free(struct uffs_DeviceSt *dev, void *p)
{
	rt_base_t level;

	dev = dev;
	if (uffs_ccm_contains(p))
	{
		URET ret;

		level = rt_hw_interrupt_disable();
		ret = uffs_ccm_free(p);
		rt_hw_interrupt_enable(level);
		return ret;
	}

	rt_free(p);
	return U_SUCC;
}

void uffs_MemSetupSystemAllocator(uffs_MemAllocator *allocator)
{
	allocator->malloc = sys_malloc;
	allocator->free = sys_free;
}
#endif

#if !defined(RT_THREAD)
/* debug message output throught 'printf' */
static void output_dbg_msg(const char *msg);
static struct uffs_DebugMsgOutputSt m_dbg_ops = {
	output_dbg_msg,
	NULL,
};

static void output_dbg_msg(const char *msg)
{
	rt_kprintf("%s", msg);
}

void uffs_SetupDebugOutput(void)
{
	uffs_InitDebugMessageOutput(&m_dbg_ops, UFFS_MSG_NOISY);
}
#else

void uffs_SetupDebugOutput(void)
{
}
#endif
