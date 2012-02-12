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
#include <linux/plist.h>

#include "../../arch/arm/mach-omap2/voltage.h"
#include "../../arch/arm/mach-omap2/smartreflex.h"

#define CUSTOMVOLTAGE_VERSION 2

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

struct omap_vdd_user_list {
    struct device * dev;
    struct plist_node node;
};

struct omap_vdd_dvfs_info {
    struct list_head node;

    spinlock_t user_lock; /* spin lock */
    struct plist_head vdd_user_list;
    struct voltagedomain * voltdm;
    struct list_head dev_list;
};

static int num_mpuvolt, num_corevolt, num_ivavolt, num_mpudeps, num_ivadeps;

static struct mutex * frequency_mutex = NULL;
static struct mutex * dvfs_mutex = NULL;

static int num_mpufreqs;

static u32 ** mpu_voltages = NULL;

static struct device * mpu_device = NULL;

static struct voltagedomain * mpu_voltdm = NULL;
static struct omap_vdd_dvfs_info * mpu_dvfsinfo = NULL;

static int * mpu_depend = NULL;

static struct opp ** mpu_opp = NULL;

static int num_gpufreqs, num_l3freqs, num_fdiffreqs, num_hsifreqs;

static u32 ** core_voltages = NULL;

static struct device * gpu_device = NULL;
static struct device * l3_device = NULL;
static struct device * fdif_device = NULL;
static struct device * hsi_device = NULL;

static struct voltagedomain * core_voltdm = NULL;
static struct omap_vdd_dvfs_info * core_dvfsinfo = NULL;

static int * gpu_depend = NULL;
static int * l3_depend = NULL;
static int * fdif_depend = NULL;
static int * hsi_depend = NULL;

static struct opp ** gpu_opp = NULL;
static struct opp ** l3_opp = NULL;
static struct opp ** fdif_opp = NULL;
static struct opp ** hsi_opp = NULL;

static int num_ivafreqs, num_dspfreqs, num_aessfreqs;

static u32 ** iva_voltages = NULL;

static struct device * iva_device = NULL;
static struct device * dsp_device = NULL;
static struct device * aess_device = NULL;

static struct voltagedomain * iva_voltdm = NULL;
static struct omap_vdd_dvfs_info * iva_dvfsinfo = NULL;

static int * iva_depend = NULL;
static int * dsp_depend = NULL;
static int * aess_depend = NULL;

static struct opp ** iva_opp = NULL;
static struct opp ** dsp_opp = NULL;
static struct opp ** aess_opp = NULL;

static int * mpu_depindex = NULL;
static int * mpucore_depindex = NULL;

static int * iva_depindex = NULL;
static int * ivacore_depindex = NULL;

static unsigned long * new_voltages = NULL;

extern struct device_opp * find_device_opp(struct device * dev);
extern struct omap_vdd_dvfs_info * _voltdm_to_dvfs_info(struct voltagedomain * voltdm);

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
    else if (!strcmp(dev_name, "l3_main_1"))
	{
	    if (!l3_device)
		l3_device = dev;
	}
    else if (!strcmp(dev_name, "fdif"))
	{
	    if (!fdif_device)
		fdif_device = dev;
	}
    else if (!strcmp(dev_name, "hsi"))
	{
	    if (!hsi_device)
		hsi_device = dev;
	}
    else if (!strcmp(dev_name, "dsp"))
	{
	    if (!dsp_device)
		dsp_device = dev;
	}
    else if (!strcmp(dev_name, "aess"))
	{
	    if (!aess_device)
		aess_device = dev;
	}

    return;
}
EXPORT_SYMBOL(customvoltage_register_oppdevice);

void customvoltage_init(void)
{
    int i, j;

    struct device_opp * dev_opp;

    struct opp * temp_opp;

    struct omap_volt_data * volt_data;

    unsigned long voltage;

    // MPU voltage domain
    mpu_voltdm = voltdm_lookup("mpu");

    mpu_dvfsinfo = _voltdm_to_dvfs_info(mpu_voltdm);

    num_mpuvolt = 0;

    volt_data = mpu_voltdm->vdd->volt_data;

    while (volt_data->volt_nominal)
	{
	    num_mpuvolt++;
	    volt_data++;
	}

    mpu_voltages = kzalloc(num_mpuvolt * sizeof(u32 *), GFP_KERNEL);

    for (i = 0; i < num_mpuvolt; i++)
	{
	    mpu_voltages[i] = &(mpu_voltdm->vdd->volt_data[i].volt_nominal);
	}

    dev_opp = find_device_opp(mpu_device);

    num_mpufreqs = 0;

    list_for_each_entry(temp_opp, &dev_opp->opp_list, node)
	{
	    if (temp_opp->available)
		num_mpufreqs++;
	}

    mpu_opp = kzalloc(num_mpufreqs * sizeof(struct opp *), GFP_KERNEL);

    i = 0;

    list_for_each_entry(temp_opp, &dev_opp->opp_list, node)
	{
	    if (temp_opp->available)
		{
		    mpu_opp[i] = temp_opp;

		    i++;
		}
	}

    mpu_depend = kzalloc(num_mpufreqs * sizeof(int), GFP_KERNEL);

    for (i = 0; i < num_mpuvolt; i++)
	{
	    voltage = mpu_voltdm->vdd->volt_data[i].volt_nominal;

	    for (j = 0; j < num_mpufreqs; j++)
		{
		    if (mpu_opp[j]->u_volt == voltage)
			mpu_depend[j] = i;
		}
	}

    // core voltage domain
    core_voltdm = voltdm_lookup("core");

    core_dvfsinfo = _voltdm_to_dvfs_info(core_voltdm);

    num_corevolt = 0;

    volt_data = core_voltdm->vdd->volt_data;

    while (volt_data->volt_nominal)
	{
	    num_corevolt++;
	    volt_data++;
	}

    core_voltages = kzalloc(num_corevolt * sizeof(u32 *), GFP_KERNEL);

    for (i = 0; i < num_corevolt; i++)
	{
	    core_voltages[i] = &(core_voltdm->vdd->volt_data[i].volt_nominal);
	}

    dev_opp = find_device_opp(gpu_device);

    num_gpufreqs = 0;

    list_for_each_entry(temp_opp, &dev_opp->opp_list, node)
	{
	    if (temp_opp->available)
		num_gpufreqs++;
	}

    gpu_opp = kzalloc(num_gpufreqs * sizeof(struct opp *), GFP_KERNEL);

    i = 0;

    list_for_each_entry(temp_opp, &dev_opp->opp_list, node)
	{
	    if (temp_opp->available)
		{
		    gpu_opp[i] = temp_opp;

		    i++;
		}
	}

    dev_opp = find_device_opp(l3_device);

    num_l3freqs = 0;

    list_for_each_entry(temp_opp, &dev_opp->opp_list, node)
	{
	    if (temp_opp->available)
		num_l3freqs++;
	}

    l3_opp = kzalloc(num_l3freqs * sizeof(struct opp *), GFP_KERNEL);

    i = 0;

    list_for_each_entry(temp_opp, &dev_opp->opp_list, node)
	{
	    if (temp_opp->available)
		{
		    l3_opp[i] = temp_opp;

		    i++;
		}
	}

    dev_opp = find_device_opp(fdif_device);

    num_fdiffreqs = 0;

    list_for_each_entry(temp_opp, &dev_opp->opp_list, node)
	{
	    if (temp_opp->available)
		num_fdiffreqs++;
	}

    fdif_opp = kzalloc(num_fdiffreqs * sizeof(struct opp *), GFP_KERNEL);

    i = 0;

    list_for_each_entry(temp_opp, &dev_opp->opp_list, node)
	{
	    if (temp_opp->available)
		{
		    fdif_opp[i] = temp_opp;

		    i++;
		}
	}

    dev_opp = find_device_opp(hsi_device);

    num_hsifreqs = 0;

    list_for_each_entry(temp_opp, &dev_opp->opp_list, node)
	{
	    if (temp_opp->available)
		num_hsifreqs++;
	}

    hsi_opp = kzalloc(num_hsifreqs * sizeof(struct opp *), GFP_KERNEL);

    i = 0;

    list_for_each_entry(temp_opp, &dev_opp->opp_list, node)
	{
	    if (temp_opp->available)
		{
		    hsi_opp[i] = temp_opp;

		    i++;
		}
	}

    gpu_depend = kzalloc(num_gpufreqs * sizeof(int), GFP_KERNEL);
    l3_depend = kzalloc(num_l3freqs * sizeof(int), GFP_KERNEL);
    fdif_depend = kzalloc(num_fdiffreqs * sizeof(int), GFP_KERNEL);
    hsi_depend = kzalloc(num_hsifreqs * sizeof(int), GFP_KERNEL);

    for (i = 0; i < num_corevolt; i++)
	{
	    voltage = core_voltdm->vdd->volt_data[i].volt_nominal;

	    for (j = 0; j < num_gpufreqs; j++)
		{
		    if (gpu_opp[j]->u_volt == voltage)
			gpu_depend[j] = i;
		}

	    for (j = 0; j < num_l3freqs; j++)
		{
		    if (l3_opp[j]->u_volt == voltage)
			l3_depend[j] = i;
		}

	    for (j = 0; j < num_fdiffreqs; j++)
		{
		    if (fdif_opp[j]->u_volt == voltage)
			fdif_depend[j] = i;
		}

	    for (j = 0; j < num_hsifreqs; j++)
		{
		    if (hsi_opp[j]->u_volt == voltage)
			hsi_depend[j] = i;
		}
	}

    // IVA voltage domain
    iva_voltdm = voltdm_lookup("iva");

    iva_dvfsinfo = _voltdm_to_dvfs_info(iva_voltdm);

    num_ivavolt = 0;

    volt_data = iva_voltdm->vdd->volt_data;

    while (volt_data->volt_nominal)
	{
	    num_ivavolt++;
	    volt_data++;
	}

    iva_voltages = kzalloc(num_ivavolt * sizeof(u32 *), GFP_KERNEL);

    for (i = 0; i < num_ivavolt; i++)
	{
	    iva_voltages[i] = &(iva_voltdm->vdd->volt_data[i].volt_nominal);
	}

    dev_opp = find_device_opp(iva_device);

    num_ivafreqs = 0;

    list_for_each_entry(temp_opp, &dev_opp->opp_list, node)
	{
	    if (temp_opp->available)
		num_ivafreqs++;
	}

    iva_opp = kzalloc(num_ivafreqs * sizeof(struct opp *), GFP_KERNEL);

    i = 0;

    list_for_each_entry(temp_opp, &dev_opp->opp_list, node)
	{
	    if (temp_opp->available)
		{
		    iva_opp[i] = temp_opp;

		    i++;
		}
	}

    dev_opp = find_device_opp(dsp_device);

    num_dspfreqs = 0;

    list_for_each_entry(temp_opp, &dev_opp->opp_list, node)
	{
	    if (temp_opp->available)
		num_dspfreqs++;
	}

    dsp_opp = kzalloc(num_dspfreqs * sizeof(struct opp *), GFP_KERNEL);

    i = 0;

    list_for_each_entry(temp_opp, &dev_opp->opp_list, node)
	{
	    if (temp_opp->available)
		{
		    dsp_opp[i] = temp_opp;

		    i++;
		}
	}

    dev_opp = find_device_opp(aess_device);

    num_aessfreqs = 0;

    list_for_each_entry(temp_opp, &dev_opp->opp_list, node)
	{
	    if (temp_opp->available)
		num_aessfreqs++;
	}

    aess_opp = kzalloc(num_aessfreqs * sizeof(struct opp *), GFP_KERNEL);

    i = 0;

    list_for_each_entry(temp_opp, &dev_opp->opp_list, node)
	{
	    if (temp_opp->available)
		{
		    aess_opp[i] = temp_opp;

		    i++;
		}
	}

    iva_depend = kzalloc(num_ivafreqs * sizeof(int), GFP_KERNEL);
    dsp_depend = kzalloc(num_dspfreqs * sizeof(int), GFP_KERNEL);
    aess_depend = kzalloc(num_aessfreqs * sizeof(int), GFP_KERNEL);

    for (i = 0; i < num_corevolt; i++)
	{
	    voltage = core_voltdm->vdd->volt_data[i].volt_nominal;

	    for (j = 0; j < num_ivafreqs; j++)
		{
		    if (iva_opp[j]->u_volt == voltage)
			iva_depend[j] = i;
		}

	    for (j = 0; j < num_dspfreqs; j++)
		{
		    if (dsp_opp[j]->u_volt == voltage)
			dsp_depend[j] = i;
		}

	    for (j = 0; j < num_aessfreqs; j++)
		{
		    if (aess_opp[j]->u_volt == voltage)
			aess_depend[j] = i;
		}
	}

    // MPU->core and IVA->core dependency tables
    num_mpudeps = mpu_voltdm->vdd->dep_vdd_info->nr_dep_entries;

    mpu_depindex = kzalloc(num_mpudeps * sizeof(int), GFP_KERNEL);
    mpucore_depindex = kzalloc(num_mpudeps * sizeof(int), GFP_KERNEL);

    for (i = 0; i < num_mpudeps; i++)
	{
	    for (j = 0; j < num_mpuvolt; j++)
		{
		    if (mpu_voltdm->vdd->dep_vdd_info->dep_table[i].main_vdd_volt
			== mpu_voltdm->vdd->volt_data[j].volt_nominal)
			{
			    mpu_depindex[i] = j;
			}
		}

	    for (j = 0; j < num_corevolt; j++)
		{
		    if (mpu_voltdm->vdd->dep_vdd_info->dep_table[i].dep_vdd_volt
			== core_voltdm->vdd->volt_data[j].volt_nominal)
			{
			    mpucore_depindex[i] = j;
			}
		}
	}

    num_ivadeps = iva_voltdm->vdd->dep_vdd_info->nr_dep_entries;

    iva_depindex = kzalloc(num_ivadeps * sizeof(int), GFP_KERNEL);
    ivacore_depindex = kzalloc(num_ivadeps * sizeof(int), GFP_KERNEL);

    for (i = 0; i < num_ivadeps; i++)
	{
	    for (j = 0; j < num_ivavolt; j++)
		{
		    if (iva_voltdm->vdd->dep_vdd_info->dep_table[i].main_vdd_volt
			== iva_voltdm->vdd->volt_data[j].volt_nominal)
			{
			    iva_depindex[i] = j;
			}
		}

	    for (j = 0; j < num_corevolt; j++)
		{
		    if (iva_voltdm->vdd->dep_vdd_info->dep_table[i].dep_vdd_volt
			== core_voltdm->vdd->volt_data[j].volt_nominal)
			{
			    ivacore_depindex[i] = j;
			}
		}
	}

    new_voltages = kzalloc(max(max(num_ivavolt, num_corevolt), num_mpuvolt) * sizeof(int), GFP_KERNEL);

    return;
}
EXPORT_SYMBOL(customvoltage_init);

ssize_t customvoltage_mpuvolt_read(struct device * dev, struct device_attribute * attr, char * buf)
{
    int i, j = 0;

    for (i = num_mpuvolt - 1; i >= 0; i--)
	{
	    j += sprintf(&buf[j], "%lumhz: %lu mV\n", mpu_opp[i]->rate / 1000000, (long unsigned)(*mpu_voltages[i] / 1000));
	}

    return j;
}
EXPORT_SYMBOL(customvoltage_mpuvolt_read);

static void customvoltage_mpuvolt_update(void)
{
    int i, j;

    struct omap_vdd_user_list * vdd_user;

    mutex_lock(frequency_mutex);
    mutex_lock(dvfs_mutex);

    spin_lock(&mpu_dvfsinfo->user_lock);

    plist_for_each_entry(vdd_user, &mpu_dvfsinfo->vdd_user_list, node)
	{
	    for (i = 0; i < num_mpuvolt; i++)
		{
		    if (vdd_user->node.prio == mpu_voltdm->vdd->volt_data[i].volt_nominal)
			{
			    vdd_user->node.prio = new_voltages[i];
			    
			    break;
			}
		}
	}

    for (i = 0; i < num_mpuvolt; i++)
	{
	    mpu_voltdm->vdd->volt_data[i].volt_nominal = new_voltages[i];
	    mpu_voltdm->vdd->volt_data[i].volt_calibrated = 0;

	    for (j = 0; j < num_mpufreqs; j++)
		if (mpu_depend[j] == i)
		    mpu_opp[j]->u_volt = new_voltages[i];
	}

    for (i = 0; i < num_mpudeps; i++)
	mpu_voltdm->vdd->dep_vdd_info->dep_table[i].main_vdd_volt = new_voltages[mpu_depindex[i]];

    spin_unlock(&mpu_dvfsinfo->user_lock);

    omap_sr_disable_reset_volt(mpu_voltdm);
    omap_sr_enable(mpu_voltdm, omap_voltage_get_curr_vdata(mpu_voltdm));

    mutex_unlock(dvfs_mutex);
    mutex_unlock(frequency_mutex);

    return;
}

ssize_t customvoltage_mpuvolt_write(struct device * dev, struct device_attribute * attr, const char * buf, size_t size)
{
    int i = 0, j = 0, next_volt = 0;
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
			    new_voltages[num_mpuvolt - 1 - next_volt] = voltage * 1000;
		
			    next_volt++;
			}

		    if (buf[i] == '\0' || next_volt >= num_mpuvolt)
			{
			    break;
			}

		    j = 0;
		}
	}

    for (i = 0; i < num_mpuvolt - next_volt; i++)
	new_voltages[i] = *mpu_voltages[i];

    customvoltage_mpuvolt_update();

    return size;
}
EXPORT_SYMBOL(customvoltage_mpuvolt_write);

static ssize_t customvoltage_corevolt_read(struct device * dev, struct device_attribute * attr, char * buf)
{
    int i, j = 0;

    for (i = num_corevolt - 1; i >= 0; i--)
	{
	    j += sprintf(&buf[j], "%lu mV\n", (long unsigned)(*core_voltages[i] / 1000));
	}

    return j;
}

static void customvoltage_corevolt_update(void)
{
    int i, j;

    struct omap_vdd_user_list * vdd_user;

    mutex_lock(frequency_mutex);
    mutex_lock(dvfs_mutex);

    spin_lock(&core_dvfsinfo->user_lock);

    plist_for_each_entry(vdd_user, &core_dvfsinfo->vdd_user_list, node)
	{
	    for (i = 0; i < num_corevolt; i++)
		{
		    if (vdd_user->node.prio == core_voltdm->vdd->volt_data[i].volt_nominal)
			{
			    vdd_user->node.prio = new_voltages[i];
			    
			    break;
			}
		}
	}

    for (i = 0; i < num_corevolt; i++)
	{
	    core_voltdm->vdd->volt_data[i].volt_nominal = new_voltages[i];
	    core_voltdm->vdd->volt_data[i].volt_calibrated = 0;

	    for (j = 0; j < num_gpufreqs; j++)
		if (gpu_depend[j] == i)
		    gpu_opp[j]->u_volt = new_voltages[i];

	    for (j = 0; j < num_l3freqs; j++)
		if (l3_depend[j] == i)
		    l3_opp[j]->u_volt = new_voltages[i];

	    for (j = 0; j < num_fdiffreqs; j++)
		if (fdif_depend[j] == i)
		    fdif_opp[j]->u_volt = new_voltages[i];

	    for (j = 0; j < num_hsifreqs; j++)
		if (hsi_depend[j] == i)
		    hsi_opp[j]->u_volt = new_voltages[i];
	}

    for (i = 0; i < num_mpudeps; i++)
	mpu_voltdm->vdd->dep_vdd_info->dep_table[i].dep_vdd_volt = new_voltages[mpucore_depindex[i]];

    for (i = 0; i < num_ivadeps; i++)
	iva_voltdm->vdd->dep_vdd_info->dep_table[i].dep_vdd_volt = new_voltages[ivacore_depindex[i]];

    spin_unlock(&core_dvfsinfo->user_lock);

    omap_sr_disable_reset_volt(core_voltdm);
    omap_sr_enable(core_voltdm, omap_voltage_get_curr_vdata(core_voltdm));

    mutex_unlock(dvfs_mutex);
    mutex_unlock(frequency_mutex);

    return;
}

static ssize_t customvoltage_corevolt_write(struct device * dev, struct device_attribute * attr, const char * buf, size_t size)
{
    int i = 0, j = 0, next_volt = 0;
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
			    new_voltages[num_corevolt - 1 - next_volt] = voltage * 1000;
		
			    next_volt++;
			}

		    if (buf[i] == '\0' || next_volt >= num_corevolt)
			{
			    break;
			}

		    j = 0;
		}
	}

    for (i = 0; i < num_corevolt - next_volt; i++)
	new_voltages[i] = *core_voltages[i];

    customvoltage_corevolt_update();

    return size;
}

static ssize_t customvoltage_ivavolt_read(struct device * dev, struct device_attribute * attr, char * buf)
{
    int i, j = 0;

    for (i = num_ivavolt - 1; i >= 0; i--)
	{
	    j += sprintf(&buf[j], "%lu mV\n", (long unsigned)(*iva_voltages[i] / 1000));
	}

    return j;
}

static void customvoltage_ivavolt_update(void)
{
    int i, j;

    struct omap_vdd_user_list * vdd_user;

    mutex_lock(frequency_mutex);
    mutex_lock(dvfs_mutex);

    spin_lock(&iva_dvfsinfo->user_lock);

    plist_for_each_entry(vdd_user, &iva_dvfsinfo->vdd_user_list, node)
	{
	    for (i = 0; i < num_ivavolt; i++)
		{
		    if (vdd_user->node.prio == iva_voltdm->vdd->volt_data[i].volt_nominal)
			{
			    vdd_user->node.prio = new_voltages[i];
			    
			    break;
			}
		}
	}

    for (i = 0; i < num_ivavolt; i++)
	{
	    iva_voltdm->vdd->volt_data[i].volt_nominal = new_voltages[i];
	    iva_voltdm->vdd->volt_data[i].volt_calibrated = 0;

	    for (j = 0; j < num_ivafreqs; j++)
		if (iva_depend[j] == i)
		    iva_opp[j]->u_volt = new_voltages[i];

	    for (j = 0; j < num_dspfreqs; j++)
		if (dsp_depend[j] == i)
		    dsp_opp[j]->u_volt = new_voltages[i];

	    for (j = 0; j < num_aessfreqs; j++)
		if (aess_depend[j] == i)
		    aess_opp[j]->u_volt = new_voltages[i];
	}

    for (i = 0; i < num_ivadeps; i++)
	iva_voltdm->vdd->dep_vdd_info->dep_table[i].main_vdd_volt = new_voltages[iva_depindex[i]];

    spin_unlock(&iva_dvfsinfo->user_lock);

    omap_sr_disable_reset_volt(iva_voltdm);
    omap_sr_enable(iva_voltdm, omap_voltage_get_curr_vdata(iva_voltdm));

    mutex_unlock(dvfs_mutex);
    mutex_unlock(frequency_mutex);

    return;
}

static ssize_t customvoltage_ivavolt_write(struct device * dev, struct device_attribute * attr, const char * buf, size_t size)
{
    int i = 0, j = 0, next_volt = 0;
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
			    new_voltages[num_ivavolt - 1 - next_volt] = voltage * 1000;
		
			    next_volt++;
			}

		    if (buf[i] == '\0' || next_volt >= num_ivavolt)
			{
			    break;
			}

		    j = 0;
		}
	}

    for (i = 0; i < num_ivavolt - next_volt; i++)
	new_voltages[i] = *iva_voltages[i];

    customvoltage_ivavolt_update();

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
