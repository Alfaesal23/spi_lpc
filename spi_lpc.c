// SPDX-License-Identifier: GPL-2.0
/*
 * SPI LPC flash platform security driver
 *
 * Copyright 2020 (c) Richard Hughes (rhughes@redhat.com)
 *
 * This file is licensed under  the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/module.h>
#include <linux/security.h>
#include <linux/pci.h>

/* LPC bridge PCI config space registers */
#define BIOS_CNTL_REG				0xDC
#define BIOS_CNTL_WRITE_ENABLE_MASK		0x01
#define BIOS_CNTL_LOCK_ENABLE_MASK		0x02
#define BIOS_CNTL_WP_DISABLE_MASK		0x20

static const struct pci_device_id pci_tbl[] = {
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x02A4)}, /* Comet Lake SPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x34A4)}, /* Ice Lake-LP SPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x9C66)}, /* 8 Series SPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x9CE6)}, /* Wildcat Point-LP GSPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x9D2A)}, /* Sunrise Point-LP/SPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x9D4E)}, /* Sunrise Point LPC/eSPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x9DA4)}, /* Cannon Point-LP SPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA140)}, /* Sunrise Point-H LPC */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA141)}, /* Sunrise Point-H LPC */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA142)}, /* Sunrise Point-H LPC */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA143)}, /* H110 LPC/eSPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA144)}, /* H170 LPC/eSPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA145)}, /* Z170 LPC/eSPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA146)}, /* Q170 LPC/eSPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA147)}, /* Q150 LPC/eSPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA148)}, /* B150 LPC/eSPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA149)}, /* C236 LPC/eSPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA14A)}, /* C232 LPC/eSPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA14B)}, /* Sunrise Point-H LPC */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA14C)}, /* Sunrise Point-H LPC */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA14D)}, /* QM170 LPC/eSPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA14E)}, /* HM170 LPC/eSPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA14F)}, /* Sunrise Point-H LPC */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA150)}, /* CM236 LPC/eSPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA151)}, /* Sunrise Point-H LPC */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA152)}, /* HM175 LPC/eSPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA153)}, /* QM175 LPC/eSPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA154)}, /* CM238 LPC/eSPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA155)}, /* Sunrise Point-H LPC */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA1C1)}, /* C621 LPC/eSPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA1C2)}, /* C622 LPC/eSPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA1C3)}, /* C624 LPC/eSPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA1C4)}, /* C625 LPC/eSPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA1C5)}, /* C626 LPC/eSPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA1C6)}, /* C627 LPC/eSPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA1C7)}, /* C628 LPC/eSPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA304)}, /* H370 LPC/eSPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA305)}, /* Z390 LPC/eSPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA306)}, /* Q370 LPC/eSPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA30C)}, /* QM370 LPC/eSPI */
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA324)}, /* Cannon Lake PCH SPI */
	{0, }
};
MODULE_DEVICE_TABLE(pci, pci_tbl);

struct dentry *spi_dir;
struct dentry *spi_bioswe;
struct dentry *spi_ble;
struct dentry *spi_smm_bwp;
struct pci_dev *dev;
const u8 bios_cntl_off = BIOS_CNTL_REG;

static ssize_t bioswe_read(struct file *filp, char __user *buf,
			   size_t count, loff_t *ppos)
{
	char tmp[2];
	u8 bios_cntl_val;

	pci_read_config_byte(dev, bios_cntl_off, &bios_cntl_val);
	sprintf(tmp, "%d\n",
		bios_cntl_val & BIOS_CNTL_WRITE_ENABLE_MASK ? 1 : 0);
	return simple_read_from_buffer(buf, count, ppos, tmp, sizeof(tmp));
}

static const struct file_operations spi_bioswe_ops = {
	.read  = bioswe_read,
};

static ssize_t ble_read(struct file *filp, char __user *buf,
			size_t count, loff_t *ppos)
{
	char tmp[2];
	u8 bios_cntl_val;

	pci_read_config_byte(dev, bios_cntl_off, &bios_cntl_val);
	sprintf(tmp, "%d\n",
		bios_cntl_val & BIOS_CNTL_LOCK_ENABLE_MASK ? 1 : 0);
	return simple_read_from_buffer(buf, count, ppos, tmp, sizeof(tmp));
}

static const struct file_operations spi_ble_ops = {
	.read  = ble_read,
};

static ssize_t smm_bwp_read(struct file *filp, char __user *buf,
			    size_t count, loff_t *ppos)
{
	char tmp[2];
	u8 bios_cntl_val;

	pci_read_config_byte(dev, bios_cntl_off, &bios_cntl_val);
	sprintf(tmp, "%d\n",
		bios_cntl_val & BIOS_CNTL_WP_DISABLE_MASK ? 1 : 0);
	return simple_read_from_buffer(buf, count, ppos, tmp, sizeof(tmp));
}

static const struct file_operations spi_smm_bwp_ops = {
	.read  = smm_bwp_read,
};

static int __init mod_init(void)
{
	int i;

	/* Find SPI Controller */
	for (i = 0; !dev && pci_tbl[i].vendor; ++i)
		dev = pci_get_device(pci_tbl[i].vendor,
				     pci_tbl[i].device, NULL);
	if (!dev)
		return -ENODEV;

	spi_dir = securityfs_create_dir("spi", NULL);
	if (IS_ERR(spi_dir))
		return -1;

	spi_bioswe =
	    securityfs_create_file("bioswe",
				   0600, spi_dir, NULL,
				   &spi_bioswe_ops);
	if (IS_ERR(spi_bioswe))
		goto out;
	spi_ble =
	    securityfs_create_file("ble",
				   0600, spi_dir, NULL,
				   &spi_ble_ops);
	if (IS_ERR(spi_ble))
		goto out;
	spi_smm_bwp =
	    securityfs_create_file("smm_bwp",
				   0600, spi_dir, NULL,
				   &spi_smm_bwp_ops);
	if (IS_ERR(spi_smm_bwp))
		goto out;

	return 0;
out:
	securityfs_remove(spi_bioswe);
	securityfs_remove(spi_ble);
	securityfs_remove(spi_smm_bwp);
	securityfs_remove(spi_dir);
	return -1;
}

static void __exit mod_exit(void)
{
	securityfs_remove(spi_bioswe);
	securityfs_remove(spi_ble);
	securityfs_remove(spi_smm_bwp);
	securityfs_remove(spi_dir);
}

module_init(mod_init);
module_exit(mod_exit);

MODULE_DESCRIPTION("SPI LPC flash platform security driver");
MODULE_AUTHOR("Richard Hughes <rhughes@redhat.com>");
MODULE_LICENSE("GPL");