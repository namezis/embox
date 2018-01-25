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

#include <util/err.h>

#include <embox_drm/drm_priv.h>
#include <embox_drm/drm_gem.h>

#include "etnaviv_gem.h"
#include "etnaviv_drv.h"


int etnaviv_gem_obj_add(struct drm_device *dev, struct drm_gem_object *obj) {

	struct etnaviv_drm_private *priv = dev->dev_private;
	struct etnaviv_gem_object *etnaviv_obj = to_etnaviv_bo(obj);

	pthread_mutex_lock(&priv->gem_lock);
	list_add_tail(&etnaviv_obj->gem_node, &priv->gem_list);
	pthread_mutex_unlock(&priv->gem_lock);

	return 0;
}

static int etnaviv_gem_new_impl(struct drm_device *dev, uint32_t size,
		uint32_t flags, struct reservation_object *robj,
		const struct etnaviv_gem_ops *ops, struct drm_gem_object **obj) {
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
