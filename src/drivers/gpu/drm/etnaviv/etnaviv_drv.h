/**
 * @file
 *
 * @date Jan 18, 2018
 * @author Anton Bondarev
 */

#ifndef SRC_DRIVERS_GPU_DRM_ETNAVIV_ETNAVIV_DRV_H_
#define SRC_DRIVERS_GPU_DRM_ETNAVIV_ETNAVIV_DRV_H_

#include <drm/drm_priv.h>

#define ETNA_MAX_PIPES 4

struct etnaviv_drm_private {
	int num_gpus;
	struct etnaviv_gpu *gpu[ETNA_MAX_PIPES];
};

#endif /* SRC_DRIVERS_GPU_DRM_ETNAVIV_ETNAVIV_DRV_H_ */
