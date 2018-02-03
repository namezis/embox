/**
 * @file etnaviv_compat.h
 * @brief
 * @author Denis Deryugin <deryugin.denis@gmail.com>
 * @version
 * @date 31.01.2018
 */

#ifndef ETNAVIV_COMPAT_H_
#define ETNAVIV_COMPAT_H_

#include <errno.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <mem/sysmalloc.h>
#include <string.h>

#define GFP_TEMORARY  0
#define GFP_KERNEL    0
#define __GFP_NOWARN  0
#define __GFP_NORETRY 0

#define drm_malloc_ab(a, b) sysmalloc((a) * (b))
#define kmalloc(a, b) sysmalloc(a)
#define kzalloc(a, b) sysmalloc(a)
#define kfree(a) sysfree(a)

#define ALIGN(v,a) (((v) + (a) - 1) & ~((a) - 1))
#define u64_to_user_ptr(a) ((uint32_t) a)
#define copy_from_user(a, b, c) (0 * (int) memcpy((void*) a, (void*) b, c))

#define DRM_ERROR log_error

#define atomic_t int

struct reservation_object {
	int stub;
};

struct vm_area_struct {
	int stub;
};

struct ww_acquire_ctx {
	int stub;
};

static inline int order_base_2(int q) {
	int res;
	while (q > 0) {
		res++;
		q /= 2;
	}

	return res;
};

#define u64 uint64_t
#define u32 uint32_t
#define u16 uint16_t
#define u8 uint8_t

#define __initconst
#define __init

#define BIT(n) (1 << (n))

#endif
