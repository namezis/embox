/**
 * @file
 *
 * @date Feb 8, 2018
 * @author Anton Bondarev
 */
#include <drivers/pm_runtime.h>


/**
 * rpm_suspend - Carry out runtime suspend of given device.
 * @dev: Device to suspend.
 * @rpmflags: Flag bits.
 *
 * Check if the device's runtime PM status allows it to be suspended.
 * Cancel a pending idle notification, autosuspend or suspend. If
 * another suspend has been started earlier, either return immediately
 * or wait for it to finish, depending on the RPM_NOWAIT and RPM_ASYNC
 * flags. If the RPM_ASYNC flag is set then queue a suspend request;
 * otherwise run the ->runtime_suspend() callback directly. When
 * ->runtime_suspend succeeded, if a deferred resume was requested while
 * the callback was running then carry it out, otherwise send an idle
 * notification for its parent (if the suspend succeeded and both
 * ignore_children of parent->power and irq_safe of dev->power are not set).
 * If ->runtime_suspend failed with -EAGAIN or -EBUSY, and if the RPM_AUTO
 * flag is set and the next autosuspend-delay expiration time is in the
 * future, schedule another autosuspend attempt.
 *
 * This function must be called under dev->power.lock with interrupts disabled.
 */
static int rpm_suspend(struct device *dev, int rpmflags) {
	return 0;
}

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
static int rpm_resume(struct device *dev, int rpmflags) {
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

/**
 * __pm_runtime_suspend - Entry point for runtime put/suspend operations.
 * @dev: Device to suspend.
 * @rpmflags: Flag bits.
 *
 * If the RPM_GET_PUT flag is set, decrement the device's usage count and
 * return immediately if it is larger than zero.  Then carry out a suspend,
 * either synchronous or asynchronous.
 *
 * This routine may be called in atomic context if the RPM_ASYNC flag is set,
 * or if pm_runtime_irq_safe() has been called.
 */
int
__pm_runtime_suspend(struct device *dev, int rpmflags)
{
	//unsigned long flags;
	int retval;
#if 0
	if (rpmflags & RPM_GET_PUT) {
		if (!atomic_dec_and_test(&dev->power.usage_count))
			return 0;
	}

	might_sleep_if(!(rpmflags & RPM_ASYNC) && !dev->power.irq_safe);
#endif
	//spin_lock_irqsave(&dev->power.lock, flags);
	retval = rpm_suspend(dev, rpmflags);
	//spin_unlock_irqrestore(&dev->power.lock, flags);

	return retval;
}


/**
 * pm_runtime_enable - Enable runtime PM of a device.
 * @dev: Device to handle.
 */
void pm_runtime_enable(struct device *dev) {
}


/**
 * update_autosuspend - Handle a change to a device's autosuspend settings.
 * @dev: Device to handle.
 * @old_delay: The former autosuspend_delay value.
 * @old_use: The former use_autosuspend value.
 *
 * Prevent runtime suspend if the new delay is negative and use_autosuspend is
 * set; otherwise allow it.  Send an idle notification if suspends are allowed.
 *
 * This function must be called under dev->power.lock with interrupts disabled.
 */
static void update_autosuspend(struct device *dev, int old_delay, int old_use) {
#if 0
	int delay = dev->power.autosuspend_delay;

	/* Should runtime suspend be prevented now? */
	if (dev->power.use_autosuspend && delay < 0) {

		/* If it used to be allowed then prevent it. */
		if (!old_use || old_delay >= 0) {
			atomic_inc(&dev->power.usage_count);
			rpm_resume(dev, 0);
		}
	}

	/* Runtime suspend should be allowed now. */
	else {

		/* If it used to be prevented then allow it. */
		if (old_use && old_delay < 0)
			atomic_dec(&dev->power.usage_count);

		/* Maybe we can autosuspend now. */
		rpm_idle(dev, RPM_AUTO);
	}
#endif
}

/**
 * __pm_runtime_use_autosuspend - Set a device's use_autosuspend flag.
 * @dev: Device to handle.
 * @use: New value for use_autosuspend.
 *
 * Set the device's power.use_autosuspend flag, and allow or prevent runtime
 * suspends as needed.
 */
void __pm_runtime_use_autosuspend(struct device *dev, int use)
{

	int old_delay, old_use;
#if 0
	spin_lock_irq(&dev->power.lock);
	old_delay = dev->power.autosuspend_delay;
	old_use = dev->power.use_autosuspend;
	dev->power.use_autosuspend = use;
	update_autosuspend(dev, old_delay, old_use);
	spin_unlock_irq(&dev->power.lock);
#endif
	old_delay = 10;
	old_use = 10;
	update_autosuspend(dev, old_delay, old_use);
}

/**
 * pm_runtime_set_autosuspend_delay - Set a device's autosuspend_delay value.
 * @dev: Device to handle.
 * @delay: Value of the new delay in milliseconds.
 *
 * Set the device's power.autosuspend_delay value.  If it changes to negative
 * and the power.use_autosuspend flag is set, prevent runtime suspends.  If it
 * changes the other way, allow runtime suspends.
 */
void pm_runtime_set_autosuspend_delay(struct device *dev, int delay)
{
	int old_delay, old_use;
#if 0
	spin_lock_irq(&dev->power.lock);
	old_delay = dev->power.autosuspend_delay;
	old_use = dev->power.use_autosuspend;
	dev->power.autosuspend_delay = delay;
	update_autosuspend(dev, old_delay, old_use);
	spin_unlock_irq(&dev->power.lock);
#endif
	old_delay = 10;
	old_use = 10;
	update_autosuspend(dev, old_delay, old_use);

}
