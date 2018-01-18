/**
 * @file
 *
 * @date Jan 18, 2018
 * @author Anton Bondarev
 */

#ifndef SRC_DRIVERS_GPU_DRM_ETNAVIV_ETNAVIV_GPU_H_
#define SRC_DRIVERS_GPU_DRM_ETNAVIV_ETNAVIV_GPU_H_

#include <stdint.h>


struct etnaviv_chip_identity {
	/* Chip model. */
	uint32_t model;

	/* Revision value.*/
	uint32_t revision;

	/* Supported feature fields. */
	uint32_t features;

	/* Supported minor feature fields. */
	uint32_t minor_features0;

	/* Supported minor feature 1 fields. */
	uint32_t minor_features1;

	/* Supported minor feature 2 fields. */
	uint32_t minor_features2;

	/* Supported minor feature 3 fields. */
	uint32_t minor_features3;

	/* Supported minor feature 4 fields. */
	uint32_t minor_features4;

	/* Supported minor feature 5 fields. */
	uint32_t minor_features5;

	/* Number of streams supported. */
	uint32_t stream_count;

	/* Total number of temporary registers per thread. */
	uint32_t register_max;

	/* Maximum number of threads. */
	uint32_t thread_count;

	/* Number of shader cores. */
	uint32_t shader_core_count;

	/* Size of the vertex cache. */
	uint32_t vertex_cache_size;

	/* Number of entries in the vertex output buffer. */
	uint32_t vertex_output_buffer_size;

	/* Number of pixel pipes. */
	uint32_t pixel_pipes;

	/* Number of instructions. */
	uint32_t instruction_count;

	/* Number of constants. */
	uint32_t num_constants;

	/* Buffer size */
	uint32_t buffer_size;

	/* Number of varyings */
	uint8_t varyings_count;
};

struct etnaviv_gpu {
	struct drm_device *drm;

	struct etnaviv_chip_identity identity;
};

extern int etnaviv_gpu_get_param(struct etnaviv_gpu *gpu, uint32_t param,
		uint64_t *value);

#endif /* SRC_DRIVERS_GPU_DRM_ETNAVIV_ETNAVIV_GPU_H_ */
