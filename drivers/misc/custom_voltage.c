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
#include <linux/opp.h>
#include <linux/slab.h>

#include "../../arch/arm/mach-omap2/voltage.h"
#include "../../arch/arm/mach-omap2/smartreflex.h"

#define CUSTOMVOLTAGE_VERSION 2

static struct mutex * frequency_mutex = NULL;
static struct mutex * dvfs_mutex = NULL;

static int num_mpufreqs;

static unsigned long ** mpu_voltages = NULL;
static unsigned long ** mpu_freqs = NULL;

static struct device * mpu_device = NULL;

static struct voltagedomain * mpu_voltdm = NULL;

static int num_gpufreqs;

static unsigned long ** core_voltages = NULL;
static unsigned long ** gpu_freqs = NULL;

static struct device * gpu_device = NULL;

static struct voltagedomain * core_voltdm = NULL;

static int num_ivafreqs;

static unsigned long ** iva_voltages = NULL;
static unsigned long ** iva_freqs = NULL;

static struct device * iva_device = NULL;

static struct voltagedomain * iva_voltdm = NULL;

static int * mpu_depvolt = NULL;
static int * iva_depvolt = NULL;

struct opp {
    struct list_head node;
    
    bool available;
    unsigned long rate;
    unsigned long u_volt;
    
    struct device_opp *dev_opp;
};

struct device_opp {
    struct list_head node;

    struct device * dev;
    struct list_head opp_list;
};

extern struct device_opp * find_device_opp(struct device * dev);

void customvoltage_register_freqmutex(struct mutex * freqmutex)
{
    frequency_mutex = freqmutex;

    return;
}
EXPORT_SYMBOL(customvoltage_register_freqmutex);

void customvoltage_register_dvfsmutex(struct mutex * dvfsmutex)
{
    dvfs_mutex = dvfsmutex;

    return;
}
EXPORT_SYMBOL(customvoltage_register_dvfsmutex);

void customvoltage_register_oppdevice(struct device * dev, char * dev_name)
{
    if (!strcmp(dev_name, "mpu"))
	{
	    if (!mpu_device)
		mpu_device = dev;
	}
    else if (!strcmp(dev_name, "gpu"))
	{
	    if (!gpu_device)
		gpu_device = dev;
	}
    else if (!strcmp(dev_name, "iva"))
	{
	    if (!iva_device)
		iva_device = dev;
	}

    return;
}
EXPORT_SYMBOL(customvoltage_register_oppdevice);

void customvoltage_init(void)
{
    int i, j;

    struct device_opp * dev_opp;

    struct opp * temp_opp;

    mpu_voltdm = voltdm_lookup("mpu");

    dev_opp = find_device_opp(mpu_device);

    num_mpufreqs = 0;

    list_for_each_entry(temp_opp, &dev_opp->opp_list, node)
	{
	    if (temp_opp->available)
		num_mpufreqs++;
	}

    mpu_voltages = kzalloc(num_mpufreqs * sizeof(unsigned long *), GFP_KERNEL);
    mpu_freqs = kzalloc(num_mpufreqs * sizeof(unsigned long *), GFP_KERNEL);

    i = 0;

    list_for_each_entry(temp_opp, &dev_opp->opp_list, node)
	{
	    if (temp_opp->available)
		{
		    mpu_voltages[i] = &(temp_opp->u_volt);
		    mpu_freqs[i] = &(temp_opp->rate);

		    i++;
		}
	}

    core_voltdm = voltdm_lookup("core");

    dev_opp = find_device_opp(gpu_device);

    num_gpufreqs = 0;

    list_for_each_entry(temp_opp, &dev_opp->opp_list, node)
	{
	    if (temp_opp->available)
		num_gpufreqs++;
	}

    core_voltages = kzalloc(num_gpufreqs * sizeof(unsigned long *), GFP_KERNEL);
    gpu_freqs = kzalloc(num_gpufreqs * sizeof(unsigned long *), GFP_KERNEL);

    i = 0;

    list_for_each_entry(temp_opp, &dev_opp->opp_list, node)
	{
	    if (temp_opp->available)
		{
		    core_voltages[i] = &(temp_opp->u_volt);
		    gpu_freqs[i] = &(temp_opp->rate);

		    i++;
		}
	}

    iva_voltdm = voltdm_lookup("iva");

    dev_opp = find_device_opp(iva_device);

    num_ivafreqs = 0;

    list_for_each_entry(temp_opp, &dev_opp->opp_list, node)
	{
	    if (temp_opp->available)
		num_ivafreqs++;
	}

    iva_voltages = kzalloc(num_ivafreqs * sizeof(unsigned long *), GFP_KERNEL);
    iva_freqs = kzalloc(num_ivafreqs * sizeof(unsigned long *), GFP_KERNEL);

    i = 0;

    list_for_each_entry(temp_opp, &dev_opp->opp_list, node)
	{
	    if (temp_opp->available)
		{
		    iva_voltages[i] = &(temp_opp->u_volt);
		    iva_freqs[i] = &(temp_opp->rate);

		    i++;
		}
	}

    mpu_depvolt = kzalloc(num_mpufreqs * sizeof(int), GFP_KERNEL);

    for (i = 0; i < num_mpufreqs; i++)
	{
	    for (j = 0; j < num_gpufreqs; j++)
		{
		    if (mpu_voltdm->vdd->dep_vdd_info->dep_table[i].dep_vdd_volt
			== core_voltdm->vdd->volt_data[j].volt_nominal)
			{
			    mpu_depvolt[i] = j;

			    break;
			}
		}
	}

    iva_depvolt = kzalloc(num_ivafreqs * sizeof(int), GFP_KERNEL);

    for (i = 0; i < num_ivafreqs; i++)
	{
	    for (j = 0; j < num_gpufreqs; j++)
		{
		    if (iva_voltdm->vdd->dep_vdd_info->dep_table[i].dep_vdd_volt 
			== core_voltdm->vdd->volt_data[j].volt_nominal)
			{
			    iva_depvolt[i] = j;

			    break;
			}
		}
	}

    return;
}
EXPORT_SYMBOL(customvoltage_init);

ssize_t customvoltage_mpuvolt_read(struct device * dev, struct device_attribute * attr, char * buf)
{
    int i, j = 0;

    for (i = num_mpufreqs - 1; i >= 0; i--)
	{
	    j += sprintf(&buf[j], "%lumhz: %lu mV\n", *mpu_freqs[i] / 1000000, *mpu_voltages[i] / 1000);
	}

    return j;
}
EXPORT_SYMBOL(customvoltage_mpuvolt_read);

ssize_t customvoltage_mpuvolt_write(struct device * dev, struct device_attribute * attr, const char * buf, size_t size)
{
    int i = 0, j = 0, next_freq = 0;
    unsigned long voltage;

    char buffer[20];

    mutex_lock(frequency_mutex);
    mutex_lock(dvfs_mutex);

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
			    *mpu_voltages[num_mpufreqs - 1 - next_freq] = voltage * 1000;
		
			    next_freq++;
			}

		    if (buf[i] == '\0' || next_freq > num_mpufreqs)
			{
			    break;
			}

		    j = 0;
		}
	}

    omap_sr_disable_reset_volt(mpu_voltdm);

    for (i = 0; i < num_mpufreqs; i++)
	{
	    mpu_voltdm->vdd->volt_data[i].volt_nominal = *mpu_voltages[i];
	    mpu_voltdm->vdd->volt_data[i].volt_calibrated = 0;
	    mpu_voltdm->vdd->dep_vdd_info->dep_table[i].main_vdd_volt = *mpu_voltages[i];
	}

    omap_sr_enable(mpu_voltdm, omap_voltage_get_curr_vdata(mpu_voltdm));

    mutex_unlock(dvfs_mutex);
    mutex_unlock(frequency_mutex);

    return size;
}
EXPORT_SYMBOL(customvoltage_mpuvolt_write);

static ssize_t customvoltage_corevolt_read(struct device * dev, struct device_attribute * attr, char * buf)
{
    int i, j = 0;

    for (i = num_gpufreqs - 1; i >= 0; i--)
	{
	    j += sprintf(&buf[j], "%lumhz: %lu mV\n", *gpu_freqs[i] / 1000000, *core_voltages[i] / 1000);
	}

    return j;
}

static ssize_t customvoltage_corevolt_write(struct device * dev, struct device_attribute * attr, const char * buf, size_t size)
{
    int i = 0, j = 0, next_freq = 0;
    unsigned long voltage;

    char buffer[20];

    mutex_lock(frequency_mutex);
    mutex_lock(dvfs_mutex);

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
			    *core_voltages[num_gpufreqs - 1 - next_freq] = voltage * 1000;
		
			    next_freq++;
			}

		    if (buf[i] == '\0' || next_freq > num_gpufreqs)
			{
			    break;
			}

		    j = 0;
		}
	}

    omap_sr_disable_reset_volt(core_voltdm);

    for (i = 0; i < num_gpufreqs; i++)
	{
	    core_voltdm->vdd->volt_data[i].volt_nominal = *core_voltages[i];
	    core_voltdm->vdd->volt_data[i].volt_calibrated = 0;
	}

    for (i = 0; i < num_mpufreqs; i++)
	{
	    mpu_voltdm->vdd->dep_vdd_info->dep_table[i].dep_vdd_volt = *core_voltages[mpu_depvolt[i]];
	}

    for (i = 0; i < num_ivafreqs; i++)
	{
	    iva_voltdm->vdd->dep_vdd_info->dep_table[i].dep_vdd_volt = *core_voltages[iva_depvolt[i]];
	}

    omap_sr_enable(core_voltdm, omap_voltage_get_curr_vdata(core_voltdm));

    mutex_unlock(dvfs_mutex);
    mutex_unlock(frequency_mutex);

    return size;
}

static ssize_t customvoltage_ivavolt_read(struct device * dev, struct device_attribute * attr, char * buf)
{
    int i, j = 0;

    for (i = num_ivafreqs - 1; i >= 0; i--)
	{
	    j += sprintf(&buf[j], "%lumhz: %lu mV\n", *iva_freqs[i] / 1000000, *iva_voltages[i] / 1000);
	}

    return j;
}

static ssize_t customvoltage_ivavolt_write(struct device * dev, struct device_attribute * attr, const char * buf, size_t size)
{
    int i = 0, j = 0, next_freq = 0;
    unsigned long voltage;

    char buffer[20];

    mutex_lock(frequency_mutex);
    mutex_lock(dvfs_mutex);

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
			    *iva_voltages[num_ivafreqs - 1 - next_freq] = voltage * 1000;
		
			    next_freq++;
			}

		    if (buf[i] == '\0' || next_freq > num_ivafreqs)
			{
			    break;
			}

		    j = 0;
		}
	}

    omap_sr_disable_reset_volt(iva_voltdm);

    for (i = 0; i < num_ivafreqs; i++)
	{
	    iva_voltdm->vdd->volt_data[i].volt_nominal = *iva_voltages[i];
	    iva_voltdm->vdd->volt_data[i].volt_calibrated = 0;
	    iva_voltdm->vdd->dep_vdd_info->dep_table[i].main_vdd_volt = *iva_voltages[i];
	}

    omap_sr_enable(iva_voltdm, omap_voltage_get_curr_vdata(iva_voltdm));

    mutex_unlock(dvfs_mutex);
    mutex_unlock(frequency_mutex);

    return size;
}

static ssize_t customvoltage_version(struct device * dev, struct device_attribute * attr, char * buf)
{
    return sprintf(buf, "%u\n", CUSTOMVOLTAGE_VERSION);
}

static DEVICE_ATTR(mpu_voltages, S_IRUGO | S_IWUGO, customvoltage_mpuvolt_read, customvoltage_mpuvolt_write);
static DEVICE_ATTR(core_voltages, S_IRUGO | S_IWUGO, customvoltage_corevolt_read, customvoltage_corevolt_write);
static DEVICE_ATTR(iva_voltages, S_IRUGO | S_IWUGO, customvoltage_ivavolt_read, customvoltage_ivavolt_write);
static DEVICE_ATTR(version, S_IRUGO , customvoltage_version, NULL);

static struct attribute *customvoltage_attributes[] = 
    {
	&dev_attr_mpu_voltages.attr,
	&dev_attr_core_voltages.attr,
	&dev_attr_iva_voltages.attr,
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
