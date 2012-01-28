/* drivers/misc/custom_voltage.c
 *
 * Copyright 2012  Ezekeel
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/cpufreq.h>
#include <linux/opp.h>
#include <linux/slab.h>

#include "../../arch/arm/mach-omap2/voltage.h"
#include "../../arch/arm/mach-omap2/smartreflex.h"

#define CUSTOMVOLTAGE_VERSION 1

static int num_freqs;

static unsigned long * voltages = NULL;

static struct cpufreq_frequency_table * frequency_table = NULL;

static struct mutex * frequency_mutex = NULL;

static struct device * mpu_device = NULL;

static struct voltagedomain * mpu_voltdm = NULL;

struct opp {
    struct list_head node;
    
    bool available;
    unsigned long rate;
    unsigned long u_volt;
    
    struct device_opp *dev_opp;
};

void customvoltage_register_freqtable(struct cpufreq_frequency_table * freq_table)
{
    frequency_table = freq_table;

    return;
}
EXPORT_SYMBOL(customvoltage_register_freqtable);

void customvoltage_register_freqmutex(struct mutex * freq_mutex)
{
    frequency_mutex = freq_mutex;

    return;
}
EXPORT_SYMBOL(customvoltage_register_freqmutex);

void customvoltage_register_oppdevice(struct device * dev, char * dev_name)
{
    if (!strcmp(dev_name, "mpu"))
	{
	    if (!mpu_device)
		mpu_device = dev;
	}

    return;
}
EXPORT_SYMBOL(customvoltage_register_oppdevice);

void customvoltage_init(void)
{
    int i;

    struct opp * temp_opp;

    mpu_voltdm = voltdm_lookup("mpu");

    num_freqs = 0;

    while (frequency_table[num_freqs].frequency != CPUFREQ_TABLE_END)
	num_freqs++;

    voltages = kzalloc(num_freqs * sizeof(unsigned long), GFP_KERNEL);

    for (i = 0; i < num_freqs; i++)
	{
	    temp_opp = opp_find_freq_exact(mpu_device, frequency_table[i].frequency * 1000, true);
	    voltages[i] = temp_opp->u_volt;
	}

    return;
}
EXPORT_SYMBOL(customvoltage_init);

static void customvoltage_voltages_update(void)
{
    int i;

    struct opp * temp_opp;

    mutex_lock(frequency_mutex);

    for (i = 0; i < num_freqs; i++)
	{
	    temp_opp = opp_find_freq_exact(mpu_device, frequency_table[i].frequency * 1000, true);
	    temp_opp->u_volt = voltages[i];
	}

    omap_sr_disable_reset_volt(mpu_voltdm);

    for (i = 0; i < num_freqs; i++)
	{
	    mpu_voltdm->vdd->volt_data[i].volt_nominal = voltages[i];
	    mpu_voltdm->vdd->volt_data[i].volt_calibrated = 0;
	    mpu_voltdm->vdd->dep_vdd_info->dep_table[i].main_vdd_volt = voltages[i];
	}

    omap_sr_enable(mpu_voltdm, omap_voltage_get_curr_vdata(mpu_voltdm));

    mutex_unlock(frequency_mutex);

    return;
}

ssize_t customvoltage_voltages_read(struct device * dev, struct device_attribute * attr, char * buf)
{
    int i, j = 0;

    for (i = num_freqs - 1; i >= 0; i--)
	{
	    j += sprintf(&buf[j], "%umhz: %lu mV\n", frequency_table[i].frequency / 1000, voltages[i] / 1000);
	}

    return j;
}
EXPORT_SYMBOL(customvoltage_voltages_read);

ssize_t customvoltage_voltages_write(struct device * dev, struct device_attribute * attr, const char * buf, size_t size)
{
    int i = 0, j = 0, next_freq = 0;
    unsigned long voltage;

    char buffer[20];

    while (1)
	{
	    buffer[j] = buf[i];

	    i++;
	    j++;

	    if (buf[i] == ' ' || buf[i] == '\0')
		{
		    buffer[j] = '\0';

		    if (sscanf(buffer, "%lu", &voltage) == 1)
			{
			    voltages[num_freqs - 1 - next_freq] = voltage * 1000;
		
			    next_freq++;
			}

		    if (buf[i] == '\0' || next_freq > num_freqs)
			{
			    break;
			}

		    j = 0;
		}
	}

    customvoltage_voltages_update();

    return size;
}
EXPORT_SYMBOL(customvoltage_voltages_write);

static ssize_t customvoltage_version(struct device * dev, struct device_attribute * attr, char * buf)
{
    return sprintf(buf, "%u\n", CUSTOMVOLTAGE_VERSION);
}

static DEVICE_ATTR(voltages, S_IRUGO | S_IWUGO, customvoltage_voltages_read, customvoltage_voltages_write);
static DEVICE_ATTR(version, S_IRUGO , customvoltage_version, NULL);

static struct attribute *customvoltage_attributes[] = 
    {
	&dev_attr_voltages.attr,
	&dev_attr_version.attr,
	NULL
    };

static struct attribute_group customvoltage_group = 
    {
	.attrs  = customvoltage_attributes,
    };

static struct miscdevice customvoltage_device = 
    {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "customvoltage",
    };

static int __init customvoltage_initialization(void)
{
    int ret;

    pr_info("%s misc_register(%s)\n", __FUNCTION__, customvoltage_device.name);

    ret = misc_register(&customvoltage_device);

    if (ret) 
	{
	    pr_err("%s misc_register(%s) fail\n", __FUNCTION__, customvoltage_device.name);

	    return 1;
	}

    if (sysfs_create_group(&customvoltage_device.this_device->kobj, &customvoltage_group) < 0) 
	{
	    pr_err("%s sysfs_create_group fail\n", __FUNCTION__);
	    pr_err("Failed to create sysfs group for device (%s)!\n", customvoltage_device.name);
	}

    return 0;
}

device_initcall(customvoltage_initialization);
