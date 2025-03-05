// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2025 Rockchip Electronics Co., Ltd.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/mm.h>
#include <linux/mmzone.h>
#include <linux/list.h>
#include <linux/page-flags.h>
#include <linux/pfn.h>
#include <linux/proc_fs.h>

/*
 * This kernel module creates a procfs entry that prints
 * detailed zone and page information, including the page's order if it
 * is part of the buddy allocator (PageBuddy is set). It is updated for
 * for_each_populated_zone(zone) iterates over all zones
 * without referencing pgdat. For each zone and order, it shows:
 *   - free_area info by migrate type
 *   - each page in the free list, including:
 *	   * page pointer
 *	   * PFN (page frame number)
 *	   * raw page flags
 *	   * physical address
 *	   * node ID
 *	   * whether it's a compound page
 *	   * the (buddy) order if PageBuddy is set
 *
 * Usage:
 *   cat /proc/rk_zoneinfo/zoneinfo > /data/data/1.txt
 */

static int rk_zoneinfo_show(struct seq_file *m, void *v)
{
	struct zone *zone;
	int order, type;
	unsigned long flags;

	seq_puts(m, "==== ZONE & PAGE INFO (with page order) ====\n");

	for_each_populated_zone(zone) {
		seq_printf(m, "Zone: %s\n", zone->name);
		seq_printf(m, "  managed_pages:  %lu\n",
			   atomic_long_read(&zone->managed_pages));
		seq_printf(m, "  spanned_pages:  %lu\n", zone->spanned_pages);
		seq_printf(m, "  present_pages:  %lu\n\n", zone->present_pages);

		spin_lock_irqsave(&zone->lock, flags);

		/* For each order in this zone, we examine the free lists */
		for (order = 0; order < MAX_ORDER; order++) {
			seq_printf(m, "\tOrder: %d | nr_free: %lu\n",
				   order, zone->free_area[order].nr_free);

			for (type = 0; type < MIGRATE_TYPES; type++) {
				struct list_head *head =
					&zone->free_area[order].free_list[type];
				struct page *page;

				if (list_empty(head)) {
					seq_printf(m, "\t\tMigrate type %d => (empty)\n", type);
					continue;
				}

				list_for_each_entry(page, head, lru) {
					unsigned long pfn = page_to_pfn(page);
					unsigned int buddy_order = 0;

					if (PageBuddy(page))
						buddy_order = page_private(page);

					seq_printf(
						m,
						"\t\tMigrate type %d => page: %p | PFN: %08lu | phys_addr: 0x%010lx | flags: 0x%lx | compound: %d | node: %d | zone: %s | buddy_order: %u\n",
						type,
						page,
						pfn,
						(unsigned long)page_to_phys(page),
						page->flags,
						PageCompound(page),
						page_to_nid(page),
						zone->name,
						buddy_order
					);
				}
			}
			seq_puts(m, "\n");
		}
		spin_unlock_irqrestore(&zone->lock, flags);
		seq_puts(m, "----------------------------------------\n");
	}

	return 0;
}

static int rk_zoneinfo_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, rk_zoneinfo_show, NULL);
}

static const struct proc_ops rk_zoneinfo_proc_fops = {
	.proc_open	= rk_zoneinfo_proc_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
};

static int __init rk_zoneinfo_procfs_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_mkdir("rk_zoneinfo", NULL);
	if (!entry)
		return -ENOENT;

	if (!proc_create("zoneinfo", 0444, entry, &rk_zoneinfo_proc_fops))
		goto err;

	pr_info("rk_zoneinfo_procfs module loaded.\n");

	return 0;
err:
	remove_proc_subtree("rk_zoneinfo", NULL);

	return -ENOENT;
}

module_init(rk_zoneinfo_procfs_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("xxm@rock-chips.com");
MODULE_DESCRIPTION("iterating zones to show zone/page info and page order if buddy.");
