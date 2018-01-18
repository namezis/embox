/**
 * @file
 * @brief Direct Rendering Manager compatibility module
 * @author Denis Deryugin <deryugin.denis@gmail.com>
 * @version
 * @date 06.12.2017
 */

#include <util/log.h>

#include <stddef.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <util/err.h>

#include <drivers/char_dev.h>
#include <fs/dvfs.h>

#include <drm.h>
#include <etnaviv_drm.h>

#include <drm/drm_priv.h>

#include "etnaviv_drv.h"
#include "etnaviv_gpu.h"
#include "etnaviv_gem.h"

#define VERSION_NAME      "IPU"
#define VERSION_NAME_LEN  3
#define VERSION_DATE      "7 Dec 2017"
#define VERSION_DATE_LEN  10
#define VERSION_DESC      "DEADBEEF"
#define VERSION_DESC_LEN  8

#define ETNAVIV_DEV_NAME "card"


extern int etnaviv_ioctl_wait_fence(struct drm_device *dev, void *data, struct drm_file *file);



/*
 * DRM ioctls:
 */
#if 0
static int etnaviv_ioctl_get_param(struct drm_device *dev, void *data,
		struct drm_file *file)
{
	struct etnaviv_drm_private *priv = dev->dev_private;
	struct drm_etnaviv_param *args = data;
	struct etnaviv_gpu *gpu;

	if (args->pipe >= ETNA_MAX_PIPES)
		return -EINVAL;

	gpu = priv->gpu[args->pipe];
	if (!gpu)
		return -ENXIO;

	return etnaviv_gpu_get_param(gpu, args->param, &args->value);
}
#endif

int etnaviv_ioctl_gem_new(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct drm_etnaviv_gem_new *args = data;

	if (args->flags & ~(ETNA_BO_CACHED | ETNA_BO_WC | ETNA_BO_UNCACHED |
			    ETNA_BO_FORCE_MMU))
		return -EINVAL;

	return etnaviv_gem_new_handle(dev, file, args->size,
			args->flags, &args->handle);
}

static struct idesc_ops etnaviv_dev_idesc_ops;

static struct drm_device etnaviv_drm_device;
static struct drm_file etnaviv_drm_file;

static struct idesc *etnaviv_dev_open(struct inode *node, struct idesc *idesc) {
	struct file *file;

	file = dvfs_alloc_file();
	if (!file) {
		return err_ptr(ENOMEM);
	}
	*file = (struct file) {
		.f_idesc  = {
				.idesc_ops   = &etnaviv_dev_idesc_ops,
		},
	};
	return &file->f_idesc;
}

static void etnaviv_dev_close(struct idesc *desc) {
	dvfs_destroy_file((struct file *)desc);
}

static ssize_t etnaviv_dev_write(struct idesc *desc, const struct iovec *iov, int cnt) {
	return 0;
}

static ssize_t etnaviv_dev_read(struct idesc *desc, const struct iovec *iov, int cnt) {
	return 0;
}


static int etnaviv_dev_idesc_ioctl(struct idesc *idesc, int request, void *data) {
	drm_version_t *version;
	struct drm_etnaviv_param *req;
	int nr = _IOC_NR(request);
	struct drm_device *dev = &etnaviv_drm_device;
	struct drm_file *file = &etnaviv_drm_file;


	log_debug("dir=%d, type=%d, nr=%d", _IOC_DIR(request), _IOC_TYPE(request), _IOC_NR(request));
	switch (nr) {
	case 0: /* DRM_IOCTL_VERSION */
		version = data;
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
		req = data;
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
		//etnaviv_ioctl_gem_info(dev, data, file);
		break;
	case 70: /* DRM_ETNAVIV_GEM_SUBMIT */
		//etnaviv_ioctl_gem_submit(dev, data, file);
		break;
	case 71: /* DRM_ETNAVIV_WAIT_FENCE */
		//etnaviv_ioctl_wait_fence(dev, data, file);
		break;
	default:
		log_debug("NIY, request=%d", request);
	}
	return 0;
}

static int etnaviv_dev_idesc_fstat(struct idesc *idesc, void *buff) {
       return 0;
}

static int etnaviv_dev_idesc_status(struct idesc *idesc, int mask) {
	return 0;
}

static struct file_operations etnaviv_dev_ops = {
		.open = etnaviv_dev_open,
};

static struct idesc_ops etnaviv_dev_idesc_ops = {
	.close = etnaviv_dev_close,
	.id_readv = etnaviv_dev_read,
	.id_writev = etnaviv_dev_write,
	.ioctl  = etnaviv_dev_idesc_ioctl,
	.fstat  = etnaviv_dev_idesc_fstat,
	.status = etnaviv_dev_idesc_status,
};

CHAR_DEV_DEF(ETNAVIV_DEV_NAME, &etnaviv_dev_ops, &etnaviv_dev_idesc_ops, NULL);

