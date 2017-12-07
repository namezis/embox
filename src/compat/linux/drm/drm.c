/**
 * @file drm.c
 * @brief Direct Rendering Manager compatibility module
 * @author Denis Deryugin <deryugin.denis@gmail.com>
 * @version
 * @date 06.12.2017
 */

#include <stdint.h>
#include <string.h>

#include <drivers/block_dev.h>
#include <embox/unit.h>
#include <fs/dvfs.h>
#include <mem/heap.h>
#include <util/log.h>

#include <drm/drm_file.h>

#include "etnaviv_drm.h"

#define VERSION_NAME      "IPU"
#define VERSION_NAME_LEN  3
#define VERSION_DATE      "7 Dec 2017"
#define VERSION_DATE_LEN  10
#define VERSION_DESC      "DEADBEEF"
#define VERSION_DESC_LEN  8

extern int etnaviv_ioctl_gem_new(struct drm_device *dev, void *data, struct drm_file *file);
extern int etnaviv_ioctl_gem_info(struct drm_device *dev, void *data, struct drm_file *file);
extern int etnaviv_ioctl_gem_submit(struct drm_device *dev, void *data, struct drm_file *file);
extern int etnaviv_ioctl_wait_fence(struct drm_device *dev, void *data, struct drm_file *file);

static int drm_ioctl(struct block_dev *bdev, int cmd, void *args, size_t size) {
	drm_version_t *version;
	struct drm_etnaviv_param *req;
	void *dev = 0;
	void *data = 0;
	void *file = 0;
	int nr = _IOC_NR(cmd);

	log_debug("dir=%d,type=%d,nr=%d", _IOC_DIR(cmd), _IOC_TYPE(cmd), _IOC_NR(cmd));
	log_debug("args = %p\n", args);
	switch (nr) {
	case 0: /* DRM_IOCTL_VERSION */
		version = args;
		*version = (drm_version_t) {
			.version_major  = 1,
			.version_minor  = 1,
			.name_len       = VERSION_NAME_LEN,
			.name           = strdup(VERSION_NAME),
			.date_len       = VERSION_DATE_LEN,
			.date           = strdup(VERSION_DATE),
			.desc_len       = VERSION_DESC_LEN,
			.desc           = strdup(VERSION_DESC),
		};
		break;
	case 64: /* DRM_ETNAVIV_GET_PARAM */
		req = args;
		switch (req->param) {
		case ETNAVIV_PARAM_GPU_MODEL:
			req->value = 0xdeadbeef;
			break;
		case ETNAVIV_PARAM_GPU_REVISION:
			req->value = 0xcafebabe;
			break;
		default:
			log_debug("NIY DRM_ETNAVIV_GET_PARAM,param=0x%08x", req->param);
			req->value = -1;
		}
		break;
	case 66: /* DRM_ETNAVIV_GEM_NEW */
		etnaviv_ioctl_gem_new(dev, data, file);
		break;
	case 67: /* DRM_ETNAVIV_GEM_INFO */
		etnaviv_ioctl_gem_info(dev, data, file);
		break;
	case 70: /* DRM_ETNAVIV_GEM_SUBMIT */
		etnaviv_ioctl_gem_submit(dev, data, file);
		break;
	case 71: /* DRM_ETNAVIV_WAIT_FENCE */
		etnaviv_ioctl_wait_fence(dev, data, file);
		break;
	default:
		log_debug("NIY, cmd = %d, args = %p", cmd, args);
	}
	return 0;
}

static int drm_read(struct block_dev *bdev, char *buffer, size_t count, blkno_t blkno) {
	log_debug("NIY");
	return 0;
}

static int drm_write(struct block_dev *bdev, char *buffer, size_t count, blkno_t blkno) {
	log_debug("NIY");
	return 0;
}

static struct block_dev_driver drm_driver = {
	.name = "drm_ops",
	.ioctl = drm_ioctl,
	.read = drm_read,
	.write = drm_write
};

static int drm_init(void) {
//	mkdir("/dev/dri", DVFS_DIR_VIRTUAL);
	block_dev_create("card", &drm_driver, NULL);
	return 0;
}
EMBOX_UNIT_INIT(drm_init);
