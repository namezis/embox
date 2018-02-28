/**
 * @file
 *
 * @date Jan 18, 2018
 * @author Anton Bondarev
 */

#include <util/log.h>

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include <stdio.h>
#include <util/err.h>
#include <mem/sysmalloc.h>

#include <embox_drm/drm_priv.h>
#include <embox_drm/drm_gem.h>

#include "etnaviv_gem.h"
#include "etnaviv_drv.h"


int etnaviv_gem_obj_add(struct drm_device *dev, struct drm_gem_object *obj) {

	struct etnaviv_drm_private *priv = dev->dev_private;
	printf("trace %s %d\n", __func__, __LINE__);
	struct etnaviv_gem_object *etnaviv_obj = to_etnaviv_bo(obj);

	printf("trace %s %d\n", __func__, __LINE__);
	//pthread_mutex_lock(&priv->gem_lock);
	printf("trace %s %d\n", __func__, __LINE__);
	printf("obj=%p,etobj=%p,priv=%p\n", obj, etnaviv_obj, priv);
	//list_add_tail(&etnaviv_obj->gem_node, &priv->gem_list);
	printf("trace %s %d\n", __func__, __LINE__);
	//pthread_mutex_unlock(&priv->gem_lock);
	printf("trace %s %d\n", __func__, __LINE__);

	return 0;
}

static int etnaviv_gem_new_impl(struct drm_device *dev, uint32_t size,
		uint32_t flags, struct reservation_object *robj,
		const struct etnaviv_gem_ops *ops, struct drm_gem_object **obj) {
	struct etnaviv_gem_object *etnaviv_obj;
	unsigned sz = sizeof(*etnaviv_obj);
	etnaviv_obj = sysmalloc(sz);

	if (!etnaviv_obj)
		return -ENOMEM;

	etnaviv_obj->flags = flags;
	etnaviv_obj->ops = ops;
	if (robj) {
	//	etnaviv_obj->resv = robj;
	} else {
	//	etnaviv_obj->resv = &etnaviv_obj->_resv;
	//	reservation_object_init(&etnaviv_obj->_resv);
	}

	mutex_init(&etnaviv_obj->lock);
	INIT_LIST_HEAD(&etnaviv_obj->gem_node);

	*obj = &etnaviv_obj->base;

	return 0;
}

static struct drm_gem_object *__etnaviv_gem_new(struct drm_device *dev,
		uint32_t size, uint32_t flags) {
	struct drm_gem_object *obj = NULL;
	int ret;

	ret = etnaviv_gem_new_impl(dev, size, flags, NULL,
	NULL, &obj);
	if (ret)
		goto fail;

	ret = drm_gem_object_init(dev, obj, size);
#if 0
	if (ret == 0) {
		struct address_space *mapping;

		/*
		 * Our buffers are kept pinned, so allocating them
		 * from the MOVABLE zone is a really bad idea, and
		 * conflicts with CMA.  See coments above new_inode()
		 * why this is required _and_ expected if you're
		 * going to pin these pages.
		 */
		mapping = obj->filp->f_mapping;
		mapping_set_gfp_mask(mapping, GFP_HIGHUSER);
	}
#endif
	return obj;

fail:
	return err_ptr(ret);
}

/* convenience method to construct a GEM buffer object, and userspace handle */
int etnaviv_gem_new_handle(struct drm_device *dev, struct drm_file *file,
		uint32_t size, uint32_t flags, uint32_t *handle) {
	struct drm_gem_object *obj;
	int ret;

	obj = __etnaviv_gem_new(dev, size, flags);
	if (err(obj))
		return (int) obj;

	ret = etnaviv_gem_obj_add(dev, obj);
	if (ret < 0) {
		return ret;
	}
	ret = drm_gem_handle_create(file, obj, handle);

	/* drop reference from allocate - handle holds it now */
	//drm_gem_object_unreference_unlocked(obj);

	return ret;
}

struct drm_gem_object *etnaviv_gem_new(struct drm_device *dev, uint32_t size,
		uint32_t flags) {
	struct drm_gem_object *obj;
	int ret;

	obj = __etnaviv_gem_new(dev, size, flags);
	if (err(obj))
		return obj;

	ret = etnaviv_gem_obj_add(dev, obj);
	if (ret < 0) {
		//drm_gem_object_unreference_unlocked(obj);
		return err_ptr(ret);
	}

	return obj;
}


int etnaviv_gem_mmap_offset(struct drm_gem_object *obj, uint64_t *offset)
{
	*offset = (int) obj;
	return 0;
	int ret;

	/* Make it mmapable */
	ret = drm_gem_create_mmap_offset(obj);
	if (ret) {
		log_error("could not allocate mmap offset");
	} else {
		*offset = drm_vma_node_offset_addr(&obj->vma_node);
	}

	return ret;
}

struct etnaviv_vram_mapping *etnaviv_gem_mapping_get(
	struct drm_gem_object *obj, struct etnaviv_gpu *gpu)
{
	struct etnaviv_gem_object *etnaviv_obj = to_etnaviv_bo(obj);
	struct etnaviv_vram_mapping *mapping;

	mapping = kzalloc(sizeof(*mapping), GFP_KERNEL);
	if (!mapping) {
		return 0;
	}

	//INIT_LIST_HEAD(&mapping->scan_node);
	mapping->object = etnaviv_obj;
	mapping->iova = (uint32_t) obj->dma_buf - 0x10000000;

	log_debug("object=%p, iova=%p", etnaviv_obj, (void*) mapping->iova);

	return mapping;
}
