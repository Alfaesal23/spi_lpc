// SPDX-License-Identifier: GPL-2.0
/*
 * SPI LPC flash platform security driver
 *
 * Copyright 2020 (c) Richard Hughes (richard@hughsie.com)
 *                    Daniel Gutson (daniel.gutson@eclypsium.com)
 *
 * This file is licensed under  the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/module.h>
#include <linux/security.h>
#include <linux/pci.h>
#include "data_access.h"
#include "low_level_access.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define SIZE_BYTE sizeof(u8)
#define SIZE_WORD sizeof(u16)
#define SIZE_DWORD sizeof(u32)
#define SIZE_QWORD sizeof(u64)
#define BYTE_MASK 0xFFu
#define WORD_MASK 0xFFFFu
#define DWORD_MASK 0xFFFFFFFFu
#define HIGH_DWORD_MASK (0xFFFFFFFFull << (SIZE_DWORD * 8u))

enum PCH_Arch pch_arch;
enum CPU_Arch cpu_arch;

typedef int Read_BC_Flag_Fn(struct BC *bc, u64 *value);

static int get_pci_vid_did(u8 bus, u8 dev, u8 fun, u16 *vid, u16 *did)
{
	u32 vid_did;
	int ret = pci_read_dword(&vid_did, bus, dev, fun, 0);
	if (ret == 0) {
		*vid = vid_did & WORD_MASK;
		*did = (vid_did >> (SIZE_WORD * 8)) & WORD_MASK;
	}
	return ret;
}

static int get_pch_arch(enum PCH_Arch *pch_arch)
{
	u16 pch_vid;
	u16 pch_did;
	int ret = get_pci_vid_did(0, 0x1f, 0, &pch_vid, &pch_did);
	if (ret != 0)
		return ret;

	pr_info("PCH VID: %x - DID: %x\n", pch_vid, pch_did);
	ret = viddid2pch_arch(pch_vid, pch_did, pch_arch);

	return ret;
}

static int get_cpu_arch(enum CPU_Arch *cpu_arch)
{
	u16 cpu_vid;
	u16 cpu_did;
	int ret = get_pci_vid_did(0, 0, 0, &cpu_vid, &cpu_did);
	if (ret != 0)
		return ret;

	pr_info("CPU VID: %x - DID: %x\n", cpu_vid, cpu_did);
	ret = viddid2cpu_arch(cpu_vid, cpu_did, cpu_arch);

	return ret;
}

static int get_pch_cpu(enum PCH_Arch *pch_arch, enum CPU_Arch *cpu_arch)
{
	const int cpu_res = get_cpu_arch(cpu_arch);
	const int pch_res = get_pch_arch(pch_arch);

	return cpu_res != 0 && pch_res != 0 ? -1 : 0;
}

struct dentry *spi_dir;
struct dentry *spi_bioswe;
struct dentry *spi_ble;
struct dentry *spi_smm_bwp;

/* Buffer to return: always 3 because of the following chars:
       value \n \0
*/
#define BUFFER_SIZE 3

static ssize_t bc_flag_read(struct file *filp, char __user *buf, size_t count,
			    loff_t *ppos)
{
	char tmp[BUFFER_SIZE];
	ssize_t ret;
	u64 value = 0;
	struct BC bc;

	if (*ppos == BUFFER_SIZE)
		return 0; // nothing else to read

	if (file_inode(filp)->i_private == NULL)
		return -1;

	ret = read_BC(pch_arch, cpu_arch, &bc);

	if (ret == 0)
		ret = ((Read_BC_Flag_Fn *)file_inode(filp)->i_private)(&bc,
								       &value);

	if (ret != 0)
		return ret;

	sprintf(tmp, "%d\n", (int)value & 1);
	ret = simple_read_from_buffer(buf, count, ppos, tmp, sizeof(tmp));

	return ret;
}

static const struct file_operations bc_flags_ops = {
	.read = bc_flag_read,
};

static int __init mod_init(void)
{
	if (get_pch_cpu(&pch_arch, &cpu_arch) != 0) {
		pr_err("Couldn't detect PCH or CPU\n");
		return -1;
	}

	spi_dir = securityfs_create_dir("firmware", NULL);
	if (IS_ERR(spi_dir)) {
		pr_err("Couldn't create firmware sysfs dir\n");
		return -1;
	} else {
		pr_info("firmware securityfs dir creation successful\n");
	}

	spi_bioswe = securityfs_create_file("bioswe", 0600, spi_dir,
					    &read_BC_BIOSWE, &bc_flags_ops);
	if (IS_ERR(spi_bioswe)) {
		pr_err("Error creating sysfs file bioswe\n");
		goto out_bioswe;
	}

	spi_ble = securityfs_create_file("ble", 0600, spi_dir, &read_BC_BLE,
					 &bc_flags_ops);
	if (IS_ERR(spi_ble)) {
		pr_err("Error creating sysfs file ble\n");
		goto out_ble;
	}

	spi_smm_bwp = securityfs_create_file("smm_bwp", 0600, spi_dir,
					     &read_BC_SMM_BWP, &bc_flags_ops);
	if (IS_ERR(spi_smm_bwp)) {
		pr_err("Error creating sysfs file smm_bwp\n");
		goto out_smm_bwp;
	}

	return 0;

out_smm_bwp:
	securityfs_remove(spi_smm_bwp);
out_ble:
	securityfs_remove(spi_ble);
out_bioswe:
	securityfs_remove(spi_bioswe);
	securityfs_remove(spi_dir);
	return -1;
}

static void __exit mod_exit(void)
{
	securityfs_remove(spi_smm_bwp);
	securityfs_remove(spi_ble);
	securityfs_remove(spi_bioswe);
	securityfs_remove(spi_dir);
}

module_init(mod_init);
module_exit(mod_exit);

MODULE_DESCRIPTION("SPI LPC flash platform security driver");
MODULE_AUTHOR("Richard Hughes <richard@hughsie.com>");
MODULE_AUTHOR("Daniel Gutson <daniel.gutson@eclypsium.com>");
MODULE_LICENSE("GPL");