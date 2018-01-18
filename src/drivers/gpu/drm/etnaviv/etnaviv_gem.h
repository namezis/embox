/**
 * @file
 *
 * @date Jan 18, 2018
 * @author Anton Bondarev
 */
#ifndef SRC_DRIVERS_GPU_DRM_ETNAVIV_ETNAVIV_GEM_H_
#define SRC_DRIVERS_GPU_DRM_ETNAVIV_ETNAVIV_GEM_H_

#include <stdint.h>
#include <drm/drm_priv.h>


struct etnaviv_gem_ops {
	int todo;
};

extern int etnaviv_gem_new_handle(struct drm_device *dev, struct drm_file *file,
		uint32_t size, uint32_t flags, uint32_t *handle);

extern struct drm_gem_object *etnaviv_gem_new(struct drm_device *dev,
		uint32_t size, uint32_t flags);

extern int etnaviv_ioctl_gem_info(struct drm_device *dev, void *data,
		struct drm_file *file);

extern int etnaviv_ioctl_gem_submit(struct drm_device *dev, void *data,
		struct drm_file *file);

#endif /* SRC_DRIVERS_GPU_DRM_ETNAVIV_ETNAVIV_GEM_H_ */
