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

extern void pm_runtime_enable(struct device *dev);
extern int __pm_runtime_resume(struct device *dev, int rpmflags);
extern int __pm_runtime_idle(struct device *dev, int rpmflags);
extern int __pm_runtime_suspend(struct device *dev, int rpmflags);
extern void __pm_runtime_use_autosuspend(struct device *dev, int use);
extern void pm_runtime_set_autosuspend_delay(struct device *dev, int delay);

static inline int pm_runtime_get_sync(struct device *dev)
{
	return __pm_runtime_resume(dev, RPM_GET_PUT);
}

static inline int pm_runtime_put_autosuspend(struct device *dev)
{
	return __pm_runtime_suspend(dev,
	    RPM_GET_PUT | RPM_ASYNC | RPM_AUTO);
}

static inline void pm_runtime_use_autosuspend(struct device *dev)
{
	__pm_runtime_use_autosuspend(dev, 1);
}

#endif /* SRC_DRIVERS_POWER_PM_RUNTIME_H_ */
