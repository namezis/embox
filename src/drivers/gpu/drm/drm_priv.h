/**
 * @file
 *
 * @date Jan 18, 2018
 * @author Anton Bondarev
 */

#ifndef SRC_DRIVERS_GPU_DRM_DRM_PRIV_H_
#define SRC_DRIVERS_GPU_DRM_DRM_PRIV_H_


struct reservation_object;

struct drm_device  {
	void *dev_private;
};

struct drm_file {
	int tmp;
};

struct drm_gem_object {
	int tmp;
};

#endif /* SRC_DRIVERS_GPU_DRM_DRM_PRIV_H_ */
