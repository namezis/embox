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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <util/err.h>

#include <drivers/char_dev.h>
#include <drivers/common/memory.h>
#include <drivers/power/imx.h>
#include <fs/dvfs.h>
#include <kernel/irq.h>

#include <drm.h>
#include <etnaviv_drm.h>

#include <embox_drm/drm_priv.h>
#include <embox_drm/drm_gem.h>

#include "etnaviv_drv.h"
#include "etnaviv_gpu.h"
#include "etnaviv_gem.h"
#include "common.xml.h"
#include "state_hi.xml.h"

#define VERSION_NAME      "IPU"
#define VERSION_NAME_LEN  3
#define VERSION_DATE      "7 Dec 2017"
#define VERSION_DATE_LEN  10
#define VERSION_DESC      "DEADBEEF"
#define VERSION_DESC_LEN  8

#define ETNAVIV_DEV_NAME "card"
/* Interrupt numbers */
#define GPU3D_IRQ	OPTION_GET(NUMBER,gpu3d_irq)
#define R2D_GPU2D_IRQ	OPTION_GET(NUMBER,r2d_gpu2d_irq)
#define V2D_GPU2D_IRQ	OPTION_GET(NUMBER,v2d_gpu2d_irq)

/*
 * DRM ioctls:
 */

static int etnaviv_ioctl_get_param(struct drm_device *dev, void *data,
		struct drm_file *file)
{
	struct etnaviv_drm_private *priv = dev->dev_private;
	struct drm_etnaviv_param *args = data;
	struct etnaviv_gpu *gpu;

	if (args->pipe >= ETNA_MAX_PIPES) {
		return -EINVAL;
	}

	if (!(gpu = priv->gpu[args->pipe])) {
		return -ENXIO;
	}

	return etnaviv_gpu_get_param(gpu, args->param, &args->value);
}

int etnaviv_ioctl_gem_new(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct drm_etnaviv_gem_new *args = data;

	if (args->flags & ~(ETNA_BO_CACHED | ETNA_BO_WC | ETNA_BO_UNCACHED |
			    ETNA_BO_FORCE_MMU)) {
		log_error("unsupported flags");
		//return -EINVAL;
	}

	return etnaviv_gem_new_handle(dev, file, args->size,
			args->flags, &args->handle);
}

int etnaviv_ioctl_gem_info(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct drm_etnaviv_gem_info *args = data;
	struct drm_gem_object *obj;
	int ret;

	if (args->pad)
		return -EINVAL;

	obj = drm_gem_object_lookup(file, args->handle);
	if (!obj) {
		log_error("obj (%p) didn't found", args->handle);
		return -ENOENT;
	}
	log_debug("obj (%p) founded by handle (%i)", obj, args->handle);

	ret = etnaviv_gem_mmap_offset(obj, &args->offset);

	return ret;
}


static int etnaviv_ioctl_wait_fence(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct drm_etnaviv_wait_fence *args = data;
	struct etnaviv_drm_private *priv = dev->dev_private;
	struct timespec *timeout = (void *)&args->timeout;
	struct etnaviv_gpu *gpu;

	if (args->flags & ~(ETNA_WAIT_NONBLOCK))
		return -EINVAL;

	if (args->pipe >= ETNA_MAX_PIPES)
		return -EINVAL;

	gpu = priv->gpu[args->pipe];
	if (!gpu)
		return -ENXIO;

	if (args->flags & ETNA_WAIT_NONBLOCK)
		timeout = NULL;

	return etnaviv_gpu_wait_fence_interruptible(gpu, args->fence,
						    timeout);
}


static struct idesc_ops etnaviv_dev_idesc_ops;

static struct drm_device etnaviv_drm_device;
static struct drm_file etnaviv_drm_file;
static struct etnaviv_drm_private etnaviv_drm_private;
static struct etnaviv_gpu etnaviv_gpus[ETNA_MAX_PIPES];

#define VIVANTE_2D_BASE OPTION_GET(NUMBER,vivante_2d_base)
#define VIVANTE_3D_BASE OPTION_GET(NUMBER,vivante_3d_base)

static irq_return_t etna_irq_handler(unsigned int irq, void *data)
{
	struct etnaviv_gpu *gpu = data;
	irq_return_t ret = IRQ_NONE;

	uint32_t intr = gpu_read(gpu, VIVS_HI_INTR_ACKNOWLEDGE);

	if (intr != 0) {
		log_debug("intr 0x%08x", intr);

		if (intr & VIVS_HI_INTR_ACKNOWLEDGE_AXI_BUS_ERROR) {
			log_error("AXI bus error");
			intr &= ~VIVS_HI_INTR_ACKNOWLEDGE_AXI_BUS_ERROR;
		}

		if (intr & VIVS_HI_INTR_ACKNOWLEDGE_MMU_EXCEPTION) {
			int i;

			log_error("MMU fault status 0x%08x",
				gpu_read(gpu, VIVS_MMUv2_STATUS));
			for (i = 0; i < 4; i++) {
				log_error("MMU %d fault addr 0x%08x\n",
					i, gpu_read(gpu,
					VIVS_MMUv2_EXCEPTION_ADDR(i)));
			}
			intr &= ~VIVS_HI_INTR_ACKNOWLEDGE_MMU_EXCEPTION;
		}

		log_debug("no error");
		ret = IRQ_HANDLED;
	}

	return ret;
}

extern int clk_enable(char *clk_name);
extern int clk_disable(char *clk_name);
static struct idesc *etnaviv_dev_open(struct inode *node, struct idesc *idesc) {
	struct file *file;
	int i;

	file = dvfs_alloc_file();
	if (!file) {
		return err_ptr(ENOMEM);
	}

	*file = (struct file) {
		.f_idesc  = {
			.idesc_ops   = &etnaviv_dev_idesc_ops,
		},
	};

	etnaviv_drm_device.dev_private = &etnaviv_drm_private;
	for(i = 0; i < ETNA_MAX_PIPES; i ++) {
		etnaviv_drm_private.gpu[i] = &etnaviv_gpus[i];
	}
	etnaviv_gpus[PIPE_ID_PIPE_2D].mmio = (void *)VIVANTE_2D_BASE;
	etnaviv_gpus[PIPE_ID_PIPE_3D].mmio = (void *)VIVANTE_3D_BASE;


	if (irq_attach(	GPU3D_IRQ,
	                etna_irq_handler,
	                0,
	                &etnaviv_gpus[PIPE_ID_PIPE_3D],
	                "i.MX6 GPU3D")) {
		return NULL;
	}

	if (irq_attach(	R2D_GPU2D_IRQ,
	                etna_irq_handler,
	                0,
	                &etnaviv_gpus[PIPE_ID_PIPE_2D],
	                "i.MX6 GPU2D")) {
		return NULL;
	}

	if (irq_attach(	V2D_GPU2D_IRQ,
	                etna_irq_handler,
	                0,
	                &etnaviv_gpus[PIPE_ID_PIPE_2D],
	                "i.MX6 GPU2D")) {
		return NULL;
	}

	imx_gpu_power_set(1);

	clk_enable("gpu3d");
	clk_enable("gpu2d");
	clk_enable("openvg");
	clk_enable("vpu");

	etnaviv_gpu_init(&etnaviv_gpus[PIPE_ID_PIPE_2D]);
	etnaviv_gpu_init(&etnaviv_gpus[PIPE_ID_PIPE_3D]);

	etnaviv_gpu_debugfs(&etnaviv_gpus[PIPE_ID_PIPE_2D], "GPU2D");
	etnaviv_gpu_debugfs(&etnaviv_gpus[PIPE_ID_PIPE_2D], "GPU3D");

	return &file->f_idesc;
}

static void etnaviv_dev_close(struct idesc *desc) {
	dvfs_destroy_file((struct file *)desc);

	if (irq_detach(	GPU3D_IRQ,
	                &etnaviv_gpus[PIPE_ID_PIPE_3D])) {
		log_error("Failed to detach GPU3D_IRQ");
	}

	if (irq_detach(	R2D_GPU2D_IRQ,
	                &etnaviv_gpus[PIPE_ID_PIPE_2D])) {
		log_error("Failed to detach R2D_GPU2D_IRQ");
	}

	if (irq_detach(	V2D_GPU2D_IRQ,
	                &etnaviv_gpus[PIPE_ID_PIPE_2D])) {
		log_error("Failed to detach V2D_GPU2D_IRQ");
	}

	imx_gpu_power_set(0);
}

static ssize_t etnaviv_dev_write(struct idesc *desc, const struct iovec *iov, int cnt) {
	log_debug("trace");
	return 0;
}

static ssize_t etnaviv_dev_read(struct idesc *desc, const struct iovec *iov, int cnt) {
	log_debug("trace");
	return 0;
}

int etnaviv_dmp(int id) {
	assert(id >= 0 && id < ETNA_MAX_PIPES);
	etnaviv_gpu_debugfs(&etnaviv_gpus[id],
		id == PIPE_ID_PIPE_2D ? "GPU2D" : "GPU3D");
	return 0;
}

static int etnaviv_dev_idesc_ioctl(struct idesc *idesc, int request, void *data) {
	drm_version_t *version;
	int nr = _IOC_NR(request);
	struct drm_device *dev = &etnaviv_drm_device;
	struct drm_file *file = &etnaviv_drm_file;
	struct drm_etnaviv_param *args = data;
	int res = 0;

	log_debug("pipe=%cD, dir=%d, type=%d, nr=%d", args->pipe == PIPE_ID_PIPE_2D ? '2' : '3',
			_IOC_DIR(request), _IOC_TYPE(request), _IOC_NR(request));
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
	case DRM_COMMAND_BASE + DRM_ETNAVIV_GET_PARAM:
		res = etnaviv_ioctl_get_param(dev, data, file);
		break;
	case DRM_COMMAND_BASE + DRM_ETNAVIV_GEM_NEW:
		res = etnaviv_ioctl_gem_new(dev, data, file);
		break;
	case DRM_COMMAND_BASE + DRM_ETNAVIV_GEM_INFO:
		res = etnaviv_ioctl_gem_info(dev, data, file);
		break;
	case DRM_COMMAND_BASE + DRM_ETNAVIV_GEM_SUBMIT:
		res = etnaviv_ioctl_gem_submit(dev, data, file);
		break;
	case DRM_COMMAND_BASE + DRM_ETNAVIV_WAIT_FENCE:
		res = etnaviv_ioctl_wait_fence(dev, data, file);
		etnaviv_gpu_debugfs(&etnaviv_gpus[args->pipe], args->pipe == 0 ? "GPU3D" : "GPU2D");
		break;
	default:
		log_debug("NIY, request=%d", request);
		res = -ENOSYS;
	}
	return res;
}

static int etnaviv_dev_idesc_fstat(struct idesc *idesc, void *buff) {
       return 0;
}

static int etnaviv_dev_idesc_status(struct idesc *idesc, int mask) {
	return 0;
}

static uint8_t local_buffer[16 * 1024 * 1024] __attribute__ ((aligned (0x1000)));
static int ptr = 0;
static void *etnaviv_dev_idesc_mmap(struct idesc *idesc, void *addr, size_t len, int prot,
			int flags, int fd, off_t off) {
	void *res = &local_buffer[ptr];
	struct drm_gem_object *obj;
	obj = (void *) (uint32_t) off;

	obj->dma_buf = res;

	ptr += len;

	return res;
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
	.idesc_mmap = etnaviv_dev_idesc_mmap,
};

CHAR_DEV_DEF(ETNAVIV_DEV_NAME, &etnaviv_dev_ops, &etnaviv_dev_idesc_ops, NULL);

static struct periph_memory_desc vivante3d_mem = {
	.start = VIVANTE_3D_BASE,
	.len   = 0x4000,
};

static struct periph_memory_desc vivante2d_mem = {
	.start = VIVANTE_2D_BASE,
	.len   = 0x4000,
};

PERIPH_MEMORY_DEFINE(vivante2d_mem);
PERIPH_MEMORY_DEFINE(vivante3d_mem);
