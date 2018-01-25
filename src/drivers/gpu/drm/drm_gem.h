/**
 * @file
 * @brief GEM Graphics Execution Manager Driver Interfaces
 *
 * @date Jan 19, 2018
 * @author Anton Bondarev
 */

#ifndef SRC_DRIVERS_GPU_DRM_DRM_GEM_H_
#define SRC_DRIVERS_GPU_DRM_DRM_GEM_H_

#include <stdint.h>
#include <stddef.h>

#include <embox_drm/drm_vma_manager.h>

struct file;
struct drm_device;
struct drm_file;

struct drm_gem_object {
	/**
	 * @handle_count:
	 *
	 * This is the GEM file_priv handle count of this object.
	 *
	 * Each handle also holds a reference. Note that when the handle_count
	 * drops to 0 any global names (e.g. the id in the flink namespace) will
	 * be cleared.
	 *
	 * Protected by &drm_device.object_name_lock.
	 */
	unsigned handle_count;

	/**
	 * @dev: DRM dev this object belongs to.
	 */
	struct drm_device *dev;

	/**
	 * @filp:
	 *
	 * SHMEM file node used as backing storage for swappable buffer objects.
	 * GEM also supports driver private objects with driver-specific backing
	 * storage (contiguous CMA memory, special reserved blocks). In this
	 * case @filp is NULL.
	 */
	struct file *filp;

	/**
	 * @vma_node:
	 *
	 * Mapping info for this object to support mmap. Drivers are supposed to
	 * allocate the mmap offset using drm_gem_create_mmap_offset(). The
	 * offset itself can be retrieved using drm_vma_node_offset_addr().
	 *
	 * Memory mapping itself is handled by drm_gem_mmap(), which also checks
	 * that userspace is allowed to access the object.
	 */
	struct drm_vma_offset_node vma_node;

	/**
	 * @size:
	 *
	 * Size of the object, in bytes.  Immutable over the object's
	 * lifetime.
	 */
	size_t size;

	/**
	 * @name:
	 *
	 * Global name for this object, starts at 1. 0 means unnamed.
	 * Access is covered by &drm_device.object_name_lock. This is used by
	 * the GEM_FLINK and GEM_OPEN ioctls.
	 */
	int name;

};

extern int drm_gem_create_mmap_offset(struct drm_gem_object *obj);

/**
 * drm_gem_object_get - acquire a GEM buffer object reference
 * @obj: GEM buffer object
 *
 * This function acquires an additional reference to @obj. It is illegal to
 * call this without already holding a reference. No locks required.
 */
static inline void drm_gem_object_get(struct drm_gem_object *obj)
{
	//kref_get(&obj->refcount);
}

/**
 * __drm_gem_object_put - raw function to release a GEM buffer object reference
 * @obj: GEM buffer object
 *
 * This function is meant to be used by drivers which are not encumbered with
 * &drm_device.struct_mutex legacy locking and which are using the
 * gem_free_object_unlocked callback. It avoids all the locking checks and
 * locking overhead of drm_gem_object_put() and drm_gem_object_put_unlocked().
 *
 * Drivers should never call this directly in their code. Instead they should
 * wrap it up into a ``driver_gem_object_put(struct driver_gem_object *obj)``
 * wrapper function, and use that. Shared code should never call this, to
 * avoid breaking drivers by accident which still depend upon
 * &drm_device.struct_mutex locking.
 */
static inline void
__drm_gem_object_put(struct drm_gem_object *obj)
{
	//kref_put(&obj->refcount, drm_gem_object_free);
}

extern void drm_gem_object_put_unlocked(struct drm_gem_object *obj);
extern void drm_gem_object_put(struct drm_gem_object *obj);

/**
 * drm_gem_object_reference - acquire a GEM buffer object reference
 * @obj: GEM buffer object
 *
 * This is a compatibility alias for drm_gem_object_get() and should not be
 * used by new code.
 */
static inline void drm_gem_object_reference(struct drm_gem_object *obj)
{
	drm_gem_object_get(obj);
}

/**
 * __drm_gem_object_unreference - raw function to release a GEM buffer object
 *                                reference
 * @obj: GEM buffer object
 *
 * This is a compatibility alias for __drm_gem_object_put() and should not be
 * used by new code.
 */
static inline void __drm_gem_object_unreference(struct drm_gem_object *obj)
{
	__drm_gem_object_put(obj);
}

/**
 * drm_gem_object_unreference_unlocked - release a GEM buffer object reference
 * @obj: GEM buffer object
 *
 * This is a compatibility alias for drm_gem_object_put_unlocked() and should
 * not be used by new code.
 */
static inline void
drm_gem_object_unreference_unlocked(struct drm_gem_object *obj)
{
	drm_gem_object_put_unlocked(obj);
}

/**
 * drm_gem_object_unreference - release a GEM buffer object reference
 * @obj: GEM buffer object
 *
 * This is a compatibility alias for drm_gem_object_put() and should not be
 * used by new code.
 */
static inline void drm_gem_object_unreference(struct drm_gem_object *obj)
{
	drm_gem_object_put(obj);
}



extern struct drm_gem_object *
drm_gem_object_lookup(struct drm_file *filp, uint32_t handle);

extern int drm_gem_handle_create(struct drm_file *file_priv,
			  struct drm_gem_object *obj,
			  uint32_t *handlep);


#endif /* SRC_DRIVERS_GPU_DRM_DRM_GEM_H_ */
