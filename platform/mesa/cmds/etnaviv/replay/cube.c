/*
 * Copyright (C) 2016 Etnaviv Project.
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */
/* Basic "hello world" test */
//#ifdef HAVE_CONFIG_H
//#  include "config.h"
//#endif

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <hal/mmu.h>
#include "xf86drm.h"
#include "etnaviv_drmif.h"
#include "etnaviv_drm.h"
#include <mem/vmem.h>
#include "esTransform.h"
#include <drivers/video/fb.h>

//#include "drm_setup.h"
#include "../cmdstream.xml.h"

#include "../state.xml.h"
#include "../state_3d.xml.h"
#include "../common.xml.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

extern void dcache_inval(const void *p, size_t size);
extern void dcache_flush(const void *p, size_t size);

void _nocache(uint32_t addr, int len) {
	for (uint32_t p = addr; p < addr + len + 4095; p+= 4096) {
		int ret;
		if ((ret = vmem_page_set_flags(vmem_current_context(), p, VMEM_PAGE_WRITABLE))) {
			if (0) printf("Page %p err %d\n", (void *) p, ret);
		}
	}
	mmu_flush_tlb();
}

extern int etnaviv_dmp(int id); /* Print debug information about GPU device */
static inline void etna_emit_load_state(struct etna_cmd_stream *stream,
		const uint16_t offset, const uint16_t count) {
	uint32_t v;

	v = 	(VIV_FE_LOAD_STATE_HEADER_OP_LOAD_STATE | VIV_FE_LOAD_STATE_HEADER_OFFSET(offset) |
			(VIV_FE_LOAD_STATE_HEADER_COUNT(count) & VIV_FE_LOAD_STATE_HEADER_COUNT__MASK));

	etna_cmd_stream_emit(stream, v);
}

static inline void etna_set_state(struct etna_cmd_stream *stream, uint32_t address, uint32_t value)
{
	etna_cmd_stream_reserve(stream, 2);
	etna_emit_load_state(stream, address >> 2, 1);
	etna_cmd_stream_emit(stream, value);
}

static inline void etna_set_state_from_bo(struct etna_cmd_stream *stream,
		uint32_t address, struct etna_bo *bo) {
	etna_cmd_stream_reserve(stream, 2);
	etna_emit_load_state(stream, address >> 2, 1);
	etna_cmd_stream_reloc(stream, &(struct etna_reloc){
			.bo = bo,
			.flags = ETNA_RELOC_READ,
			.offset = 0,
			});
}

static inline void etna_set_state_f32(struct etna_cmd_stream *commandBuffer, uint32_t address, float value)
{
    union {
        uint32_t i32;
        float f32;
    } x = { .f32 = value };
    etna_set_state(commandBuffer, address, x.i32);
}

static inline void
CMD_STALL(struct etna_cmd_stream *stream, uint32_t from, uint32_t to) {
	etna_cmd_stream_emit(stream, VIV_FE_STALL_HEADER_OP_STALL);
	etna_cmd_stream_emit(stream, VIV_FE_STALL_TOKEN_FROM(from) | VIV_FE_STALL_TOKEN_TO(to));
}

void etna_stall(struct etna_cmd_stream *stream, uint32_t from, uint32_t to) {
	etna_cmd_stream_reserve(stream, 4);

	etna_emit_load_state(stream, VIVS_GL_SEMAPHORE_TOKEN >> 2, 1);
	etna_cmd_stream_emit(stream, VIVS_GL_SEMAPHORE_TOKEN_FROM(from) | VIVS_GL_SEMAPHORE_TOKEN_TO(to));

	if (from == SYNC_RECIPIENT_FE) {
		/* if the frontend is to be stalled, queue a STALL frontend command */
		CMD_STALL(stream, from, to);
	} else {
		/* otherwise, load the STALL token state */
		etna_emit_load_state(stream, VIVS_GL_STALL_TOKEN >> 2, 1);
		etna_cmd_stream_emit(stream, VIVS_GL_STALL_TOKEN_FROM(from) | VIVS_GL_STALL_TOKEN_TO(to));
	}
}


static inline void etna_set_state_multi(struct etna_cmd_stream *commandBuffer, uint32_t base, uint32_t num, uint32_t *values)
{
    uint32_t *tgt = (uint32_t*)(commandBuffer->buffer + commandBuffer->offset);
    //printf("tft=%p, num = %d\n", tgt, num);
    tgt[0] = VIV_FE_LOAD_STATE_HEADER_OP_LOAD_STATE |
            VIV_FE_LOAD_STATE_HEADER_COUNT(num & 0x3ff) |
            VIV_FE_LOAD_STATE_HEADER_OFFSET(base >> 2);
    //printf("trace %d\n", __LINE__);
    memcpy(&tgt[1], values, 4*num);
    //printf("trace %d\n", __LINE__);
    commandBuffer->offset += (4 + num*4) / 4;
    //printf("trace %d\n", __LINE__);
    if(commandBuffer->offset & (4 / 4)) /* PAD */
    {
        commandBuffer->offset += 4 / 4;
    }
    //printf("trace %d\n", __LINE__);
}

static inline void etna_set_state_fixp(struct etna_cmd_stream *commandBuffer, uint32_t address, uint32_t value)
{
    uint32_t *tgt = (uint32_t*)(commandBuffer->buffer + commandBuffer->offset);
    tgt[0] = VIV_FE_LOAD_STATE_HEADER_OP_LOAD_STATE |
            VIV_FE_LOAD_STATE_HEADER_COUNT(1) |
            VIV_FE_LOAD_STATE_HEADER_FIXP |
            VIV_FE_LOAD_STATE_HEADER_OFFSET(address >> 2);
    tgt[1] = value;
    commandBuffer->offset += 8 / 4;
}


#include "cube_cmd.h"

#define VERTEX_BUFFER_SIZE 0x60000
static struct etna_device *dev;
static struct etna_pipe *_pipe;
static inline uint32_t
etna_align_up(uint32_t value, uint32_t granularity)
{
	return (value + (granularity - 1)) & (~(granularity - 1));
}


float vVertices[] = {
	// front
	-1.0f, -1.0f, +1.0f, // point blue
	+1.0f, -1.0f, +1.0f, // point magenta
	-1.0f, +1.0f, +1.0f, // point cyan
	+1.0f, +1.0f, +1.0f, // point white
	// back
	+1.0f, -1.0f, -1.0f, // point red
	-1.0f, -1.0f, -1.0f, // point black
	+1.0f, +1.0f, -1.0f, // point yellow
	-1.0f, +1.0f, -1.0f, // point green
	// right
	+1.0f, -1.0f, +1.0f, // point magenta
	+1.0f, -1.0f, -1.0f, // point red
	+1.0f, +1.0f, +1.0f, // point white
	+1.0f, +1.0f, -1.0f, // point yellow
	// left
	-1.0f, -1.0f, -1.0f, // point black
	-1.0f, -1.0f, +1.0f, // point blue
	-1.0f, +1.0f, -1.0f, // point green
	-1.0f, +1.0f, +1.0f, // point cyan
	// top
	-1.0f, +1.0f, +1.0f, // point cyan
	+1.0f, +1.0f, +1.0f, // point white
	-1.0f, +1.0f, -1.0f, // point green
	+1.0f, +1.0f, -1.0f, // point yellow
	// bottom
	-1.0f, -1.0f, -1.0f, // point black
	+1.0f, -1.0f, -1.0f, // point red
	-1.0f, -1.0f, +1.0f, // point blue
	+1.0f, -1.0f, +1.0f  // point magenta
};

float vColors[] = {
	// front
	0.0f,  0.0f,  1.0f, // blue
	1.0f,  0.0f,  1.0f, // magenta
	0.0f,  1.0f,  1.0f, // cyan
	1.0f,  1.0f,  1.0f, // white
	// back
	1.0f,  0.0f,  0.0f, // red
	0.0f,  0.0f,  0.0f, // black
	1.0f,  1.0f,  0.0f, // yellow
	0.0f,  1.0f,  0.0f, // green
	// right
	1.0f,  0.0f,  1.0f, // magenta
	1.0f,  0.0f,  0.0f, // red
	1.0f,  1.0f,  1.0f, // white
	1.0f,  1.0f,  0.0f, // yellow
	// left
	0.0f,  0.0f,  0.0f, // black
	0.0f,  0.0f,  1.0f, // blue
	0.0f,  1.0f,  0.0f, // green
	0.0f,  1.0f,  1.0f, // cyan
	// top
	0.0f,  1.0f,  1.0f, // cyan
	1.0f,  1.0f,  1.0f, // white
	0.0f,  1.0f,  0.0f, // green
	1.0f,  1.0f,  0.0f, // yellow
	// bottom
	0.0f,  0.0f,  0.0f, // black
	1.0f,  0.0f,  0.0f, // red
	0.0f,  0.0f,  1.0f, // blue
	1.0f,  0.0f,  1.0f  // magenta
};

float vNormals[] = {
	// front
	+0.0f, +0.0f, +1.0f, // forward
	+0.0f, +0.0f, +1.0f, // forward
	+0.0f, +0.0f, +1.0f, // forward
	+0.0f, +0.0f, +1.0f, // forward
	// back
	+0.0f, +0.0f, -1.0f, // backbard
	+0.0f, +0.0f, -1.0f, // backbard
	+0.0f, +0.0f, -1.0f, // backbard
	+0.0f, +0.0f, -1.0f, // backbard
	// right
	+1.0f, +0.0f, +0.0f, // right
	+1.0f, +0.0f, +0.0f, // right
	+1.0f, +0.0f, +0.0f, // right
	+1.0f, +0.0f, +0.0f, // right
	// left
	-1.0f, +0.0f, +0.0f, // left
	-1.0f, +0.0f, +0.0f, // left
	-1.0f, +0.0f, +0.0f, // left
	-1.0f, +0.0f, +0.0f, // left
	// top
	+0.0f, +1.0f, +0.0f, // up
	+0.0f, +1.0f, +0.0f, // up
	+0.0f, +1.0f, +0.0f, // up
	+0.0f, +1.0f, +0.0f, // up
	// bottom
	+0.0f, -1.0f, +0.0f, // down
	+0.0f, -1.0f, +0.0f, // down
	+0.0f, -1.0f, +0.0f, // down
	+0.0f, -1.0f, +0.0f  // down
};
#define COMPONENTS_PER_VERTEX (3)
#define NUM_VERTICES (6*4)

   /* Now load the shader itself */
    uint32_t vs[] = {
        0x01831009, 0x00000000, 0x00000000, 0x203fc048,
        0x02031009, 0x00000000, 0x00000000, 0x203fc058,
        0x07841003, 0x39000800, 0x00000050, 0x00000000,
        0x07841002, 0x39001800, 0x00aa0050, 0x00390048,
        0x07841002, 0x39002800, 0x01540050, 0x00390048,
        0x07841002, 0x39003800, 0x01fe0050, 0x00390048,
        0x03851003, 0x29004800, 0x000000d0, 0x00000000,
        0x03851002, 0x29005800, 0x00aa00d0, 0x00290058,
        0x03811002, 0x29006800, 0x015400d0, 0x00290058,
        0x07851003, 0x39007800, 0x00000050, 0x00000000,
        0x07851002, 0x39008800, 0x00aa0050, 0x00390058,
        0x07851002, 0x39009800, 0x01540050, 0x00390058,
        0x07801002, 0x3900a800, 0x01fe0050, 0x00390058,
        0x0401100c, 0x00000000, 0x00000000, 0x003fc008,
        0x03801002, 0x69000800, 0x01fe00c0, 0x00290038,
        0x03831005, 0x29000800, 0x01480040, 0x00000000,
        0x0383100d, 0x00000000, 0x00000000, 0x00000038,
        0x03801003, 0x29000800, 0x014801c0, 0x00000000,
        0x00801005, 0x29001800, 0x01480040, 0x00000000,
        0x0080108f, 0x3fc06800, 0x00000050, 0x203fc068,
        0x03801003, 0x00000800, 0x01480140, 0x00000000,
        0x04001009, 0x00000000, 0x00000000, 0x200000b8,
        0x02041001, 0x2a804800, 0x00000000, 0x003fc048,
        0x02041003, 0x2a804800, 0x00aa05c0, 0x00000002,
    };
    uint32_t ps[] = {
        0x00000000, 0x00000000, 0x00000000, 0x00000000
    };


static    struct etna_bo *rt = 0; /* main render target */
static    struct etna_bo *rt_ts = 0; /* tile status for main render target */
static    struct etna_bo *z = 0; /* depth for main render target */
static    struct etna_bo *z_ts = 0; /* depth ts for main render target */
static    struct etna_bo *vtx = 0; /* vertex buffer */
static    struct etna_bo *aux_rt = 0; /* auxilary render target */
static    struct etna_bo *aux_rt_ts = 0; /* tile status for auxilary render target */
static    int width = 1024 / 2;
static    int height = 768 / 2;

size_t rt_size;
size_t rt_ts_size;
size_t z_size;
size_t z_ts_size;
size_t aux_rt_size;
size_t aux_rt_ts_size;

size_t vs_size = sizeof(vs);

static void ins_nop(uint32_t *buf, int offt) {
	buf[offt]     = 0x18000000;
	buf[offt + 1] = 0x00000000;
}

static void ins_nopn(uint32_t *buf, int offt, int n) {
	for (int i = 0; i < n; i++) {
		buf[offt + i * 2]     = 0x18000000;
		buf[offt + 1 + i * 2] = 0x00000000;
	}
}

static int add_pipe_ops(uint32_t *buf, int len) {
	static int use_dither = 1;

	for (int i = 0; i < len; i++) {
		/* RS source addr */
		if (buf[i] == 0x08010582) {
			uint32_t arg = buf[i + 1];

			for (int j = len + 1; j >= i + 2; j--) {
				buf[j] = buf[j - 2];
			}

			buf[i + 2] = 0x080105b0; /* RS.PIPE_SOURCE */
			buf[i + 3] = arg;
			i += 3;
			len += 2;

			continue;
		}

		/* RS dest addr */
		if (buf[i] == 0x08010584) {
			uint32_t arg = buf[i + 1];

			for (int j = len + 1; j >= i + 2; j--) {
				buf[j] = buf[j - 2];
			}

			buf[i + 2] = 0x080105b8; /* RS.PIPE_DEST */
			buf[i + 3] = arg;
			i += 3;
			len += 2;

			continue;
		}

		/* PE color addr */
		if (buf[i] == 0x0801050c) {
			uint32_t arg = buf[i + 1];

			for (int j = len + 1; j >= i + 2; j--) {
				buf[j] = buf[j - 2];
			}

			buf[i + 2] = 0x08010518; /* PE.PIPE_COLOR */
			buf[i + 3] = arg;
			i += 3;
			len += 2;

			continue;
		}

		/* PE depth addr */
		if (buf[i] == 0x08010504) {
			uint32_t arg = buf[i + 1];

			for (int j = len + 1; j >= i + 2; j--) {
				buf[j] = buf[j - 2];
			}

			buf[i + 2] = 0x08010520; /* PE.PIPE_DEPTH */
			buf[i + 3] = arg;
			i += 3;
			len += 2;

			continue;
		}

		if (buf[i] == 0x08010590) {
			int n = 6;
			uint32_t tile = 0x55555555;//0xffffffff;//0x55555555;//0x000000f0;
			for (int j = len + n - 1 + (n % 2 ? 0 : 1); j >= i + n - 1; j--) {
				buf[j] = buf[j - n + 1 - (n % 2 ? 0 : 1)];
			}

			buf[i] = 0x08000590 | (((n % 2) ? n : n - 1) << 16);

			for (int j = 1; j <= n; j ++) {
				buf[i + j] = tile;
			}
			if (1) {
			//buf[i + 1] = 0xffff0000;
			//buf[i + 2] = 0xff00ff00;
			//buf[i + 3] = 0xff0000ff;
			buf[i + 4] = 0xffff00ff;

			if (0) buf[i + 2] = tile;
			tile = 0;
			}
			len += n - 1;

			if (!(n % 2)) {
				buf[i + n] = 0x08010590;
				buf[i + n + 1] = tile;
				len++;
				i++;
			}

			i += n;

			continue;
		}

		if (buf[i] == 0x0801058c || buf[i] == 0x0801058d) {
			buf[i + 1] = use_dither * 0xffffffff;
			i++;
			continue;
		}

		if (buf[i] == 0x0802058c) {
			buf[i + 1] = buf[i + 2] = use_dither * 0xffffffff;
			i += 2;
			continue;
		}

		if (buf[i] == 0x0801050b) { /* PE_COLOR_FORMAT */
			//buf[i + 1] |= (1 << 20); // non-supertiled
			i++;
			continue;
		}

		if (buf[i] == 0x08010500) {
			//buf[i + 1] |= (1 << 26); // non-supertiled
			i++;
			continue;
		}

		if (buf[i] == 0x08010583 || buf[i] == 0x08010585) {
			//buf[i + 1] |= (1 << 31); // non-supertiled
			i++;
			continue;
		}

		if (buf[i] == 0x08010e03) { /* FLush cache */
			buf[i + 1] = 3;
		}
	}

	return len;
}

static void wait_tick(void) {
	int t = 0xffffff;
	while(t--);
}

static void stall_tick(void) {
	int cnt = 0;
	while(1) {
		wait_tick();
		printf("cnt=%d\n", cnt++);
	}
}

void nonzero_lookup_raw(void *_ptr, int len, const char *name) {
	int *ptr = _ptr;
	int cnt = 0;
	printf("Look for non-zero elements for %s\n", name ? name : "");
	dcache_inval(ptr, len);
	for (int i = 0; i < len / 4; i++, ptr++) {
		if (*ptr != 0) {
			cnt++;
			printf("%s non-zero %d=%08x\n", name ? name : "", i, *ptr);
			if (cnt > 32) {
				printf("...\n");
				return;
			}
		}
	}
}

void nonzero_lookup(struct etna_bo *stream, int len, const char *name) {
	nonzero_lookup_raw(etna_bo_map(stream), len, name);
}

uint32_t z_physical, bmp_physical;
uint32_t rt_physical, rt_ts_physical, z_ts_physical, aux_rt_ts_physical, aux_rt_physical, vtx_physical;
int padded_width, padded_height;
int bits_per_tile;

/* macro for MASKED() (multiple can be &ed) */
#define VIV_MASKED(NAME, VALUE) (~(NAME ## _MASK | NAME ## __MASK) | ((VALUE)<<(NAME ## __SHIFT)))
/* for boolean bits */
#define VIV_MASKED_BIT(NAME, VALUE) (~(NAME ## _MASK | NAME) | ((VALUE) ? NAME : 0))
/* for inline enum bit fields
 * XXX in principle headergen could simply generate these fields prepackaged
 */
#define VIV_MASKED_INL(NAME, VALUE) (~(NAME ## _MASK | NAME ## __MASK) | (NAME ## _ ## VALUE))



#define ETNA_FLUSH(stream) \
	etna_set_state((stream), VIVS_GL_FLUSH_CACHE, \
			VIVS_GL_FLUSH_CACHE_DEPTH | \
			VIVS_GL_FLUSH_CACHE_COLOR)
#define VIVS_FE_VERTEX_STREAM_CONTROL_VERTEX_STRIDE__SHIFT FE_VERTEX_STREAM_CONTROL_VERTEX_STRIDE__SHIFT
//#define VIVS_FE_VERTEX_ELEMENT_CONFIG_ENDIAN__SHIFT FE_VERTEX_ELEMENT_CONFIG_ENDIAN__SHIFT
	int done = 10;
static void etna_warm_up_rs(struct etna_cmd_stream *cmdPtr, uint32_t aux_rt_physical, uint32_t aux_rt_ts_physical)
{
    etna_set_state(cmdPtr, VIVS_TS_COLOR_STATUS_BASE, aux_rt_ts_physical); /* ADDR_G */
    etna_set_state(cmdPtr, VIVS_TS_COLOR_SURFACE_BASE, aux_rt_physical); /* ADDR_F */
    etna_set_state(cmdPtr, VIVS_TS_FLUSH_CACHE, VIVS_TS_FLUSH_CACHE_FLUSH);
    etna_set_state(cmdPtr, VIVS_RS_CONFIG,  /* wut? */
            VIVS_RS_CONFIG_SOURCE_FORMAT(RS_FORMAT_A8R8G8B8) |
            VIVS_RS_CONFIG_SOURCE_TILED |
            VIVS_RS_CONFIG_DEST_FORMAT(RS_FORMAT_R5G6B5) |
            VIVS_RS_CONFIG_DEST_TILED);
    etna_set_state(cmdPtr, VIVS_RS_SOURCE_ADDR, aux_rt_physical); /* ADDR_F */
    etna_set_state(cmdPtr, VIVS_RS_SOURCE_STRIDE, 0x400 * 2);
    etna_set_state(cmdPtr, VIVS_RS_PIPE_SOURCE_ADDR(0), aux_rt_physical); /* ADDR_F */
    etna_set_state(cmdPtr, VIVS_RS_DEST_ADDR, aux_rt_physical); /* ADDR_F */
    etna_set_state(cmdPtr, VIVS_RS_DEST_STRIDE, 0x400 * 2);
    etna_set_state(cmdPtr, VIVS_RS_PIPE_DEST_ADDR(0), aux_rt_physical); /* ADDR_F */
    etna_set_state(cmdPtr, VIVS_RS_WINDOW_SIZE,
            VIVS_RS_WINDOW_SIZE_HEIGHT(4) |
            VIVS_RS_WINDOW_SIZE_WIDTH(16));
    etna_set_state(cmdPtr, VIVS_RS_CLEAR_CONTROL, VIVS_RS_CLEAR_CONTROL_MODE_DISABLED);
    etna_set_state(cmdPtr, VIVS_RS_KICKER, 0xbeebbeeb);

    done += 11 * 2;
}

static inline void etna_draw_primitives(struct etna_cmd_stream *cmdPtr, uint32_t primitive_type, uint32_t start, uint32_t count)
{
    uint32_t *tgt = (uint32_t*)(cmdPtr->buffer + cmdPtr->offset);
    tgt[0] = VIV_FE_DRAW_PRIMITIVES_HEADER_OP_DRAW_PRIMITIVES;
    tgt[1] = primitive_type;
    tgt[2] = start;
    tgt[3] = count;
    cmdPtr->offset += 16 / 4;
}


uint32_t buffer_memory[0x10][1024];

static void draw_cube(void) {
	struct etna_cmd_stream *stream, *cmdPtr;
	static int _cs=0;
	stream = etna_cmd_stream_new(_pipe, 0x1000, NULL, NULL);

	cmdPtr = stream;
 printf("trace %d\n", __LINE__);
	for (int i = 0; i < 100; i++) {
		etna_cmd_stream_emit(cmdPtr, VIV_FE_NOP_HEADER_OP_NOP);
		etna_cmd_stream_emit(cmdPtr, 0);
	}

#if 0
    etna_set_state(cmdPtr, VIVS_RS_CLEAR_CONTROL,
            VIVS_RS_CLEAR_CONTROL_MODE_ENABLED1 |
            (0xffff << VIVS_RS_CLEAR_CONTROL_BITS__SHIFT));
    etna_set_state(cmdPtr, VIVS_RS_KICKER,
            0xbeebbeeb);

    etna_set_state(cmdPtr, VIVS_RS_CLEAR_CONTROL,
            VIVS_RS_CLEAR_CONTROL_MODE_ENABLED4 |
            (0xffff << VIVS_RS_CLEAR_CONTROL_BITS__SHIFT));
    etna_set_state(cmdPtr, VIVS_RS_KICKER,
            0xbeebbeeb);
    etna_set_state(cmdPtr, VIVS_RS_CLEAR_CONTROL,
            VIVS_RS_CLEAR_CONTROL_MODE_ENABLED4_2 |
            (0xffff << VIVS_RS_CLEAR_CONTROL_BITS__SHIFT));
    etna_set_state(cmdPtr, VIVS_RS_KICKER,
            0xbeebbeeb);
#endif

   printf("trace %d\n", __LINE__);
    etna_set_state(cmdPtr, VIVS_GL_VERTEX_ELEMENT_CONFIG, 0x1);
    etna_set_state(cmdPtr, VIVS_RA_CONTROL, 0x1);

    etna_set_state(cmdPtr, VIVS_PA_W_CLIP_LIMIT, 0x34000001);
    etna_set_state(cmdPtr, VIVS_PA_SYSTEM_MODE, 0x11);
    etna_set_state(cmdPtr, VIVS_PA_CONFIG, ~((3 << 22)));
    etna_set_state(cmdPtr, VIVS_SE_CONFIG, 0x0);
    etna_set_state(cmdPtr, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_COLOR | VIVS_GL_FLUSH_CACHE_DEPTH);
    etna_set_state(cmdPtr, VIVS_PE_COLOR_FORMAT,
            VIV_MASKED_BIT(VIVS_PE_COLOR_FORMAT_OVERWRITE, 0));
    etna_set_state(cmdPtr, VIVS_PE_ALPHA_CONFIG, /* can & all these together */
            VIV_MASKED_BIT(VIVS_PE_ALPHA_CONFIG_BLEND_ENABLE_COLOR, 0) &
            VIV_MASKED_BIT(VIVS_PE_ALPHA_CONFIG_BLEND_SEPARATE_ALPHA, 0));
    etna_set_state(cmdPtr, VIVS_PE_ALPHA_CONFIG,
            VIV_MASKED(VIVS_PE_ALPHA_CONFIG_SRC_FUNC_COLOR, BLEND_FUNC_ONE) &
            VIV_MASKED(VIVS_PE_ALPHA_CONFIG_SRC_FUNC_ALPHA, BLEND_FUNC_ONE));
    etna_set_state(cmdPtr, VIVS_PE_ALPHA_CONFIG,
            VIV_MASKED(VIVS_PE_ALPHA_CONFIG_DST_FUNC_COLOR, BLEND_FUNC_ZERO) &
            VIV_MASKED(VIVS_PE_ALPHA_CONFIG_DST_FUNC_ALPHA, BLEND_FUNC_ZERO));
    etna_set_state(cmdPtr, VIVS_PE_ALPHA_CONFIG,
            VIV_MASKED(VIVS_PE_ALPHA_CONFIG_EQ_COLOR, BLEND_EQ_ADD) &
            VIV_MASKED(VIVS_PE_ALPHA_CONFIG_EQ_ALPHA, BLEND_EQ_ADD));
    etna_set_state(cmdPtr, VIVS_PE_ALPHA_BLEND_COLOR,
            (0 << VIVS_PE_ALPHA_BLEND_COLOR_B__SHIFT) |
            (0 << VIVS_PE_ALPHA_BLEND_COLOR_G__SHIFT) |
            (0 << VIVS_PE_ALPHA_BLEND_COLOR_R__SHIFT) |
            (0 << VIVS_PE_ALPHA_BLEND_COLOR_A__SHIFT));

 printf("trace %d\n", __LINE__);
    etna_set_state(cmdPtr, VIVS_PE_ALPHA_OP, VIV_MASKED_BIT(VIVS_PE_ALPHA_OP_ALPHA_TEST, 0));
    etna_set_state(cmdPtr, VIVS_PA_CONFIG, VIV_MASKED_INL(VIVS_PA_CONFIG_CULL_FACE_MODE, OFF));
    etna_set_state(cmdPtr, VIVS_PE_DEPTH_CONFIG, VIV_MASKED_BIT(VIVS_PE_DEPTH_CONFIG_WRITE_ENABLE, 0));
    etna_set_state(cmdPtr, VIVS_PE_STENCIL_CONFIG, VIV_MASKED(VIVS_PE_STENCIL_CONFIG_REF_FRONT, 0));
    etna_set_state(cmdPtr, VIVS_PE_STENCIL_OP, VIV_MASKED(VIVS_PE_STENCIL_OP_FUNC_FRONT, COMPARE_FUNC_ALWAYS));
    etna_set_state(cmdPtr, VIVS_PE_STENCIL_OP, VIV_MASKED(VIVS_PE_STENCIL_OP_FUNC_BACK, COMPARE_FUNC_ALWAYS));
    etna_set_state(cmdPtr, VIVS_PE_STENCIL_CONFIG, VIV_MASKED(VIVS_PE_STENCIL_CONFIG_MASK_FRONT, 0xff));
    etna_set_state(cmdPtr, VIVS_PE_STENCIL_CONFIG, 0xffffff7f); //VIV_MASKED(VIVS_PE_STENCIL_CONFIG_WRITE_MASK, 0xff));
    etna_set_state(cmdPtr, VIVS_PE_STENCIL_OP, VIV_MASKED(VIVS_PE_STENCIL_OP_FAIL_FRONT, STENCIL_OP_KEEP));
    etna_set_state(cmdPtr, VIVS_PE_DEPTH_CONFIG, VIV_MASKED_BIT(VIVS_PE_DEPTH_CONFIG_EARLY_Z, 0));
    etna_set_state(cmdPtr, VIVS_PE_STENCIL_OP, VIV_MASKED(VIVS_PE_STENCIL_OP_FAIL_BACK, STENCIL_OP_KEEP));
    etna_set_state(cmdPtr, VIVS_PE_DEPTH_CONFIG, VIV_MASKED_BIT(VIVS_PE_DEPTH_CONFIG_EARLY_Z, 0));
    etna_set_state(cmdPtr, VIVS_PE_STENCIL_OP, VIV_MASKED(VIVS_PE_STENCIL_OP_DEPTH_FAIL_FRONT, STENCIL_OP_KEEP));
    etna_set_state(cmdPtr, VIVS_PE_DEPTH_CONFIG, VIV_MASKED_BIT(VIVS_PE_DEPTH_CONFIG_EARLY_Z, 0));
    etna_set_state(cmdPtr, VIVS_PE_STENCIL_OP, VIV_MASKED(VIVS_PE_STENCIL_OP_DEPTH_FAIL_BACK, STENCIL_OP_KEEP));
    etna_set_state(cmdPtr, VIVS_PE_DEPTH_CONFIG, VIV_MASKED_BIT(VIVS_PE_DEPTH_CONFIG_EARLY_Z, 0));
    etna_set_state(cmdPtr, VIVS_PE_STENCIL_OP, VIV_MASKED(VIVS_PE_STENCIL_OP_PASS_FRONT, STENCIL_OP_KEEP));
    etna_set_state(cmdPtr, VIVS_PE_DEPTH_CONFIG, VIV_MASKED_BIT(VIVS_PE_DEPTH_CONFIG_EARLY_Z, 0));
    etna_set_state(cmdPtr, VIVS_PE_STENCIL_OP, VIV_MASKED(VIVS_PE_STENCIL_OP_PASS_BACK, STENCIL_OP_KEEP));
    etna_set_state(cmdPtr, VIVS_PE_DEPTH_CONFIG, VIV_MASKED_BIT(VIVS_PE_DEPTH_CONFIG_EARLY_Z, 0));
    etna_set_state(cmdPtr, VIVS_PE_COLOR_FORMAT, VIV_MASKED(VIVS_PE_COLOR_FORMAT_COMPONENTS, 0xf));

	done += 34 * 2;

 printf("trace %d\n", __LINE__);
    etna_set_state(cmdPtr, VIVS_SE_DEPTH_BIAS, 0x0);

    etna_set_state(cmdPtr, VIVS_PA_CONFIG, VIV_MASKED_INL(VIVS_PA_CONFIG_FILL_MODE, SOLID));
    etna_set_state(cmdPtr, VIVS_PA_CONFIG, VIV_MASKED_INL(VIVS_PA_CONFIG_SHADE_MODEL, SMOOTH));
    etna_set_state(cmdPtr, VIVS_PE_COLOR_FORMAT,
            VIV_MASKED(VIVS_PE_COLOR_FORMAT_FORMAT, RS_FORMAT_X8R8G8B8) &
            VIV_MASKED_BIT(VIVS_PE_COLOR_FORMAT_SUPER_TILED, 1));

 printf("trace %d\n", __LINE__);
    done += 8;
    etna_set_state(cmdPtr, VIVS_PE_COLOR_ADDR, rt_physical); /* ADDR_A */
    etna_set_state(cmdPtr, VIVS_PE_PIPE_COLOR_ADDR(0), rt_physical); /* ADDR_A */
 printf("trace %d\n", __LINE__);
    etna_set_state(cmdPtr, VIVS_PE_COLOR_STRIDE, padded_width * 4);
 printf("trace %d\n", __LINE__);
    etna_set_state(cmdPtr, VIVS_GL_MULTI_SAMPLE_CONFIG,
            VIV_MASKED_INL(VIVS_GL_MULTI_SAMPLE_CONFIG_MSAA_SAMPLES, NONE) &
            VIV_MASKED(VIVS_GL_MULTI_SAMPLE_CONFIG_MSAA_ENABLES, 0xf) &
            VIV_MASKED(VIVS_GL_MULTI_SAMPLE_CONFIG_UNK12, 0x0) &
            VIV_MASKED(VIVS_GL_MULTI_SAMPLE_CONFIG_UNK16, 0x0)
            );
 printf("trace %d\n", __LINE__);
    done += 6;
 printf("trace %d\n", __LINE__);
if (1) {
    etna_set_state(cmdPtr, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_COLOR);
    etna_set_state(cmdPtr, VIVS_PE_COLOR_FORMAT, VIV_MASKED_BIT(VIVS_PE_COLOR_FORMAT_OVERWRITE, 1));
    etna_set_state(cmdPtr, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_COLOR);
    etna_set_state(cmdPtr, VIVS_TS_COLOR_CLEAR_VALUE, 0);
    etna_set_state(cmdPtr, VIVS_TS_COLOR_STATUS_BASE, rt_ts_physical); /* ADDR_B */
    etna_set_state(cmdPtr, VIVS_TS_COLOR_SURFACE_BASE, rt_physical); /* ADDR_A */
    etna_set_state(cmdPtr, VIVS_TS_MEM_CONFIG, VIVS_TS_MEM_CONFIG_COLOR_FAST_CLEAR);

 printf("trace %d\n", __LINE__);
    etna_set_state(cmdPtr, VIVS_SE_DEPTH_SCALE, 0x0);
    done += 10 * 2;
    etna_set_state(cmdPtr, VIVS_PE_DEPTH_CONFIG,
            VIV_MASKED_INL(VIVS_PE_DEPTH_CONFIG_DEPTH_FORMAT, D16) &
            VIV_MASKED_BIT(VIVS_PE_DEPTH_CONFIG_SUPER_TILED, 1)
            );
    etna_set_state(cmdPtr, VIVS_PE_DEPTH_ADDR, z_physical); /* ADDR_C */
    etna_set_state(cmdPtr, VIVS_PE_PIPE_DEPTH_ADDR(0), z_physical); /* ADDR_C */
    etna_set_state(cmdPtr, VIVS_PE_DEPTH_STRIDE, padded_width * 2);
    etna_set_state(cmdPtr, VIVS_PE_STENCIL_CONFIG, VIV_MASKED_INL(VIVS_PE_STENCIL_CONFIG_MODE, DISABLED));
    etna_set_state(cmdPtr, VIVS_PE_HDEPTH_CONTROL, VIVS_PE_HDEPTH_CONTROL_FORMAT_DISABLED);
 printf("trace %d\n", __LINE__);
    etna_set_state_f32(cmdPtr, VIVS_PE_DEPTH_NORMALIZE, 65535.0);
 printf("trace %d\n", __LINE__);
    etna_set_state(cmdPtr, VIVS_PE_DEPTH_CONFIG, VIV_MASKED_BIT(VIVS_PE_DEPTH_CONFIG_EARLY_Z, 0));
    etna_set_state(cmdPtr, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_DEPTH);

 printf("trace %d\n", __LINE__);
    etna_set_state(cmdPtr, VIVS_TS_DEPTH_CLEAR_VALUE, 0xffffffff);
    etna_set_state(cmdPtr, VIVS_TS_DEPTH_STATUS_BASE, z_ts_physical); /* ADDR_D */
    etna_set_state(cmdPtr, VIVS_TS_DEPTH_SURFACE_BASE, z_physical); /* ADDR_C */
    etna_set_state(cmdPtr, VIVS_TS_MEM_CONFIG,
            VIVS_TS_MEM_CONFIG_DEPTH_FAST_CLEAR |
            VIVS_TS_MEM_CONFIG_COLOR_FAST_CLEAR |
            VIVS_TS_MEM_CONFIG_DEPTH_16BPP |
            VIVS_TS_MEM_CONFIG_DEPTH_COMPRESSION);
    etna_set_state(cmdPtr, VIVS_PE_DEPTH_CONFIG, VIV_MASKED_BIT(VIVS_PE_DEPTH_CONFIG_EARLY_Z, 1)); /* flip-flopping once again */

 printf("trace %d\n", __LINE__);
    done += 28 * 2;
    /* Warm up RS on aux render target */
    etna_set_state(cmdPtr, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_COLOR | VIVS_GL_FLUSH_CACHE_DEPTH);
    etna_warm_up_rs(cmdPtr, aux_rt_physical, aux_rt_ts_physical);

 printf("trace %d\n", __LINE__);
    /* Phew, now that's one hell of a setup; the serious rendering starts now */
    etna_set_state(cmdPtr, VIVS_TS_COLOR_STATUS_BASE, rt_ts_physical); /* ADDR_B */
    etna_set_state(cmdPtr, VIVS_TS_COLOR_SURFACE_BASE, rt_physical); /* ADDR_A */

    /* ... or so we thought */
 printf("trace %d\n", __LINE__);
    etna_set_state(cmdPtr, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_COLOR | VIVS_GL_FLUSH_CACHE_DEPTH);
    etna_warm_up_rs(cmdPtr, aux_rt_physical, aux_rt_ts_physical);


    done += 4 * 2;
}


 printf("trace %d\n", __LINE__);
    /* maybe now? */
    etna_set_state(cmdPtr, VIVS_TS_COLOR_STATUS_BASE, rt_ts_physical); /* ADDR_B */
    etna_set_state(cmdPtr, VIVS_TS_COLOR_SURFACE_BASE, rt_physical); /* ADDR_A */
    etna_set_state(cmdPtr, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_COLOR | VIVS_GL_FLUSH_CACHE_DEPTH);

 printf("trace %d\n", __LINE__);
    /* nope, not really... */
    etna_set_state(cmdPtr, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_COLOR | VIVS_GL_FLUSH_CACHE_DEPTH);
    etna_warm_up_rs(cmdPtr, aux_rt_physical, aux_rt_ts_physical);
    etna_set_state(cmdPtr, VIVS_TS_COLOR_STATUS_BASE, rt_ts_physical); /* ADDR_B */
    etna_set_state(cmdPtr, VIVS_TS_COLOR_SURFACE_BASE, rt_physical); /* ADDR_A */

 printf("trace %d\n", __LINE__);
    /* semaphore time */
    etna_set_state(cmdPtr, VIVS_GL_SEMAPHORE_TOKEN,
            (SYNC_RECIPIENT_RA<<VIVS_GL_SEMAPHORE_TOKEN_FROM__SHIFT)|
            (SYNC_RECIPIENT_PE<<VIVS_GL_SEMAPHORE_TOKEN_TO__SHIFT)
            );
    etna_set_state(cmdPtr, VIVS_GL_STALL_TOKEN,
            (SYNC_RECIPIENT_RA<<VIVS_GL_STALL_TOKEN_FROM__SHIFT)|
            (SYNC_RECIPIENT_PE<<VIVS_GL_STALL_TOKEN_TO__SHIFT)
            );

 printf("trace %d\n", __LINE__);
    /* Set up the resolve to clear tile status for main render target
     * Regard the TS as an image of width 16 with 4 bytes per pixel (64 bytes per row)
     * XXX need to clear the depth ts too.
     * */
        etna_set_state(cmdPtr, VIVS_TS_MEM_CONFIG, 0);
    etna_set_state(cmdPtr, VIVS_RS_CONFIG,
            (RS_FORMAT_A8R8G8B8 << VIVS_RS_CONFIG_SOURCE_FORMAT__SHIFT) |
            (RS_FORMAT_A8R8G8B8 << VIVS_RS_CONFIG_DEST_FORMAT__SHIFT)
            );
 printf("trace %d\n", __LINE__);
    etna_set_state_multi(cmdPtr, VIVS_RS_DITHER(0), 2, (uint32_t[]){0xffffffff, 0xffffffff});
 printf("trace %d\n", __LINE__);
    etna_set_state(cmdPtr, VIVS_RS_DEST_ADDR, rt_ts_physical); /* ADDR_B */
    etna_set_state(cmdPtr, VIVS_RS_PIPE_DEST_ADDR(0), rt_ts_physical); /* ADDR_B */
 printf("trace %d\n", __LINE__);
    etna_set_state(cmdPtr, VIVS_RS_DEST_STRIDE, 0x40);
 printf("trace %d\n", __LINE__);
    etna_set_state(cmdPtr, VIVS_RS_WINDOW_SIZE,
            ((rt_ts_size/0x40) << VIVS_RS_WINDOW_SIZE_HEIGHT__SHIFT) |
            (16 << VIVS_RS_WINDOW_SIZE_WIDTH__SHIFT));
    etna_set_state(cmdPtr, VIVS_RS_FILL_VALUE(0), 0x55555555);
 printf("trace %d\n", __LINE__);
    etna_set_state(cmdPtr, VIVS_RS_CLEAR_CONTROL,
            VIVS_RS_CLEAR_CONTROL_MODE_ENABLED1 |
            (0xffff << VIVS_RS_CLEAR_CONTROL_BITS__SHIFT));
    etna_set_state(cmdPtr, VIVS_RS_EXTRA_CONFIG,
            0); /* no AA, no endian switch */
    etna_set_state(cmdPtr, VIVS_RS_KICKER,
            0xbeebbeeb);

 printf("trace %d\n", __LINE__);
    etna_set_state(cmdPtr, VIVS_TS_COLOR_CLEAR_VALUE, 0xff1f1f1f);
    etna_set_state(cmdPtr, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_COLOR);
    etna_set_state(cmdPtr, VIVS_TS_COLOR_CLEAR_VALUE, 0xff1f1f1f);
    etna_set_state(cmdPtr, VIVS_TS_COLOR_STATUS_BASE, rt_ts_physical); /* ADDR_B */
    etna_set_state(cmdPtr, VIVS_TS_COLOR_SURFACE_BASE, rt_physical); /* ADDR_A */
    etna_set_state(cmdPtr, VIVS_TS_MEM_CONFIG,
            VIVS_TS_MEM_CONFIG_DEPTH_FAST_CLEAR |
            VIVS_TS_MEM_CONFIG_COLOR_FAST_CLEAR |
            VIVS_TS_MEM_CONFIG_DEPTH_16BPP |
            VIVS_TS_MEM_CONFIG_DEPTH_COMPRESSION);
    etna_set_state(cmdPtr, VIVS_PA_CONFIG, VIV_MASKED_INL(VIVS_PA_CONFIG_CULL_FACE_MODE, CCW));
    etna_set_state(cmdPtr, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_COLOR | VIVS_GL_FLUSH_CACHE_DEPTH);
    etna_set_state(cmdPtr, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_COLOR | VIVS_GL_FLUSH_CACHE_DEPTH);
    etna_set_state(cmdPtr, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_COLOR | VIVS_GL_FLUSH_CACHE_DEPTH);
    etna_set_state(cmdPtr, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_COLOR | VIVS_GL_FLUSH_CACHE_DEPTH);

 printf("trace %d\n", __LINE__);
    etna_set_state(cmdPtr, VIVS_PE_DEPTH_CONFIG, VIV_MASKED_BIT(VIVS_PE_DEPTH_CONFIG_WRITE_ENABLE, 0));
    etna_set_state(cmdPtr, VIVS_PE_DEPTH_CONFIG, VIV_MASKED_INL(VIVS_PE_DEPTH_CONFIG_DEPTH_MODE, NONE));
    etna_set_state(cmdPtr, VIVS_PE_DEPTH_CONFIG, VIV_MASKED_BIT(VIVS_PE_DEPTH_CONFIG_WRITE_ENABLE, 0));
    etna_set_state(cmdPtr, VIVS_PE_DEPTH_CONFIG, VIV_MASKED(VIVS_PE_DEPTH_CONFIG_DEPTH_FUNC, COMPARE_FUNC_ALWAYS));
 printf("trace %d\n", __LINE__);
    etna_set_state(cmdPtr, VIVS_PE_DEPTH_CONFIG, VIV_MASKED_INL(VIVS_PE_DEPTH_CONFIG_DEPTH_MODE, Z));
 printf("trace %d\n", __LINE__);
    etna_set_state_f32(cmdPtr, VIVS_PE_DEPTH_NEAR, 0.0);
 printf("trace %d\n", __LINE__);
    etna_set_state_f32(cmdPtr, VIVS_PE_DEPTH_FAR, 1.0);
 printf("trace %d\n", __LINE__);
    etna_set_state_f32(cmdPtr, VIVS_PE_DEPTH_NORMALIZE, 65535.0);
 printf("trace %d\n", __LINE__);


    /* set up primitive assembly */
    etna_set_state_f32(cmdPtr, VIVS_PA_VIEWPORT_OFFSET_Z, 0.0);
 printf("trace %d\n", __LINE__);
    etna_set_state_f32(cmdPtr, VIVS_PA_VIEWPORT_SCALE_Z, 1.0);
 printf("trace %d\n", __LINE__);
    etna_set_state(cmdPtr, VIVS_PE_DEPTH_CONFIG, VIV_MASKED_BIT(VIVS_PE_DEPTH_CONFIG_ONLY_DEPTH, 0));
 printf("trace %d\n", __LINE__);
    etna_set_state_fixp(cmdPtr, VIVS_PA_VIEWPORT_OFFSET_X, width << 15);
 printf("trace %d\n", __LINE__);
    etna_set_state_fixp(cmdPtr, VIVS_PA_VIEWPORT_OFFSET_Y, (height << 16));
 printf("trace %d\n", __LINE__);
    etna_set_state_fixp(cmdPtr, VIVS_PA_VIEWPORT_SCALE_X, width << 15);
 printf("trace %d\n", __LINE__);
    etna_set_state_fixp(cmdPtr, VIVS_PA_VIEWPORT_SCALE_Y, height << 16);
    etna_set_state_fixp(cmdPtr, VIVS_SE_SCISSOR_LEFT, 0);
    etna_set_state_fixp(cmdPtr, VIVS_SE_SCISSOR_TOP, 0);
    etna_set_state_fixp(cmdPtr, VIVS_SE_SCISSOR_RIGHT, (width << 16) | 5);
    etna_set_state_fixp(cmdPtr, VIVS_SE_SCISSOR_BOTTOM, (height << 18) | 5);

 printf("trace %d\n", __LINE__);
    /* shader setup */
#if 1
    etna_set_state(cmdPtr, VIVS_VS_END_PC, vs_size / 16);
    etna_set_state_multi(cmdPtr, VIVS_VS_INPUT_COUNT, 3, (uint32_t[]){
            /* VIVS_VS_INPUT_COUNT */ (1<<8) | 3,
            /* VIVS_VS_TEMP_REGISTER_CONTROL */ 6 << VIVS_VS_TEMP_REGISTER_CONTROL_NUM_TEMPS__SHIFT,
            /* VIVS_VS_OUTPUT(0) */ 4});
   // etna_set_state(cmdPtr, VIVS_VS_RANGE, 0x0 + (0x15 << 16));
    etna_set_state(cmdPtr, VIVS_VS_START_PC, 0x0);
    etna_set_state_f32(cmdPtr, VIVS_VS_UNIFORMS(45), 0.5); /* u11.y */
    etna_set_state_f32(cmdPtr, VIVS_VS_UNIFORMS(44), 1.0); /* u11.x */
    etna_set_state_f32(cmdPtr, VIVS_VS_UNIFORMS(27), 0.0); /* u6.w */
    etna_set_state_f32(cmdPtr, VIVS_VS_UNIFORMS(23), 20.0); /* u5.w */
    etna_set_state_f32(cmdPtr, VIVS_VS_UNIFORMS(19), 2.0); /* u4.w */
#endif


    etna_set_state_multi(cmdPtr, VIVS_VS_INST_MEM(0), vs_size/4, vs);
    etna_set_state(cmdPtr, VIVS_RA_CONTROL, 0x1);
    etna_set_state_multi(cmdPtr, VIVS_PS_END_PC, 2, (uint32_t[]){
            /* VIVS_PS_END_PC */ 0x1,
            /* VIVS_PS_OUTPUT_REG */ 0x1});
    etna_set_state(cmdPtr, VIVS_PS_START_PC, 0x0);
    etna_set_state(cmdPtr, VIVS_PA_SHADER_ATTRIBUTES(0), 0x200);
    etna_set_state(cmdPtr, VIVS_GL_VARYING_NUM_COMPONENTS,  /* one varying, with four components */
            (4 << VIVS_GL_VARYING_NUM_COMPONENTS_VAR0__SHIFT)
            );
    etna_set_state_multi(cmdPtr, VIVS_GL_VARYING_COMPONENT_USE(0), 2, (uint32_t[]){ /* one varying, with four components */
            (VARYING_COMPONENT_USE_USED << VIVS_GL_VARYING_COMPONENT_USE_COMP0__SHIFT) |
            (VARYING_COMPONENT_USE_USED << VIVS_GL_VARYING_COMPONENT_USE_COMP1__SHIFT) |
            (VARYING_COMPONENT_USE_USED << VIVS_GL_VARYING_COMPONENT_USE_COMP2__SHIFT) |
            (VARYING_COMPONENT_USE_USED << VIVS_GL_VARYING_COMPONENT_USE_COMP3__SHIFT)
            , 0
            });
    etna_set_state_multi(cmdPtr, VIVS_PS_INST_MEM(0), sizeof(ps)/4, ps);
    etna_set_state(cmdPtr, VIVS_PS_INPUT_COUNT, (31<<8)|2);
    etna_set_state(cmdPtr, VIVS_PS_TEMP_REGISTER_CONTROL,
            (2 << VIVS_PS_TEMP_REGISTER_CONTROL_NUM_TEMPS__SHIFT));
    etna_set_state(cmdPtr, VIVS_PS_CONTROL,
            VIVS_PS_CONTROL_UNK1
            );
    etna_set_state(cmdPtr, VIVS_PA_ATTRIBUTE_ELEMENT_COUNT, 0x100);
    etna_set_state(cmdPtr, VIVS_GL_VARYING_TOTAL_COMPONENTS,  /* one varying, with four components */
            (4 << VIVS_GL_VARYING_TOTAL_COMPONENTS_NUM__SHIFT)
            );
    etna_set_state(cmdPtr, VIVS_VS_LOAD_BALANCING, 0xf3f0582);
    etna_set_state(cmdPtr, VIVS_VS_OUTPUT_COUNT, 2);
    etna_set_state(cmdPtr, VIVS_PA_CONFIG, VIV_MASKED_BIT(VIVS_PA_CONFIG_POINT_SIZE_ENABLE, 0));


    /*   Compute transform matrices in the same way as cube egl demo */
    ESMatrix modelview;
    esMatrixLoadIdentity(&modelview);
    esTranslate(&modelview, 0.0f, 0.0f, -8.0f);
    esRotate(&modelview, 45.0f, 1.0f, 0.0f, 0.0f);
    esRotate(&modelview, 45.0f, 0.0f, 1.0f, 0.0f);
    static int frame = 0;
    esRotate(&modelview, 5.0f * frame, 0.0f, 0.0f, 1.0f);
    frame++;

   printf("FRAME=%d\n", frame);
    float aspect = (float)(height) / (float)(width);

    ESMatrix projection;
    esMatrixLoadIdentity(&projection);
    esFrustum(&projection, -2.8f, +2.8f, -2.8f * aspect, +2.8f * aspect, 6.0f, 10.0f);

    ESMatrix modelviewprojection;
    esMatrixLoadIdentity(&modelviewprojection);
    esMatrixMultiply(&modelviewprojection, &modelview, &projection);

 printf("trace %d\n", __LINE__);

        ESMatrix inverse, normal; /* compute inverse transpose normal transformation matrix */
        esMatrixInverse3x3(&inverse, &modelview);
        esMatrixTranspose(&normal, &inverse);

/*    float normal[9];
    normal[0] = modelview.m[0][0];
    normal[1] = modelview.m[0][1];
    normal[2] = modelview.m[0][2];
    normal[3] = modelview.m[1][0];
    normal[4] = modelview.m[1][1];
    normal[5] = modelview.m[1][2];
    normal[6] = modelview.m[2][0];
    normal[7] = modelview.m[2][1];
    normal[8] = modelview.m[2][2]; */

 printf("trace %d\n", __LINE__);
    etna_set_state_multi(cmdPtr, VIVS_VS_UNIFORMS(0), 16, (uint32_t*)&modelviewprojection.m[0][0]);
    etna_set_state_multi(cmdPtr, VIVS_VS_UNIFORMS(16), 3, (uint32_t*)&normal.m[0][0]); /* u4.xyz */
    etna_set_state_multi(cmdPtr, VIVS_VS_UNIFORMS(20), 3, (uint32_t*)&normal.m[1][0]); /* u5.xyz */
    etna_set_state_multi(cmdPtr, VIVS_VS_UNIFORMS(24), 3, (uint32_t*)&normal.m[2][0]); /* u6.xyz */
    etna_set_state_multi(cmdPtr, VIVS_VS_UNIFORMS(28), 16, (uint32_t*)&modelview.m[0][0]);
 printf("trace %d\n", __LINE__);
    etna_set_state(cmdPtr, VIVS_FE_VERTEX_STREAM_BASE_ADDR, vtx_physical); /* ADDR_E */
    etna_set_state(cmdPtr, VIVS_FE_VERTEX_STREAM_CONTROL,
            0x24 << VIVS_FE_VERTEX_STREAM_CONTROL_VERTEX_STRIDE__SHIFT);
    etna_set_state(cmdPtr, VIVS_FE_VERTEX_ELEMENT_CONFIG(0),
            VIVS_FE_VERTEX_ELEMENT_CONFIG_TYPE_FLOAT |
            (ENDIAN_MODE_NO_SWAP << VIVS_FE_VERTEX_ELEMENT_CONFIG_ENDIAN__SHIFT) |
            (0 << VIVS_FE_VERTEX_ELEMENT_CONFIG_STREAM__SHIFT) |
            (3 <<VIVS_FE_VERTEX_ELEMENT_CONFIG_NUM__SHIFT) |
            VIVS_FE_VERTEX_ELEMENT_CONFIG_NORMALIZE_OFF |
            (0x0 << VIVS_FE_VERTEX_ELEMENT_CONFIG_START__SHIFT) |
            (0xc << VIVS_FE_VERTEX_ELEMENT_CONFIG_END__SHIFT));
    etna_set_state(cmdPtr, VIVS_FE_VERTEX_ELEMENT_CONFIG(1),
            VIVS_FE_VERTEX_ELEMENT_CONFIG_TYPE_FLOAT |
            (ENDIAN_MODE_NO_SWAP << VIVS_FE_VERTEX_ELEMENT_CONFIG_ENDIAN__SHIFT) |
            (0 << VIVS_FE_VERTEX_ELEMENT_CONFIG_STREAM__SHIFT) |
            (3 <<VIVS_FE_VERTEX_ELEMENT_CONFIG_NUM__SHIFT) |
            VIVS_FE_VERTEX_ELEMENT_CONFIG_NORMALIZE_OFF |
            (0xc << VIVS_FE_VERTEX_ELEMENT_CONFIG_START__SHIFT) |
            (0x18 << VIVS_FE_VERTEX_ELEMENT_CONFIG_END__SHIFT));
    etna_set_state(cmdPtr, VIVS_FE_VERTEX_ELEMENT_CONFIG(2),
            VIVS_FE_VERTEX_ELEMENT_CONFIG_TYPE_FLOAT |
            (ENDIAN_MODE_NO_SWAP << VIVS_FE_VERTEX_ELEMENT_CONFIG_ENDIAN__SHIFT) |
            VIVS_FE_VERTEX_ELEMENT_CONFIG_NONCONSECUTIVE |
            (0 << VIVS_FE_VERTEX_ELEMENT_CONFIG_STREAM__SHIFT) |
            (3 <<VIVS_FE_VERTEX_ELEMENT_CONFIG_NUM__SHIFT) |
            VIVS_FE_VERTEX_ELEMENT_CONFIG_NORMALIZE_OFF |
            (0x18 << VIVS_FE_VERTEX_ELEMENT_CONFIG_START__SHIFT) |
            (0x24 << VIVS_FE_VERTEX_ELEMENT_CONFIG_END__SHIFT));
    etna_set_state(cmdPtr, VIVS_VS_INPUT(0), 0x20100);
    etna_set_state(cmdPtr, VIVS_PA_CONFIG, VIV_MASKED_BIT(VIVS_PA_CONFIG_POINT_SPRITE_ENABLE, 0));

    for(int prim=0; prim<6; ++prim) { /* this part is repeated 5 times */
        etna_draw_primitives(cmdPtr, PRIMITIVE_TYPE_TRIANGLE_STRIP, prim * 4, 2);
    }
 printf("trace %d\n", __LINE__);
    if (0) for(int prim=0; prim<6; ++prim) /* this part is repeated 5 times */
    {
        etna_set_state(cmdPtr, VIVS_PS_INPUT_COUNT, (31<<8)|2);
        etna_set_state(cmdPtr, VIVS_PS_TEMP_REGISTER_CONTROL,
                (2 << VIVS_PS_TEMP_REGISTER_CONTROL_NUM_TEMPS__SHIFT));
        etna_set_state(cmdPtr, VIVS_PS_CONTROL,
                VIVS_PS_CONTROL_UNK1
                );
        etna_set_state(cmdPtr, VIVS_PA_ATTRIBUTE_ELEMENT_COUNT, 0x100);
        etna_set_state(cmdPtr, VIVS_GL_VARYING_TOTAL_COMPONENTS,  /* one varying, with four components */
                (4 << VIVS_GL_VARYING_TOTAL_COMPONENTS_NUM__SHIFT)
                );
        etna_set_state(cmdPtr, VIVS_VS_LOAD_BALANCING, 0xf3f0582);
        etna_set_state(cmdPtr, VIVS_VS_OUTPUT_COUNT, 2);
        etna_set_state(cmdPtr, VIVS_PA_CONFIG, VIV_MASKED_BIT(VIVS_PA_CONFIG_POINT_SIZE_ENABLE, 0));
        etna_set_state(cmdPtr, VIVS_FE_VERTEX_STREAM_BASE_ADDR, vtx_physical); /* ADDR_E */
        etna_set_state(cmdPtr, VIVS_FE_VERTEX_STREAM_CONTROL,
                0x24 << VIVS_FE_VERTEX_STREAM_CONTROL_VERTEX_STRIDE__SHIFT);
        etna_set_state(cmdPtr, VIVS_FE_VERTEX_ELEMENT_CONFIG(0),
                VIVS_FE_VERTEX_ELEMENT_CONFIG_TYPE_FLOAT |
                (ENDIAN_MODE_NO_SWAP << VIVS_FE_VERTEX_ELEMENT_CONFIG_ENDIAN__SHIFT) |
                (0 << VIVS_FE_VERTEX_ELEMENT_CONFIG_STREAM__SHIFT) |
                (3 <<VIVS_FE_VERTEX_ELEMENT_CONFIG_NUM__SHIFT) |
                VIVS_FE_VERTEX_ELEMENT_CONFIG_NORMALIZE_OFF |
                (0x0 << VIVS_FE_VERTEX_ELEMENT_CONFIG_START__SHIFT) |
                (0xc << VIVS_FE_VERTEX_ELEMENT_CONFIG_END__SHIFT));
        etna_set_state(cmdPtr, VIVS_FE_VERTEX_ELEMENT_CONFIG(1),
                VIVS_FE_VERTEX_ELEMENT_CONFIG_TYPE_FLOAT |
                (ENDIAN_MODE_NO_SWAP << VIVS_FE_VERTEX_ELEMENT_CONFIG_ENDIAN__SHIFT) |
                (0 << VIVS_FE_VERTEX_ELEMENT_CONFIG_STREAM__SHIFT) |
                (3 <<VIVS_FE_VERTEX_ELEMENT_CONFIG_NUM__SHIFT) |
                VIVS_FE_VERTEX_ELEMENT_CONFIG_NORMALIZE_OFF |
                (0xc << VIVS_FE_VERTEX_ELEMENT_CONFIG_START__SHIFT) |
                (0x18 << VIVS_FE_VERTEX_ELEMENT_CONFIG_END__SHIFT));
        etna_set_state(cmdPtr, VIVS_FE_VERTEX_ELEMENT_CONFIG(2),
                VIVS_FE_VERTEX_ELEMENT_CONFIG_TYPE_FLOAT |
                (ENDIAN_MODE_NO_SWAP << VIVS_FE_VERTEX_ELEMENT_CONFIG_ENDIAN__SHIFT) |
                VIVS_FE_VERTEX_ELEMENT_CONFIG_NONCONSECUTIVE |
                (0 << VIVS_FE_VERTEX_ELEMENT_CONFIG_STREAM__SHIFT) |
                (3 <<VIVS_FE_VERTEX_ELEMENT_CONFIG_NUM__SHIFT) |
                VIVS_FE_VERTEX_ELEMENT_CONFIG_NORMALIZE_OFF |
                (0x18 << VIVS_FE_VERTEX_ELEMENT_CONFIG_START__SHIFT) |
                (0x24 << VIVS_FE_VERTEX_ELEMENT_CONFIG_END__SHIFT));
        etna_set_state(cmdPtr, VIVS_VS_INPUT(0), 0x20100);
        etna_set_state(cmdPtr, VIVS_PA_CONFIG, VIV_MASKED_BIT(VIVS_PA_CONFIG_POINT_SPRITE_ENABLE, 0));
        etna_draw_primitives(cmdPtr, PRIMITIVE_TYPE_TRIANGLE_STRIP, 4 + prim*4, 2);
 printf("trace %d\n", __LINE__);
    }


    etna_set_state(cmdPtr, VIVS_GL_FLUSH_CACHE, 0xffff);
    etna_set_state(cmdPtr, VIVS_GL_FLUSH_CACHE, 0xffff);
    etna_set_state(cmdPtr, VIVS_GL_FLUSH_CACHE, 0xffff);
    etna_set_state(cmdPtr, VIVS_GL_FLUSH_CACHE, 0xffff);
    etna_set_state(cmdPtr, VIVS_GL_FLUSH_CACHE, 0xffff);
    etna_set_state(cmdPtr, VIVS_GL_FLUSH_CACHE, 0xffff);
    etna_set_state(cmdPtr, VIVS_GL_FLUSH_CACHE, 0xffff);



#if 0
    // TMP
    //
        etna_set_state(cmdPtr, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_COLOR | VIVS_GL_FLUSH_CACHE_DEPTH);
        etna_set_state(cmdPtr, VIVS_RS_CONFIG,
                VIVS_RS_CONFIG_SOURCE_FORMAT(RS_FORMAT_X8R8G8B8) |
                (0 * VIVS_RS_CONFIG_SOURCE_TILED) |
                VIVS_RS_CONFIG_DEST_FORMAT(RS_FORMAT_X8R8G8B8) |
                (0 * VIVS_RS_CONFIG_DEST_TILED));
	int supertiled = 1;
        etna_set_state(cmdPtr, VIVS_RS_SOURCE_STRIDE, (padded_width * 4 * 4 / 2) | (supertiled?VIVS_RS_SOURCE_STRIDE_TILING:0));
        etna_set_state(cmdPtr, VIVS_RS_DEST_STRIDE, (padded_width * 4 * 4 / 2) | (supertiled?VIVS_RS_DEST_STRIDE_TILING:0));
        etna_set_state(cmdPtr, VIVS_RS_DITHER(0), 0xffffffff);
        etna_set_state(cmdPtr, VIVS_RS_DITHER(1), 0xffffffff);
        etna_set_state(cmdPtr, VIVS_RS_CLEAR_CONTROL, VIVS_RS_CLEAR_CONTROL_MODE_DISABLED);
        etna_set_state(cmdPtr, VIVS_RS_EXTRA_CONFIG, 0); /* no AA, no endian switch */
        etna_set_state(cmdPtr, VIVS_RS_SOURCE_ADDR, rt_physical); /* ADDR_A */
        etna_set_state(cmdPtr, VIVS_RS_PIPE_SOURCE_ADDR(0), rt_physical); /* ADDR_B */
        etna_set_state(cmdPtr, VIVS_RS_DEST_ADDR, rt_physical); /* ADDR_A */
        etna_set_state(cmdPtr, VIVS_RS_PIPE_DEST_ADDR(0), rt_physical); /* ADDR_B */
        etna_set_state(cmdPtr, VIVS_RS_WINDOW_SIZE,
                VIVS_RS_WINDOW_SIZE_HEIGHT(padded_height) |
                VIVS_RS_WINDOW_SIZE_WIDTH(padded_width));
        etna_set_state(cmdPtr, VIVS_RS_KICKER, 0xbeebbeeb);

        /* Submit second command buffer */
	printf("trace %d\n", __LINE__);
        //etna_flush(cmdPtr, NULL);
	//etnaviv_dmp(PIPE_ID_PIPE_3D);
        etna_warm_up_rs(cmdPtr, aux_rt_physical, aux_rt_ts_physical);
        etna_set_state(cmdPtr, VIVS_TS_COLOR_STATUS_BASE, rt_ts_physical); /* ADDR_B */
        etna_set_state(cmdPtr, VIVS_TS_COLOR_SURFACE_BASE, rt_physical); /* ADDR_A */
        etna_set_state(cmdPtr, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_COLOR);
        etna_set_state(cmdPtr, VIVS_TS_MEM_CONFIG,
                VIVS_TS_MEM_CONFIG_DEPTH_FAST_CLEAR |
                VIVS_TS_MEM_CONFIG_DEPTH_16BPP |
                VIVS_TS_MEM_CONFIG_DEPTH_COMPRESSION);
        etna_set_state(cmdPtr, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_COLOR);
        etna_set_state(cmdPtr, VIVS_PE_COLOR_FORMAT,
                ~(VIVS_PE_COLOR_FORMAT_OVERWRITE | VIVS_PE_COLOR_FORMAT_OVERWRITE_MASK));

// TMP end
#endif

   printf("trace %d\n", __LINE__);
    printf("Done is %d\n", done);
#if 0
	printf("stream buf is %p\n", stream->buffer);

	printf("cmdbuf1 sz is %d (%x)\n", ARRAY_SIZE(cmdbuf1), ARRAY_SIZE(cmdbuf1));

	cmdbuf1[87] = cmdbuf1[103] = cmdbuf1[161] = cmdbuf1[189] = cmdbuf1[219] = cmdbuf1[253] = rt_physical;
	cmdbuf1[101] = cmdbuf1[159] = cmdbuf1[187] = cmdbuf1[217] = cmdbuf1[231] = cmdbuf1[251] = rt_ts_physical;
	cmdbuf1[111] = cmdbuf1[129] = z_physical;
	cmdbuf1[127] = z_ts_physical;
	cmdbuf1[197] = cmdbuf1[139] = cmdbuf1[145]
		= cmdbuf1[149] = cmdbuf1[203] = cmdbuf1[167]
		= cmdbuf1[173] = cmdbuf1[177] = cmdbuf1[207] = aux_rt_physical;

	cmdbuf1[137] = cmdbuf1[165] = cmdbuf1[195] = aux_rt_ts_physical;
	cmdbuf1[501] = cmdbuf1[535] = cmdbuf1[569]
		= cmdbuf1[603] = cmdbuf1[637] = cmdbuf1[671] = vtx_physical;

	if (0) { ins_nop(0, 0); ins_nopn(0, 0, 0); stall_tick(); }

	if (0) { stream->offset = add_pipe_ops(cmdbuf1, 690); }

	if (0) { for (int i = 0; i < 690; i++) {
		if (stream->buffer[i] != cmdbuf1[i]) {
			printf("Element not equal %d: %08x %08x | %08x %08x\n", i, stream->buffer[i - 1], stream->buffer[i], cmdbuf1[i - 1], cmdbuf1[i]);

			if (0 && i > 250) {
				stream->buffer[i] = cmdbuf1[i];
			}
		}
	} }

	if (0) {memcpy(stream->buffer + done,
			cmdbuf1 + done,
			sizeof(cmdbuf1)); }

	if (0) add_pipe_ops(cmdbuf1, 690);
	dcache_flush(stream->buffer, 4 * stream->offset);
#endif
	//stream->offset = add_pipe_ops(stream->buffer, stream->offset);
	//

	memcpy(&buffer_memory[_cs++][0], cmdPtr->buffer, cmdPtr->offset * 4);

	etna_cmd_stream_finish(cmdPtr);

	//etna_cmd_stream_del(stream);


	if (_cs > 1) {
		printf("start cmp, cs=%d\n", _cs);
	for (int i = 0; i < 1024; i++) {
		if (buffer_memory[_cs - 1][i] != buffer_memory[_cs - 2][i]) {
			printf("Element not equal %d: %08x %08x | %08x %08x\n", i,
					buffer_memory[_cs - 1][i - 1], buffer_memory[_cs - 1][i],
					buffer_memory[_cs - 2][i - 1], buffer_memory[_cs - 2][i]);
		}
	}
	printf("end cmp\n");
	}
}

static void copy_from_rt_to_bmp(void) {
	struct etna_cmd_stream *stream;
	stream = etna_cmd_stream_new(_pipe, 0x1000, NULL, NULL);

	etna_set_state(stream, VIVS_GL_FLUSH_CACHE,
			VIVS_GL_FLUSH_CACHE_DEPTH |
			VIVS_GL_FLUSH_CACHE_COLOR);
	etna_set_state(stream, VIVS_RS_CONFIG,
			VIVS_RS_CONFIG_SOURCE_FORMAT(RS_FORMAT_X8R8G8B8) |
			VIVS_RS_CONFIG_DEST_FORMAT(RS_FORMAT_X8R8G8B8) |
			VIVS_RS_CONFIG_SOURCE_TILED);
	etna_set_state(stream, VIVS_RS_SOURCE_STRIDE,
			VIVS_RS_SOURCE_STRIDE_TILING |
			padded_width * 16);
	etna_set_state(stream, VIVS_RS_DEST_STRIDE,
			VIVS_RS_DEST_STRIDE_TILING |
			width * 4);

	etna_set_state(stream, VIVS_RS_DITHER(0),
			0xffffffff);
	etna_set_state(stream, VIVS_RS_DITHER(1),
			0xffffffff);

	etna_set_state(stream, VIVS_RS_CLEAR_CONTROL, 0x0);
	etna_set_state(stream, VIVS_RS_EXTRA_CONFIG, 0x0);

	etna_set_state(stream, VIVS_RS_SOURCE_ADDR, rt_physical);
	etna_set_state(stream, VIVS_RS_PIPE_SOURCE_ADDR(0), rt_physical);
	etna_set_state(stream, VIVS_RS_DEST_ADDR, bmp_physical);
	etna_set_state(stream, VIVS_RS_PIPE_DEST_ADDR(0), bmp_physical);

	etna_set_state(stream, VIVS_RS_WINDOW_SIZE,
			VIVS_RS_WINDOW_SIZE_HEIGHT(height) |
			VIVS_RS_WINDOW_SIZE_WIDTH(width));

	etna_set_state(stream, VIVS_RS_KICKER, 0xbeebbeeb);

	dcache_flush(stream->buffer, 4 * stream->offset);

	etna_cmd_stream_finish(stream);
}

extern uint8_t local_buffer[64 * 1024 * 1024];
static void gen_cmd_stream(struct etna_bo *bmp) {
	static int first = 0;

	if (!first) {
	padded_width = etna_align_up(width, 64) / 2;
	padded_height = etna_align_up(height, 64) / 2;

	bits_per_tile = 2;
	rt_size = padded_width * padded_height * 4;
	rt_ts_size = etna_align_up((padded_width * padded_height * 4)*bits_per_tile/0x80, 0x100);
	z_size = padded_width * padded_height * 2;
	z_ts_size = etna_align_up((padded_width * padded_height * 2)*bits_per_tile/0x80, 0x100);
	aux_rt_size = 0x4000;
	aux_rt_ts_size = 0x80 * bits_per_tile;

	padded_width *= 2;
	padded_height *= 2;


	/* Allocate buffers */
	if( (rt=etna_bo_new(dev, rt_size, ETNA_BO_UNCACHED))==NULL ||
			(rt_ts=etna_bo_new(dev, rt_ts_size, ETNA_BO_UNCACHED))==NULL ||
			(z=etna_bo_new(dev, z_size, ETNA_BO_UNCACHED))==NULL ||
			(z_ts=etna_bo_new(dev, z_ts_size, ETNA_BO_UNCACHED))==NULL ||
			(vtx=etna_bo_new(dev, VERTEX_BUFFER_SIZE, ETNA_BO_UNCACHED))==NULL ||
			(aux_rt=etna_bo_new(dev, aux_rt_size, ETNA_BO_UNCACHED))==NULL ||
			(aux_rt_ts=etna_bo_new(dev, aux_rt_ts_size, ETNA_BO_UNCACHED))==NULL) {
		fprintf(stderr, "Error allocating video memory\n");
		return;
	}

	}

	//rt=etna_bo_new(dev, rt_size, ETNA_BO_UNCACHED);
	//rt_ts=etna_bo_new(dev, rt_ts_size, ETNA_BO_UNCACHED);
	//z=etna_bo_new(dev, z_size, ETNA_BO_UNCACHED);
	//z_ts=etna_bo_new(dev, z_ts_size, ETNA_BO_UNCACHED);
	//vtx=etna_bo_new(dev, VERTEX_BUFFER_SIZE, ETNA_BO_UNCACHED);
	//aux_rt=etna_bo_new(dev, aux_rt_size, ETNA_BO_UNCACHED);
	//aux_rt_ts=etna_bo_new(dev, aux_rt_ts_size, ETNA_BO_UNCACHED);

	memset(etna_bo_map(rt), 0, rt_size);
	dcache_flush(etna_bo_map(rt), rt_size);

	memset(etna_bo_map(rt_ts), 0, rt_ts_size);
	dcache_flush(etna_bo_map(rt), rt_size);

	memset(etna_bo_map(z), 0, z_size);
	dcache_flush(etna_bo_map(z), z_size);

	memset(etna_bo_map(z_ts), 0, z_ts_size);
	dcache_flush(etna_bo_map(z_ts), z_ts_size);

	memset(etna_bo_map(vtx), 0, VERTEX_BUFFER_SIZE);
	dcache_flush(etna_bo_map(vtx), VERTEX_BUFFER_SIZE);

	memset(etna_bo_map(aux_rt), 0, aux_rt_size);
	dcache_flush(etna_bo_map(aux_rt), aux_rt_size);

	memset(etna_bo_map(aux_rt_ts), 0, aux_rt_ts_size);
	dcache_flush(etna_bo_map(aux_rt_ts), aux_rt_ts_size);

	void *vtx_logical = etna_bo_map(vtx);

	for (int vert = 0; vert < NUM_VERTICES; ++vert) {
		int src_idx = vert * COMPONENTS_PER_VERTEX;
		int dest_idx = vert * COMPONENTS_PER_VERTEX * 3;
		for(int comp=0; comp<COMPONENTS_PER_VERTEX; ++comp) {
			((float*)vtx_logical)[dest_idx+comp+0] = vVertices[src_idx + comp]; /* 0 */
			((float*)vtx_logical)[dest_idx+comp+3] = vNormals[src_idx + comp]; /* 1 */
			((float*)vtx_logical)[dest_idx+comp+6] = vColors[src_idx + comp]; /* 2 */
		}
	}

	//dcache_flush(vtx_logical, NUM_VERTICES * COMPONENTS_PER_VERTEX + 4);

	rt_physical =		((int) etna_bo_map(rt))        - 0x10000000 * 1;
	rt_ts_physical =	((int) etna_bo_map(rt_ts))     - 0x10000000 * 1;
	z_physical =		((int) etna_bo_map(z))         - 0x10000000 * 1;
	z_ts_physical =		((int) etna_bo_map(z_ts))      - 0x10000000 * 1;
	aux_rt_physical =	((int) etna_bo_map(aux_rt))    - 0x10000000 * 1;
	aux_rt_ts_physical =	((int) etna_bo_map(aux_rt_ts)) - 0x10000000 * 1;
	vtx_physical =		((int) etna_bo_map(vtx))       - 0x10000000 * 1;
	bmp_physical =		((int) etna_bo_map(bmp))       - 0x10000000 * 1;


	if (!first) {
	printf("Some buffers:  rt %p  rt_ts %p         z %p z_tz %p\n"
	       "              vtx %p aux_rt %p aux_rt_tx %p  bmp %p\n",
			etna_bo_map(rt),
			etna_bo_map(rt_ts),
			etna_bo_map(z),
			etna_bo_map(z_ts),
			etna_bo_map(vtx),
			etna_bo_map(aux_rt),
			etna_bo_map(aux_rt_ts),
			etna_bo_map(bmp));


	_nocache(etna_bo_map(rt), rt_size);
	_nocache(etna_bo_map(rt_ts), rt_ts_size);
	_nocache(etna_bo_map(z), z_size);
	_nocache(etna_bo_map(z_ts), z_ts_size);
	_nocache(etna_bo_map(vtx), VERTEX_BUFFER_SIZE);
	_nocache(etna_bo_map(aux_rt), aux_rt_size);
	_nocache(etna_bo_map(aux_rt_ts), aux_rt_ts_size);
	_nocache(etna_bo_map(bmp), width * height * 4);
	//_nocache(local_buffer, 64 * 1024 * 1024);
	_nocache(0x00000000, 0xCFFFF000);

	uint32_t *mmu_root = vmem_current_context();
	printf("MMU root: %p\n", mmu_root);

	printf("Inspecting mem pages\n");
	for (uint32_t *p = 0x23a4b000; p < 0x23d50000 ; p++) {
		static int _cn = 0;

		if (*p & 0x8) {
		printf("%p: %08x ", p, *p);

		p++;
		_cn++;

		if (_cn % 4 == 0) {
			//printf("\n");
		}
		}
	}

	mmu_on();
	}

	first = 1;
 printf("trace %d\n", __LINE__);
	draw_cube();

	copy_from_rt_to_bmp();
}


extern uint16_t ipu_fb[];
int main(int argc, char *argv[]) {
	size_t out_size = width * height * 4;
	struct etna_gpu *gpu;
	struct etna_bo *bmp;
	uint32_t *ptr;

	drmVersionPtr version;
	int fd, ret = 0;

	if (argc < 2) {
		printf("Missing DRM file name, assume /dev/card\n");
		fd = open("/dev/card", O_RDWR);
	} else {
		fd = open(argv[1], O_RDWR);
	}

	if (fd < 0) {
		printf("Failed to open DRM file\n");
		return 1;
	}

	vmem_map_region(vmem_current_context(), 0x10000000, 0x10000000, 0x10000000, VMEM_PAGE_WRITABLE);


	if (0) for (uint32_t p = 0x10000000; p < 0x40000000; p+= 4) {
		dcache_flush(p, 4);
		dcache_inval(p, 4);
	}
	vmem_off();
#if 0
	for (uint32_t p = 0x10000000; p < 0x40000000; p+= 4096) {
		int ret;
		if ((ret = vmem_page_set_flags(vmem_current_context(), p, VMEM_PAGE_WRITABLE))) {
			printf("Page %p err %d\n", (void *) p, ret);
		}
	}

	mmu_flush_tlb();
#endif

	//printf("trace %d\n", __LINE__);
	if ((version = drmGetVersion(fd))) {
		printf("Version: %d.%d.%d\n", version->version_major,
				version->version_minor, version->version_patchlevel);
		printf("  Name: %s\n", version->name);
		printf("  Date: %s\n", version->date);
		printf("  Description: %s\n", version->desc);
		drmFreeVersion(version);
	}
	//printf("trace %d\n", __LINE__);

	if (!(dev = etna_device_new(fd))) {
		ret = 2;
		printf("Failed to create Etnaviv device\n");
	}
	//printf("trace %d\n", __LINE__);

	if (!(gpu = etna_gpu_new(dev, 1))) {
		ret = 3;
		printf("Failed to create Etnaviv GPU\n");
	}

	//printf("trace %d\n", __LINE__);
	if (!(_pipe = etna_pipe_new(gpu, 0))) {
		ret = 4;
		printf("Failed to create Etnaviv pipe\n");
	}

	//printf("trace %d\n", __LINE__);
	if (!(bmp = etna_bo_new(dev, out_size, ETNA_BO_UNCACHED))) {
		fprintf(stderr, "Unable to allocate buffer\n");
	}
	//printf("trace %d\n", __LINE__);
	while(1) {
	//	memset(etna_bo_map(bmp), 0, out_size);
	//dcache_flush(etna_bo_map(bmp), out_size);
	//printf("trace %d\n", __LINE__);
	gen_cmd_stream(bmp);
	//printf("trace %d\n", __LINE__);
	ptr = etna_bo_map(bmp);
	//printf("trace %d\n", __LINE__);
	dcache_inval(ptr, height * width * 4);
	//printf("trace %d\n", __LINE__);

	if (width == 1024 && height == 768) {
		pix_fmt_convert(ptr, &ipu_fb[0], width * height, RGB888, RGB565);
	} else {
		for (int i = 0; i < height; i++) {
			pix_fmt_convert(ptr, &ipu_fb[i * 1024], width, RGB888, RGB565);
			ptr += width;
		}
	}

	dcache_flush(ipu_fb, 1024 * 768 * 2);

	//dcache_flush(ipu_fb, 1280 * 1024 * 2);

	if (0) {
		nonzero_lookup(rt, 16, "rt");
		nonzero_lookup(rt_ts, rt_ts_size, "rt_ts");
		nonzero_lookup(z, z_size, "z");
		nonzero_lookup(z_ts, z_ts_size, "z_ts");
		nonzero_lookup(aux_rt, aux_rt_size, "aux_rt");
		nonzero_lookup(aux_rt_ts, aux_rt_ts_size, "aux_rt_ts");
		nonzero_lookup(bmp, out_size, "bmp");
	}
	}

	close(fd);

	return ret;
}
