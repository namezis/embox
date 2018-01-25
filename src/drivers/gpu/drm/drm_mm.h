/**
 * @file
 * @brief GEM Graphics Execution Manager Driver Interfaces
 *
 * @date Jan 19, 2018
 * @author Anton Bondarev
 */

#ifndef SRC_DRIVERS_GPU_DRM_DRM_MM_H_
#define SRC_DRIVERS_GPU_DRM_DRM_MM_H_

#include <stdint.h>

struct drm_mm;

/**
 * struct drm_mm_node - allocated block in the DRM allocator
 *
 * This represents an allocated block in a &drm_mm allocator. Except for
 * pre-reserved nodes inserted using drm_mm_reserve_node() the structure is
 * entirely opaque and should only be accessed through the provided funcions.
 * Since allocation of these nodes is entirely handled by the driver they can be
 * embedded.
 */
struct drm_mm_node {
	/** @color: Opaque driver-private tag. */
	unsigned long color;
	/** @start: Start address of the allocated block. */
	uint64_t start;
	/** @size: Size of the allocated block. */
	uint64_t size;
	/* private: */
	struct drm_mm *mm;
#if 0
	struct list_head node_list;
	struct list_head hole_stack;

	struct rb_node rb;
	struct rb_node rb_hole_size;
	struct rb_node rb_hole_addr;
#endif
	uint64_t __subtree_last;
	uint64_t hole_size;
	int allocated;
	int scanned_block;

};

/**
 * struct drm_mm - DRM allocator
 *
 * DRM range allocator with a few special functions and features geared towards
 * managing GPU memory. Except for the @color_adjust callback the structure is
 * entirely opaque and should only be accessed through the provided functions
 * and macros. This structure can be embedded into larger driver structures.
 */
struct drm_mm {
	/**
	 * @color_adjust:
	 *
	 * Optional driver callback to further apply restrictions on a hole. The
	 * node argument points at the node containing the hole from which the
	 * block would be allocated (see drm_mm_hole_follows() and friends). The
	 * other arguments are the size of the block to be allocated. The driver
	 * can adjust the start and end as needed to e.g. insert guard pages.
	 */
	void (*color_adjust)(const struct drm_mm_node *node,
			     unsigned long color,
			     uint64_t *start, uint64_t *end);

#if 0
	/* private: */
	/* List of all memory nodes that immediately precede a free hole. */
	struct list_head hole_stack;
	/* head_node.node_list is the list of all memory nodes, ordered
	 * according to the (increasing) start address of the memory node. */
	struct drm_mm_node head_node;

	/* Keep an interval_tree for fast lookup of drm_mm_nodes by address. */
	struct rb_root interval_tree;
	struct rb_root holes_size;
	struct rb_root holes_addr;
#endif
	unsigned long scan_active;
};


#endif /* SRC_DRIVERS_GPU_DRM_DRM_MM_H_ */
