/**
 * @file
 *
 * @date Feb 8, 2018
 * @author Anton Bondarev
 */
#ifndef SRC_DRIVERS_POWER_PM_RUNTIME_H_
#define SRC_DRIVERS_POWER_PM_RUNTIME_H_


/* Runtime PM flag argument bits */
#define RPM_ASYNC		0x01	/* Request is asynchronous */
#define RPM_NOWAIT		0x02	/* Don't wait for concurrent
					    state change */
#define RPM_GET_PUT		0x04	/* Increment/decrement the
					    usage_count */
#define RPM_AUTO		0x08	/* Use autosuspend_delay */

struct device;

extern int __pm_runtime_resume(struct device *dev, int rpmflags);

static inline int pm_runtime_get_sync(struct device *dev)
{
	return __pm_runtime_resume(dev, RPM_GET_PUT);
}


#endif /* SRC_DRIVERS_POWER_PM_RUNTIME_H_ */
