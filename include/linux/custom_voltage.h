/* include/linux/custom_voltage.h */

#ifndef _LINUX_CUSTOM_VOLTAGE_H
#define _LINUX_CUSTOM_VOLTAGE_H

extern void customvoltage_register_freqmutex(struct mutex * freqmutex);
extern void customvoltage_register_dvfsmutex(struct mutex * dvfsmutex);
extern void customvoltage_init(void);
extern ssize_t customvoltage_mpuvolt_read(struct device * dev, struct device_attribute * attr, char * buf);
extern ssize_t customvoltage_mpuvolt_write(struct device * dev, struct device_attribute * attr, const char * buf, size_t size);
extern void customvoltage_register_oppdevice(struct device * dev, char * dev_name);

#endif
