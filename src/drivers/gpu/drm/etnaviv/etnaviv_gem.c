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

	return obj;

fail:
	return err_ptr(ret);
}

/* convenience method to construct a GEM buffer object, and userspace handle */
int etnaviv_gem_new_handle(struct drm_device *dev, struct drm_file *file,
		uint32_t size, uint32_t flags, uint32_t *handle) {
	struct drm_gem_object *obj;
	int ret;

	printf("trace %s %d\n", __func__, __LINE__);
	obj = __etnaviv_gem_new(dev, size, flags);
	printf("new obj is %p\n", obj);
	if (err(obj))
		return (int) obj;

	printf("trace %s %d\n", __func__, __LINE__);
	ret = etnaviv_gem_obj_add(dev, obj);
	if (ret < 0) {
		return ret;
	}
	printf("trace %s %d\n", __func__, __LINE__);
	ret = drm_gem_handle_create(file, obj, handle);

	printf("trace %s %d\n", __func__, __LINE__);
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
	}
	else {
		*offset = drm_vma_node_offset_addr(&obj->vma_node);
	}

	return ret;
}
