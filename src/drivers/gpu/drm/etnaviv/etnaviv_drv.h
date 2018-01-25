/**
 * @file
 *
 * @date Jan 18, 2018
 * @author Anton Bondarev
 */

#ifndef SRC_DRIVERS_GPU_DRM_ETNAVIV_ETNAVIV_DRV_H_
#define SRC_DRIVERS_GPU_DRM_ETNAVIV_ETNAVIV_DRV_H_

#include <pthread.h>

#include <linux/list.h>

#include <embox_drm/drm_priv.h>

#define ETNA_MAX_PIPES 4

struct etnaviv_drm_private {
	int num_gpus;
	struct etnaviv_gpu *gpu[ETNA_MAX_PIPES];

	/* list of GEM objects: */
	pthread_mutex_t gem_lock;
	struct list_head gem_list;

};

#endif /* SRC_DRIVERS_GPU_DRM_ETNAVIV_ETNAVIV_DRV_H_ */
