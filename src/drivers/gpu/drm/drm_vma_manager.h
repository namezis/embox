/**
 * @file
 *
 * @date Jan 19, 2018
 * @author Anton Bondarev
 */

#ifndef SRC_DRIVERS_GPU_DRM_DRM_VMA_MANAGER_H_
#define SRC_DRIVERS_GPU_DRM_DRM_VMA_MANAGER_H_

#include <embox_drm/drm_mm.h>
#include <asm/page.h>

struct drm_vma_offset_node {
	//rwlock_t vm_lock;
	struct drm_mm_node vm_node;
	//struct rb_root vm_files;
};


/**
 * drm_vma_node_offset_addr() - Return sanitized offset for user-space mmaps
 * @node: Linked offset node
 *
 * Same as drm_vma_node_start() but returns the address as a valid offset that
 * can be used for user-space mappings during mmap().
 * This must not be called on unlinked nodes.
 *
 * RETURNS:
 * Offset of @node for byte-based addressing. 0 if the node does not have an
 * object allocated.
 */
static inline uint64_t drm_vma_node_offset_addr(struct drm_vma_offset_node *node)
{
	return ((uint64_t)node->vm_node.start) << PAGE_SHIFT;
}

#endif /* SRC_DRIVERS_GPU_DRM_DRM_VMA_MANAGER_H_ */
