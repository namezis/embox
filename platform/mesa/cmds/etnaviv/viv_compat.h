#ifndef VIV_COMPAT_H_
#define VIV_COMPAT_H_

#include "viv.h"

#include <xf86drm.h>
#include "etnaviv_drmif.h"
#include "etnaviv_drm.h"

extern int viv_open(enum viv_hw_type type, struct viv_conn **conn);

#define etna_bo_gpu_address(bo) (((uint32_t) etna_bo_map(bo) - 0x10000000))

static inline uint32_t etna_f32_to_u32(float value)
{
    union {
        uint32_t u32;
        float f32;
    } x = { .f32 = value };
    return x.u32;
}

typedef viv_addr_t uint32_t;

	static inline void
etna_emit_load_state(struct etna_cmd_stream *stream, const uint16_t offset,
                     const uint16_t count, const int fixp)
{
   uint32_t v;

   v = VIV_FE_LOAD_STATE_HEADER_OP_LOAD_STATE |
       COND(fixp, VIV_FE_LOAD_STATE_HEADER_FIXP) |
       VIV_FE_LOAD_STATE_HEADER_OFFSET(offset) |
       (VIV_FE_LOAD_STATE_HEADER_COUNT(count) &
        VIV_FE_LOAD_STATE_HEADER_COUNT__MASK);

   etna_cmd_stream_emit(stream, v);
}

static inline void
etna_set_state(struct etna_cmd_stream *stream, uint32_t address, uint32_t value)
{
   etna_cmd_stream_reserve(stream, 2);
   etna_emit_load_state(stream, address >> 2, 1, 0);
   etna_cmd_stream_emit(stream, value);
}

static inline void etna_set_state_from_bo(struct etna_cmd_stream *stream,
		uint32_t address, struct etna_bo *bo)
{
	etna_cmd_stream_reserve(stream, 2);
	etna_emit_load_state(stream, address >> 2, 1, 0);
	etna_cmd_stream_reloc(stream, &(struct etna_reloc){
		.bo = bo,
		.flags = ETNA_RELOC_READ,
		.offset = 0,
	});
}

void etna_warm_up_rs2(struct etna_cmd_stream *cmdbuf, struct etna_bo *bo1, struct etna_bo *bo2)
{
    etna_set_state_from_bo(cmdbuf, VIVS_TS_COLOR_STATUS_BASE, bo2);
    etna_set_state_from_bo(cmdbuf, VIVS_TS_COLOR_SURFACE_BASE, bo1);
    //etna_set_state(cmdbuf, VIVS_TS_COLOR_STATUS_BASE, aux_rt_ts_physical); /* ADDR_G */
    //etna_set_state(cmdbuf, VIVS_TS_COLOR_SURFACE_BASE, aux_rt_physical); /* ADDR_F */
    etna_set_state(cmdbuf, VIVS_TS_FLUSH_CACHE, VIVS_TS_FLUSH_CACHE_FLUSH);
    etna_set_state(cmdbuf, VIVS_RS_CONFIG,  /* wut? */
            VIVS_RS_CONFIG_SOURCE_FORMAT(RS_FORMAT_A8R8G8B8) |
            VIVS_RS_CONFIG_SOURCE_TILED |
            VIVS_RS_CONFIG_DEST_FORMAT(RS_FORMAT_R5G6B5) |
            VIVS_RS_CONFIG_DEST_TILED);

    etna_set_state_from_bo(cmdbuf, VIVS_RS_SOURCE_ADDR, bo1);
//    etna_set_state(cmdbuf, VIVS_RS_SOURCE_ADDR, aux_rt_physical); /* ADDR_F */
    etna_set_state(cmdbuf, VIVS_RS_SOURCE_STRIDE, 0x40);
    //etna_set_state(cmdbuf, VIVS_RS_DEST_ADDR, aux_rt_physical); /* ADDR_F */
    etna_set_state_from_bo(cmdbuf, VIVS_RS_DEST_ADDR, bo2);
    etna_set_state(cmdbuf, VIVS_RS_DEST_STRIDE, 0x40);
    etna_set_state(cmdbuf, VIVS_RS_WINDOW_SIZE,
            VIVS_RS_WINDOW_SIZE_HEIGHT(2) |
            VIVS_RS_WINDOW_SIZE_WIDTH(16));
    etna_set_state(cmdbuf, VIVS_RS_CLEAR_CONTROL, VIVS_RS_CLEAR_CONTROL_MODE_DISABLED);
    etna_set_state(cmdbuf, VIVS_RS_KICKER, 0xbeebbeeb);
}


void etna_warm_up_rs(struct etna_cmd_stream *cmdbuf, viv_addr_t aux_rt_physical, viv_addr_t aux_rt_ts_physical)
{
    etna_set_state(cmdbuf, VIVS_TS_COLOR_STATUS_BASE, aux_rt_ts_physical); /* ADDR_G */
    etna_set_state(cmdbuf, VIVS_TS_COLOR_SURFACE_BASE, aux_rt_physical); /* ADDR_F */
    etna_set_state(cmdbuf, VIVS_TS_FLUSH_CACHE, VIVS_TS_FLUSH_CACHE_FLUSH);
    etna_set_state(cmdbuf, VIVS_RS_CONFIG,  /* wut? */
            VIVS_RS_CONFIG_SOURCE_FORMAT(RS_FORMAT_A8R8G8B8) |
            VIVS_RS_CONFIG_SOURCE_TILED |
            VIVS_RS_CONFIG_DEST_FORMAT(RS_FORMAT_R5G6B5) |
            VIVS_RS_CONFIG_DEST_TILED);

    etna_set_state(cmdbuf, VIVS_RS_SOURCE_ADDR, aux_rt_physical); /* ADDR_F */
    etna_set_state(cmdbuf, VIVS_RS_SOURCE_STRIDE, 0x40);
    etna_set_state(cmdbuf, VIVS_RS_DEST_ADDR, aux_rt_physical); /* ADDR_F */
    etna_set_state(cmdbuf, VIVS_RS_DEST_STRIDE, 0x40);
    etna_set_state(cmdbuf, VIVS_RS_WINDOW_SIZE,
            VIVS_RS_WINDOW_SIZE_HEIGHT(2) |
            VIVS_RS_WINDOW_SIZE_WIDTH(16));
    etna_set_state(cmdbuf, VIVS_RS_CLEAR_CONTROL, VIVS_RS_CLEAR_CONTROL_MODE_DISABLED);
    etna_set_state(cmdbuf, VIVS_RS_KICKER, 0xbeebbeeb);
}

#define etna_set_state_f32(cmdbuf, address, value) \
    etna_set_state(cmdbuf, address, etna_f32_to_u32(value));

/* Queue a STALL command (queues 2 words) */
static inline void
CMD_STALL(struct etna_cmd_stream *stream, uint32_t from, uint32_t to)
{
   etna_cmd_stream_emit(stream, VIV_FE_STALL_HEADER_OP_STALL);
   etna_cmd_stream_emit(stream, VIV_FE_STALL_TOKEN_FROM(from) | VIV_FE_STALL_TOKEN_TO(to));
}

static inline void
etna_stall(struct etna_cmd_stream *stream, uint32_t from, uint32_t to)
{
   etna_cmd_stream_reserve(stream, 4);

   etna_emit_load_state(stream, VIVS_GL_SEMAPHORE_TOKEN >> 2, 1, 0);
   etna_cmd_stream_emit(stream, VIVS_GL_SEMAPHORE_TOKEN_FROM(from) | VIVS_GL_SEMAPHORE_TOKEN_TO(to));

   if (from == SYNC_RECIPIENT_FE) {
      /* if the frontend is to be stalled, queue a STALL frontend command */
      CMD_STALL(stream, from, to);
   } else {
      /* otherwise, load the STALL token state */
      etna_emit_load_state(stream, VIVS_GL_STALL_TOKEN >> 2, 1, 0);
      etna_cmd_stream_emit(stream, VIVS_GL_STALL_TOKEN_FROM(from) | VIVS_GL_STALL_TOKEN_TO(to));
   }
}

static inline void
etna_set_state_multi(struct etna_cmd_stream *stream, uint32_t base,
                     uint32_t num, const uint32_t *values)
{
   if (num == 0)
      return;

   etna_cmd_stream_reserve(stream, 1 + num + 1); /* 1 extra for potential alignment */
   etna_emit_load_state(stream, base >> 2, num, 0);

   for (uint32_t i = 0; i < num; i++)
      etna_cmd_stream_emit(stream, values[i]);

   /* add potential padding */
   if ((num % 2) == 0)
      etna_cmd_stream_emit(stream, 0);
}

#undef etna_reserve
#undef ETNA_EMIT_LOAD_STATE
#undef ETNA_EMIT

#define etna_reserve		etna_cmd_stream_reserve
#define ETNA_EMIT_LOAD_STATE    etna_emit_load_state
#define ETNA_EMIT		etna_cmd_stream_emit

#define etna_finish etna_cmd_stream_finish

#define etna_flush(a, b)	 etna_cmd_stream_flush(a)

static inline void etna_set_state_fixp(struct etna_cmd_stream *cmdbuf, uint32_t address, uint32_t value)
{
    etna_reserve(cmdbuf, 2);
    ETNA_EMIT_LOAD_STATE(cmdbuf, address >> 2, 1, 1);
    ETNA_EMIT(cmdbuf, value);
}

static inline void
etna_draw_primitives(struct etna_cmd_stream *stream, uint32_t primitive_type,
                     uint32_t start, uint32_t count)
{
   etna_cmd_stream_reserve(stream, 4);

   etna_cmd_stream_emit(stream, VIV_FE_DRAW_PRIMITIVES_HEADER_OP_DRAW_PRIMITIVES);
   etna_cmd_stream_emit(stream, primitive_type);
   etna_cmd_stream_emit(stream, start);
   etna_cmd_stream_emit(stream, count);
}

#endif /* VIV_COMPAT_H_ */
