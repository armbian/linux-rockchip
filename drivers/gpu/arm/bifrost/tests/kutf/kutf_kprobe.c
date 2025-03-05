// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
/*
 *
 * (C) COPYRIGHT 2023-2024 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 */

/* Kernel probe functionality.
 */

#ifdef CONFIG_KPROBES

#include <linux/list.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/kprobes.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <kutf/kutf_kprobe.h>

#define KUTF_KP_REG_MIN_ARGS 3
#define KUTF_KP_UNREG_MIN_ARGS 2
#define KUTF_KP_FUNC_NAME_ARG 0
#define KUTF_KP_FUNC_ENTRY_EXIT_ARG 1
#define KUTF_KP_FUNC_HANDLER_ARG 2

#define KUTF_KP_WRITE_BUFSIZE 4096

/* Stores address to kernel function 'kallsyms_lookup_name'
 * as 'kallsyms_lookup_name' is no longer exported from 5.7 kernel.
 */
typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);
kallsyms_lookup_name_t kutf_ksym_lookup_name;

static ssize_t kutf_kp_reg_debugfs_write(struct file *file, const char __user *user_buf,
					 size_t count, loff_t *ppos);

static ssize_t kutf_kp_unreg_debugfs_write(struct file *file, const char __user *user_buf,
					   size_t count, loff_t *ppos);

static LIST_HEAD(kp_list);
static DEFINE_MUTEX(kp_list_lock);

/**
 * struct kutf_kp_data - Structure which holds data per kprobe instance
 * @kretp:      reference to kernel ret probe
 * @entry:      true if this probe is for function entry.Otherwise it is exit.
 * @argc:       Number of arguments to be passed to probe handler
 * @argv:       arguments passed to probe handler
 * @kp_handler:	Actual handler which is called when probe is triggered
 * @list:       node for adding to kp_list
 */
struct kutf_kp_data {
	struct kretprobe kretp;
	bool entry;
	int argc;
	char **argv;
	kutf_kp_handler kp_handler;
	struct list_head list;
};

const struct file_operations kutf_kp_reg_debugfs_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.write = kutf_kp_reg_debugfs_write,
};

const struct file_operations kutf_kp_unreg_debugfs_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.write = kutf_kp_unreg_debugfs_write,
};

struct kprobe kutf_kallsym_kp = { .symbol_name = "kallsyms_lookup_name" };

void kutf_kp_delay_handler(int argc, char **argv)
{
	long delay;

	if ((!argv) || (!argv[0]))
		return;

	if (kstrtol(argv[0], 0, &delay))
		return;

	mdelay(delay);
}

void kutf_kp_sample_kernel_function(void)
{
	pr_debug("%s called\n", __func__);
}
EXPORT_SYMBOL(kutf_kp_sample_kernel_function);

void kutf_kp_sample_handler(int argc, char **argv)
{
	int i = 0;

	for (; i < argc; i++)
		pr_info("%s %s\n", __func__, argv[i]);
}
EXPORT_SYMBOL(kutf_kp_sample_handler);

static int kutf_call_kp_handler(struct kretprobe *p)
{
	struct kutf_kp_data *kp_p;
	kutf_kp_handler kp_handler;

	kp_p = (struct kutf_kp_data *)p;
	kp_handler = kp_p->kp_handler;

	if (kp_handler) {
		/* Arguments to registered handler starts after
		 * KUTF_KP_REG_MIN_ARGS.
		 */
		(*kp_handler)((kp_p->argc) - KUTF_KP_REG_MIN_ARGS,
			      &(kp_p->argv[KUTF_KP_REG_MIN_ARGS]));
	}
	return 0;
}

static int kutf_kretp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
#if (KERNEL_VERSION(5, 11, 0) <= LINUX_VERSION_CODE)
	return kutf_call_kp_handler(ri->rph->rp);
#else
	return kutf_call_kp_handler(ri->rp);
#endif
}

static kutf_kp_handler kutf_get_kp_handler(char **argv)
{
	if ((NULL == argv) || (NULL == kutf_ksym_lookup_name))
		return NULL;

	return (kutf_kp_handler)((*kutf_ksym_lookup_name)(argv[KUTF_KP_FUNC_HANDLER_ARG]));
}

static ssize_t kutf_kp_reg_debugfs_write(struct file *file, const char __user *user_buf,
					 size_t count, loff_t *ppos)
{
	int argc = 0;
	int ret = count;
	char **argv;
	char *kbuf;
	char *func_name;
	struct kutf_kp_data *kp_p;
	struct kutf_kp_data *kp_iter;

	if (count >= KUTF_KP_WRITE_BUFSIZE)
		return -EINVAL;

	kbuf = memdup_user_nul(user_buf, count);
	if (IS_ERR(kbuf))
		return -ENOMEM;

	argv = argv_split(GFP_KERNEL, kbuf, &argc);
	if (!argv) {
		ret = -ENOMEM;
		goto out;
	}
	if (argc < KUTF_KP_REG_MIN_ARGS) {
		pr_debug("Insufficient args in reg kprobe:%d\n", argc);
		ret = -EINVAL;
		goto argv_out;
	}

	kp_p = kzalloc(sizeof(struct kutf_kp_data), GFP_KERNEL);
	if (!kp_p)
		goto argv_out;

	if (!(strcmp(argv[KUTF_KP_FUNC_ENTRY_EXIT_ARG], "entry"))) {
		kp_p->kretp.entry_handler = kutf_kretp_handler;
		kp_p->entry = 1;
	} else if (!(strcmp(argv[KUTF_KP_FUNC_ENTRY_EXIT_ARG], "exit"))) {
		kp_p->kretp.handler = kutf_kretp_handler;
	} else {
		pr_err("Invalid arg:%s passed in reg kprobe\n", argv[KUTF_KP_FUNC_ENTRY_EXIT_ARG]);
		ret = -EINVAL;
		kfree(kp_p);
		goto argv_out;
	}

	func_name = argv[KUTF_KP_FUNC_NAME_ARG];

	mutex_lock(&kp_list_lock);
	list_for_each_entry(kp_iter, &kp_list, list) {
		if ((kp_iter->entry == kp_p->entry) &&
		    (!(strcmp(kp_iter->kretp.kp.symbol_name, func_name)))) {
			ret = -EEXIST;
			kfree(kp_p);
			mutex_unlock(&kp_list_lock);
			goto argv_out;
		}
	}

	kp_p->kretp.kp.symbol_name = func_name;
	kp_p->kp_handler = kutf_get_kp_handler(argv);
	if (!(kp_p->kp_handler)) {
		pr_debug("cannot find addr for handler:%s\n", argv[KUTF_KP_FUNC_HANDLER_ARG]);
		ret = -EINVAL;
		kfree(kp_p);
		mutex_unlock(&kp_list_lock);
		goto argv_out;
	}
	kp_p->argc = argc;
	kp_p->argv = argv;
	ret = register_kretprobe(&kp_p->kretp);
	if (ret) {
		ret = -EINVAL;
		kfree(kp_p);
		mutex_unlock(&kp_list_lock);
		goto argv_out;
	}
	INIT_LIST_HEAD(&kp_p->list);
	list_add(&kp_p->list, &kp_list);

	mutex_unlock(&kp_list_lock);

	ret = count;
	goto out;

argv_out:
	argv_free(argv);

out:
	kfree(kbuf);

	return ret;
}

static ssize_t kutf_kp_unreg_debugfs_write(struct file *file, const char __user *user_buf,
					   size_t count, loff_t *ppos)
{
	int argc = 0;
	int ret = -EINVAL;
	char **argv = NULL;
	char *kbuf;
	char *func_name;
	struct kutf_kp_data *kp_iter;
	bool entry;

	if (count >= KUTF_KP_WRITE_BUFSIZE)
		return -EINVAL;

	kbuf = memdup_user_nul(user_buf, count);
	if (IS_ERR(kbuf))
		return -ENOMEM;

	argv = argv_split(GFP_KERNEL, kbuf, &argc);
	if (!argv) {
		ret = -ENOMEM;
		goto out;
	}
	if (argc < KUTF_KP_UNREG_MIN_ARGS) {
		pr_debug("Insufficient args in unreg kprobe:%d\n", argc);
		ret = -EINVAL;
		goto out;
	}
	if (!(strcmp(argv[KUTF_KP_FUNC_ENTRY_EXIT_ARG], "entry")))
		entry = 1;
	else if (!(strcmp(argv[KUTF_KP_FUNC_ENTRY_EXIT_ARG], "exit")))
		entry = 0;
	else {
		pr_err("Invalid arg:%s passed in unreg kprobe\n",
		       argv[KUTF_KP_FUNC_ENTRY_EXIT_ARG]);
		ret = -EINVAL;
		goto out;
	}
	func_name = argv[KUTF_KP_FUNC_NAME_ARG];

	mutex_lock(&kp_list_lock);
	list_for_each_entry(kp_iter, &kp_list, list) {
		if ((kp_iter->entry == entry) &&
		    (!(strcmp(func_name, kp_iter->kretp.kp.symbol_name)))) {
			unregister_kretprobe(&kp_iter->kretp);
			argv_free(kp_iter->argv);
			list_del(&kp_iter->list);
			kfree(kp_iter);
			ret = count;
			break;
		}
	}
	mutex_unlock(&kp_list_lock);
out:
	argv_free(argv);
	kfree(kbuf);

	return ret;
}

int __init kutf_kprobe_init(struct dentry *base_dir)
{
	struct dentry *kutf_kp_reg_debugfs_file;
	struct dentry *kutf_kp_unreg_debugfs_file;

	if (!(register_kprobe(&kutf_kallsym_kp))) {
		/* After kernel 5.7, 'kallsyms_lookup_name' is no longer
		 * exported. So we need this workaround to get the
		 * addr of 'kallsyms_lookup_name'. This will be used later
		 * in kprobe handler function to call the registered
		 * handler for a probe from the name passed from userspace.
		 */
		kutf_ksym_lookup_name = (kallsyms_lookup_name_t)kutf_kallsym_kp.addr;
		unregister_kprobe(&kutf_kallsym_kp);
		kutf_kp_reg_debugfs_file = debugfs_create_file("register_kprobe", 0200, base_dir,
							       NULL, &kutf_kp_reg_debugfs_fops);
		if (IS_ERR_OR_NULL(kutf_kp_reg_debugfs_file))
			pr_err("Failed to create kprobe reg debugfs file");

		kutf_kp_unreg_debugfs_file = debugfs_create_file(
			"unregister_kprobe", 0200, base_dir, NULL, &kutf_kp_unreg_debugfs_fops);
		if (IS_ERR_OR_NULL(kutf_kp_unreg_debugfs_file)) {
			pr_err("Failed to create kprobe unreg debugfs file");
			debugfs_remove(kutf_kp_reg_debugfs_file);
		}
	} else
		pr_info("kallsyms_lookup_name addr not available\n");

	return 0;
}

void kutf_kprobe_exit(void)
{
	struct kutf_kp_data *kp_iter;
	struct kutf_kp_data *n;

	mutex_lock(&kp_list_lock);

	list_for_each_entry_safe(kp_iter, n, &kp_list, list) {
		unregister_kretprobe(&kp_iter->kretp);
		argv_free(kp_iter->argv);
		list_del(&kp_iter->list);
		kfree(kp_iter);
	}

	mutex_unlock(&kp_list_lock);
}

#endif
