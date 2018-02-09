/**
 * @file
 *
 * @date Feb 8, 2018
 * @author Anton Bondarev
 */
#include <drivers/pm_runtime.h>


/**
 * rpm_resume - Carry out runtime resume of given device.
 * @dev: Device to resume.
 * @rpmflags: Flag bits.
 *
 * Check if the device's runtime PM status allows it to be resumed.  Cancel
 * any scheduled or pending requests.  If another resume has been started
 * earlier, either return immediately or wait for it to finish, depending on the
 * RPM_NOWAIT and RPM_ASYNC flags.  Similarly, if there's a suspend running in
 * parallel with this function, either tell the other process to resume after
 * suspending (deferred_resume) or wait for it to finish.  If the RPM_ASYNC
 * flag is set then queue a resume request; otherwise run the
 * ->runtime_resume() callback directly.  Queue an idle notification for the
 * device if the resume succeeded.
 *
 * This function must be called under dev->power.lock with interrupts disabled.
 */
static int rpm_resume(struct device *dev, int rpmflags)
{
	return 0;
}


/**
 * __pm_runtime_resume - Entry point for runtime resume operations.
 * @dev: Device to resume.
 * @rpmflags: Flag bits.
 *
 * If the RPM_GET_PUT flag is set, increment the device's usage count.  Then
 * carry out a resume, either synchronous or asynchronous.
 *
 * This routine may be called in atomic context if the RPM_ASYNC flag is set,
 * or if pm_runtime_irq_safe() has been called.
 */
int
__pm_runtime_resume(struct device *dev, int rpmflags) {
	//unsigned long flags;
	int retval;

#if 0
	might_sleep_if(!(rpmflags & RPM_ASYNC) && !dev->power.irq_safe &&
			dev->power.runtime_status != RPM_ACTIVE);

	if (rpmflags & RPM_GET_PUT)
		atomic_inc(&dev->power.usage_count);
#endif
	//spin_lock_irqsave(&dev->power.lock, flags);
	retval = rpm_resume(dev, rpmflags);
	//spin_unlock_irqrestore(&dev->power.lock, flags);

	return retval;
}
