/**
 * @file
 *
 * @date Jan 18, 2018
 * @author Anton Bondarev
 */

#include <stdint.h>
#include <stddef.h>
#include <util/err.h>

#include <drm/drm_priv.h>
#include "etnaviv_gem.h"

int etnaviv_gem_obj_add(struct drm_device *dev, struct drm_gem_object *obj) {
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

	fail: return err_ptr(ret);
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

	return ret;
}

struct drm_gem_object *etnaviv_gem_new(struct drm_device *dev, uint32_t size,
		uint32_t flags) {
	struct drm_gem_object *obj;

	obj = __etnaviv_gem_new(dev, size, flags);
	if (err(obj))
		return obj;

	return obj;
}
