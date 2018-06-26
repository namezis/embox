/*
 * Copyright (c) 2012-2013 Etnaviv Project
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
/* Experimentation with non-supertiled, 4-bit-per-tile rendering on cubox.
 * This succesfully renders the cube on the dove gc600!
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdarg.h>

#include <errno.h>

#include <etnaviv/common.xml.h>
#include <etnaviv/state.xml.h>
#include <etnaviv/state_3d.xml.h>
//#include <etnaviv/etnaviv_emit.h>
#include <etnaviv/cmdstream.xml.h>
//#include <etnaviv/viv.h>
//#include <etnaviv/etna.h>
#include "etnaviv/etna_bo.h"
#include <etnaviv/etnaviv_util.h>
//#include <etnaviv/etna_rs.h>
#include <etnaviv/etnaviv_drmif.h>
#include "etna.h"
#include "viv_compat.h"

#include "write_bmp.h"

#include "esTransform.h"

#include <drivers/video/fb.h>
extern uint16_t ipu_fb[];

#define VERTEX_BUFFER_SIZE 0x60000

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
size_t vs_size = sizeof(vs);
size_t ps_size = sizeof(ps);

#define PIPE_3D_ID 0x0

int main(int argc, char **argv)
{
    int rv;
    int width = 256;
    int height = 256;
    int padded_width = etna_align_up(width, 64) / 2;
    int padded_height = etna_align_up(height, 64) / 2;

    printf("padded_width %i padded_height %i\n", padded_width, padded_height);
    struct viv_conn *conn = 0;
    rv = viv_open(VIV_HW_3D, &conn);
    if(rv!=0)
    {
        fprintf(stderr, "Error opening device\n");
        exit(1);
    }
    printf("Succesfully opened device\n");

    bool supertiled = VIV_FEATURE(conn, chipMinorFeatures0,SUPER_TILED);
    unsigned bits_per_tile = VIV_FEATURE(conn, chipMinorFeatures0,2BITPERTILE)?2:4;

    printf("Supertile: %i, bits per tile: %i\n", supertiled, bits_per_tile);

    struct etna_bo *rt = 0; /* main render target */
    struct etna_bo *rt_ts = 0; /* tile status for main render target */
    struct etna_bo *z = 0; /* depth for main render target */
    struct etna_bo *z_ts = 0; /* depth ts for main render target */
    struct etna_bo *vtx = 0; /* vertex buffer */
    struct etna_bo *aux_rt = 0; /* auxilary render target */
    struct etna_bo *aux_rt_ts = 0; /* tile status for auxilary render target */
    struct etna_bo *bmp = 0; /* bitmap */

    size_t rt_size = padded_width * padded_height * 4;
    size_t rt_ts_size = etna_align_up((padded_width * padded_height * 4)*bits_per_tile/0x80, 0x100);
    size_t z_size = padded_width * padded_height * 2;
    size_t z_ts_size = etna_align_up((padded_width * padded_height * 2)*bits_per_tile/0x80, 0x100);
    size_t bmp_size = width * height * 4;

    struct etna_device *dev;

    dev = etna_device_new(conn->fd);
    if((rt=etna_bo_new(dev, rt_size, DRM_ETNA_GEM_TYPE_RT))==NULL ||
       (rt_ts=etna_bo_new(dev, rt_ts_size, DRM_ETNA_GEM_TYPE_TS))==NULL ||
       (z=etna_bo_new(dev, z_size, DRM_ETNA_GEM_TYPE_ZS))==NULL ||
       (z_ts=etna_bo_new(dev, z_ts_size, DRM_ETNA_GEM_TYPE_TS))==NULL ||
       (vtx=etna_bo_new(dev, VERTEX_BUFFER_SIZE, DRM_ETNA_GEM_TYPE_VTX))==NULL ||
       (aux_rt=etna_bo_new(dev, 0x4000, DRM_ETNA_GEM_TYPE_RT))==NULL ||
       (aux_rt_ts=etna_bo_new(dev, 0x80*bits_per_tile, DRM_ETNA_GEM_TYPE_TS))==NULL ||
       (bmp=etna_bo_new(dev, bmp_size, DRM_ETNA_GEM_TYPE_BMP))==NULL
       )
    {
        fprintf(stderr, "Error allocating video memory\n");
        exit(1);
    }

    printf("trace %d\n", __LINE__);

    /* Phew, now we got all the memory we need.
     * Write interleaved attribute vertex stream.
     * Unlike the GL example we only do this once, not every time glDrawArrays is called, the same would be accomplished
     * from GL by using a vertex buffer object.
     */
    for(int vert=0; vert<NUM_VERTICES; ++vert)
    {
        int src_idx = vert * COMPONENTS_PER_VERTEX;
        int dest_idx = vert * COMPONENTS_PER_VERTEX * 3;
        for(int comp=0; comp<COMPONENTS_PER_VERTEX; ++comp)
        {
            ((float*)etna_bo_map(vtx))[dest_idx+comp+0] = vVertices[src_idx + comp]; /* 0 */
            ((float*)etna_bo_map(vtx))[dest_idx+comp+3] = vNormals[src_idx + comp]; /* 1 */
            ((float*)etna_bo_map(vtx))[dest_idx+comp+6] = vColors[src_idx + comp]; /* 2 */
        }
    }

    printf("trace %d\n", __LINE__);
 //   struct etna_ctx *ctx = 0;
	struct etna_gpu *gpu = etna_gpu_new(dev, 1);
	struct etna_pipe *pipe = etna_pipe_new(gpu, PIPE_3D_ID);
	struct etna_cmd_stream *ctx = etna_cmd_stream_new(pipe, 0x30000, NULL, NULL);
#if 0
    if(etna_create(conn, &ctx) != ETNA_OK)
    {
        printf("Unable to create context\n");
        exit(1);
    }
#endif
    /* XXX how important is the ordering? I suppose we could group states (except the flushes, kickers, semaphores etc)
     * and simply submit them at once. Especially for consecutive states and masked stated this could be a big win
     * in DMA command buffer size. */
#if 0
        etna_set_state(ctx, VIVS_RA_CONTROL, 0x3);

        etna_set_state(ctx, VIVS_GL_MULTI_SAMPLE_CONFIG,
                VIVS_GL_MULTI_SAMPLE_CONFIG_MSAA_SAMPLES_NONE |
                VIVS_GL_MULTI_SAMPLE_CONFIG_MSAA_ENABLES(0xf) | (1 << 12) | (1 << 16)/*|
                VIVS_GL_MULTI_SAMPLE_CONFIG_UNK12 |
                VIVS_GL_MULTI_SAMPLE_CONFIG_UNK16 */
                );
        etna_set_state(ctx, VIVS_GL_VERTEX_ELEMENT_CONFIG, 0x1);
        etna_set_state(ctx, VIVS_GL_VARYING_NUM_COMPONENTS,
                VIVS_GL_VARYING_NUM_COMPONENTS_VAR0(4)| /* position */
                VIVS_GL_VARYING_NUM_COMPONENTS_VAR1(2)  /* texture coordinate */
                );
        etna_set_state(ctx, VIVS_GL_VARYING_TOTAL_COMPONENTS,
                VIVS_GL_VARYING_TOTAL_COMPONENTS_NUM(4 + 2)
                );
        etna_set_state_multi(ctx, VIVS_GL_VARYING_COMPONENT_USE(0), 2, (uint32_t[]){
                VIVS_GL_VARYING_COMPONENT_USE_COMP0(VARYING_COMPONENT_USE_USED) |
                VIVS_GL_VARYING_COMPONENT_USE_COMP1(VARYING_COMPONENT_USE_USED) |
                VIVS_GL_VARYING_COMPONENT_USE_COMP2(VARYING_COMPONENT_USE_USED) |
                VIVS_GL_VARYING_COMPONENT_USE_COMP3(VARYING_COMPONENT_USE_USED) |
                VIVS_GL_VARYING_COMPONENT_USE_COMP4(VARYING_COMPONENT_USE_USED) |
                VIVS_GL_VARYING_COMPONENT_USE_COMP5(VARYING_COMPONENT_USE_USED)
                , 0
                });

        etna_set_state(ctx, VIVS_PA_W_CLIP_LIMIT, 0x34000001);
        etna_set_state(ctx, VIVS_PA_SYSTEM_MODE, 0x11);
        etna_set_state(ctx, VIVS_PA_CONFIG, /* VIVS_PA_CONFIG_UNK22 | */ (1 << 22) |
                                            VIVS_PA_CONFIG_CULL_FACE_MODE_CCW |
                                            VIVS_PA_CONFIG_FILL_MODE_SOLID |
                                            VIVS_PA_CONFIG_SHADE_MODEL_SMOOTH  |
                                            VIVS_PA_CONFIG_POINT_SIZE_ENABLE |
                                            VIVS_PA_CONFIG_POINT_SPRITE_ENABLE);
        etna_set_state_f32(ctx, VIVS_PA_VIEWPORT_OFFSET_Z, 0.0);
        etna_set_state_f32(ctx, VIVS_PA_VIEWPORT_SCALE_Z, 1.0);
        etna_set_state_fixp(ctx, VIVS_PA_VIEWPORT_OFFSET_X, width << 15);
        etna_set_state_fixp(ctx, VIVS_PA_VIEWPORT_OFFSET_Y, height << 15);
        etna_set_state_fixp(ctx, VIVS_PA_VIEWPORT_SCALE_X, width << 15);
        etna_set_state_fixp(ctx, VIVS_PA_VIEWPORT_SCALE_Y, height << 15);
        etna_set_state(ctx, VIVS_PA_ATTRIBUTE_ELEMENT_COUNT, 0x200);
        etna_set_state(ctx, VIVS_PA_SHADER_ATTRIBUTES(0), 0x200);
        etna_set_state(ctx, VIVS_PA_SHADER_ATTRIBUTES(1), 0x200);

        etna_set_state(ctx, VIVS_SE_CONFIG, 0x0);
        etna_set_state(ctx, VIVS_SE_DEPTH_SCALE, 0x0);
        etna_set_state(ctx, VIVS_SE_DEPTH_BIAS, 0x0);
        etna_set_state_fixp(ctx, VIVS_SE_SCISSOR_LEFT, 0);
        etna_set_state_fixp(ctx, VIVS_SE_SCISSOR_TOP, 0);
        etna_set_state_fixp(ctx, VIVS_SE_SCISSOR_RIGHT, (width << 16) | 5);
        etna_set_state_fixp(ctx, VIVS_SE_SCISSOR_BOTTOM, (height << 16) | 5);

        etna_set_state(ctx, VIVS_PE_ALPHA_CONFIG,
                /* VIVS_PE_ALPHA_CONFIG_BLEND_ENABLE_COLOR | */
                /* VIVS_PE_ALPHA_CONFIG_BLEND_ENABLE_ALPHA | */
                VIVS_PE_ALPHA_CONFIG_SRC_FUNC_COLOR(BLEND_FUNC_ONE) |
                VIVS_PE_ALPHA_CONFIG_SRC_FUNC_ALPHA(BLEND_FUNC_ONE) |
                VIVS_PE_ALPHA_CONFIG_DST_FUNC_COLOR(BLEND_FUNC_ZERO) |
                VIVS_PE_ALPHA_CONFIG_DST_FUNC_ALPHA(BLEND_FUNC_ZERO) |
                VIVS_PE_ALPHA_CONFIG_EQ_COLOR(BLEND_EQ_ADD) |
                VIVS_PE_ALPHA_CONFIG_EQ_ALPHA(BLEND_EQ_ADD));
        etna_set_state(ctx, VIVS_PE_ALPHA_BLEND_COLOR,
                VIVS_PE_ALPHA_BLEND_COLOR_B(0) |
                VIVS_PE_ALPHA_BLEND_COLOR_G(0) |
                VIVS_PE_ALPHA_BLEND_COLOR_R(0) |
                VIVS_PE_ALPHA_BLEND_COLOR_A(0));
        etna_set_state(ctx, VIVS_PE_ALPHA_OP, /* VIVS_PE_ALPHA_OP_ALPHA_TEST */ 0);
        etna_set_state(ctx, VIVS_PE_STENCIL_CONFIG, VIVS_PE_STENCIL_CONFIG_REF_FRONT(0) |
                                                    VIVS_PE_STENCIL_CONFIG_MASK_FRONT(0xff) |
							0xff000000 |
 /*                                                   VIVS_PE_STENCIL_CONFIG_WRITE_MASK(0xff) | */
                                                    VIVS_PE_STENCIL_CONFIG_MODE_DISABLED);
        etna_set_state(ctx, VIVS_PE_STENCIL_OP, VIVS_PE_STENCIL_OP_FUNC_FRONT(COMPARE_FUNC_ALWAYS) |
                                                VIVS_PE_STENCIL_OP_FUNC_BACK(COMPARE_FUNC_ALWAYS) |
                                                VIVS_PE_STENCIL_OP_FAIL_FRONT(STENCIL_OP_KEEP) |
                                                VIVS_PE_STENCIL_OP_FAIL_BACK(STENCIL_OP_KEEP) |
                                                VIVS_PE_STENCIL_OP_DEPTH_FAIL_FRONT(STENCIL_OP_KEEP) |
                                                VIVS_PE_STENCIL_OP_DEPTH_FAIL_BACK(STENCIL_OP_KEEP) |
                                                VIVS_PE_STENCIL_OP_PASS_FRONT(STENCIL_OP_KEEP) |
                                                VIVS_PE_STENCIL_OP_PASS_BACK(STENCIL_OP_KEEP));
        etna_set_state(ctx, VIVS_PE_COLOR_FORMAT,
                VIVS_PE_COLOR_FORMAT_COMPONENTS(0xf) |
                VIVS_PE_COLOR_FORMAT_FORMAT(RS_FORMAT_X8R8G8B8) |
                VIVS_PE_COLOR_FORMAT_SUPER_TILED |
                VIVS_PE_COLOR_FORMAT_OVERWRITE);
        etna_set_state(ctx, VIVS_PE_DEPTH_CONFIG,
                VIVS_PE_DEPTH_CONFIG_DEPTH_FORMAT_D16 |
                VIVS_PE_DEPTH_CONFIG_SUPER_TILED |
                VIVS_PE_DEPTH_CONFIG_EARLY_Z |
                /* VIVS_PE_DEPTH_CONFIG_WRITE_ENABLE | */
                VIVS_PE_DEPTH_CONFIG_DEPTH_FUNC(COMPARE_FUNC_ALWAYS) |
                VIVS_PE_DEPTH_CONFIG_DEPTH_MODE_Z
                /* VIVS_PE_DEPTH_CONFIG_ONLY_DEPTH */
                );
        etna_set_state(ctx, VIVS_PE_DEPTH_ADDR, etna_bo_gpu_address(z));
        etna_set_state(ctx, VIVS_PE_DEPTH_STRIDE, padded_width * 2);
        etna_set_state(ctx, VIVS_PE_HDEPTH_CONTROL, VIVS_PE_HDEPTH_CONTROL_FORMAT_DISABLED);
        etna_set_state_f32(ctx, VIVS_PE_DEPTH_NORMALIZE, 65535.0);
        etna_set_state(ctx, VIVS_PE_COLOR_ADDR, etna_bo_gpu_address(rt));
        etna_set_state(ctx, VIVS_PE_COLOR_STRIDE, padded_width * 4);
        etna_set_state_f32(ctx, VIVS_PE_DEPTH_NEAR, 0.0);
        etna_set_state_f32(ctx, VIVS_PE_DEPTH_FAR, 1.0);
        etna_set_state_f32(ctx, VIVS_PE_DEPTH_NORMALIZE, 65535.0);

        etna_set_state(ctx, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_COLOR | VIVS_GL_FLUSH_CACHE_DEPTH);
        etna_set_state(ctx, VIVS_TS_FLUSH_CACHE, VIVS_TS_FLUSH_CACHE_FLUSH);
        etna_stall(ctx, SYNC_RECIPIENT_RA, SYNC_RECIPIENT_PE);

        /* Set up the resolve to clear tile status for main render target
         * Regard the TS plane as an image of width 16 with 4 bytes per pixel (64 bytes per row)
         * XXX need to clear the depth ts too? we don't really use depth buffer in this sample
         * */
        etna_set_state(ctx, VIVS_TS_MEM_CONFIG, 0);
        etna_set_state(ctx, VIVS_RS_CONFIG,
                VIVS_RS_CONFIG_SOURCE_FORMAT(RS_FORMAT_X8R8G8B8) |
                VIVS_RS_CONFIG_DEST_FORMAT(RS_FORMAT_X8R8G8B8)
                );
        etna_set_state_multi(ctx, VIVS_RS_DITHER(0), 2, (uint32_t[]){0xffffffff, 0xffffffff});
        etna_set_state(ctx, VIVS_RS_DEST_ADDR, etna_bo_gpu_address(rt_ts)); /* ADDR_B */
        etna_set_state(ctx, VIVS_RS_DEST_STRIDE, 0x40);
#if 1
        etna_set_state(ctx, VIVS_RS_WINDOW_SIZE,
                //((rt_ts_size/0x40) << VIVS_RS_WINDOW_SIZE_HEIGHT__SHIFT) |
                ((2) << VIVS_RS_WINDOW_SIZE_HEIGHT__SHIFT) |
                (16 << VIVS_RS_WINDOW_SIZE_WIDTH__SHIFT));
#endif
        etna_set_state(ctx, VIVS_RS_FILL_VALUE(0), 0x55555555);
        etna_set_state(ctx, VIVS_RS_CLEAR_CONTROL,
                VIVS_RS_CLEAR_CONTROL_MODE_ENABLED1 |
                (0xffff << VIVS_RS_CLEAR_CONTROL_BITS__SHIFT));
        etna_set_state(ctx, VIVS_RS_EXTRA_CONFIG, 0);
        etna_set_state(ctx, VIVS_RS_KICKER, 0xbeebbeeb);
        /** Done */
	printf("trace %d\n", __LINE__);
        etna_finish(ctx);
#endif

    if (1)
    for(int frame=0; frame<100; ++frame)
    {
        printf("*** FRAME %i ****\n", frame);
        /* XXX part of this can be put outside the loop, but until we have usable context management
         * this is safest.
         */


#if 1
        etna_set_state(ctx, VIVS_GL_VERTEX_ELEMENT_CONFIG, 0x1);
        etna_set_state(ctx, VIVS_RA_CONTROL, 0x1);
        etna_set_state(ctx, VIVS_PA_W_CLIP_LIMIT, 0x34000001);
        etna_set_state(ctx, VIVS_PA_SYSTEM_MODE, 0x11);
        etna_set_state(ctx, VIVS_PA_CONFIG, ETNA_MASKED_BIT(VIVS_PA_CONFIG_WIDE_LINE , 0));
        etna_set_state(ctx, VIVS_SE_CONFIG, 0x0);
        etna_set_state(ctx, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_COLOR);
        etna_set_state(ctx, VIVS_PE_COLOR_FORMAT,
                ETNA_MASKED_BIT(VIVS_PE_COLOR_FORMAT_OVERWRITE, 0));

        etna_set_state(ctx, VIVS_PE_ALPHA_CONFIG,
                ETNA_MASKED_BIT(VIVS_PE_ALPHA_CONFIG_BLEND_ENABLE_COLOR, 0) &
                ETNA_MASKED_BIT(VIVS_PE_ALPHA_CONFIG_BLEND_SEPARATE_ALPHA, 0) &
                ETNA_MASKED(VIVS_PE_ALPHA_CONFIG_SRC_FUNC_COLOR, BLEND_FUNC_ONE) &
                ETNA_MASKED(VIVS_PE_ALPHA_CONFIG_SRC_FUNC_ALPHA, BLEND_FUNC_ONE) &
                ETNA_MASKED(VIVS_PE_ALPHA_CONFIG_DST_FUNC_COLOR, BLEND_FUNC_ZERO) &
                ETNA_MASKED(VIVS_PE_ALPHA_CONFIG_DST_FUNC_ALPHA, BLEND_FUNC_ZERO) &
                ETNA_MASKED(VIVS_PE_ALPHA_CONFIG_EQ_COLOR, BLEND_EQ_ADD) &
                ETNA_MASKED(VIVS_PE_ALPHA_CONFIG_EQ_ALPHA, BLEND_EQ_ADD));
        etna_set_state(ctx, VIVS_PE_ALPHA_BLEND_COLOR,
                VIVS_PE_ALPHA_BLEND_COLOR_B(0) |
                VIVS_PE_ALPHA_BLEND_COLOR_G(0) |
                VIVS_PE_ALPHA_BLEND_COLOR_R(0) |
                VIVS_PE_ALPHA_BLEND_COLOR_A(0));
        etna_set_state(ctx, VIVS_PE_ALPHA_OP, ETNA_MASKED_BIT(VIVS_PE_ALPHA_OP_ALPHA_TEST, 0));
        etna_set_state(ctx, VIVS_PA_CONFIG, ETNA_MASKED_INL(VIVS_PA_CONFIG_CULL_FACE_MODE, OFF));
        etna_set_state(ctx, VIVS_PE_DEPTH_CONFIG, ETNA_MASKED_BIT(VIVS_PE_DEPTH_CONFIG_WRITE_ENABLE, 0));
        etna_set_state(ctx, VIVS_PE_STENCIL_CONFIG, ETNA_MASKED(VIVS_PE_STENCIL_CONFIG_REF_FRONT, 0) &
                                                    ETNA_MASKED(VIVS_PE_STENCIL_CONFIG_MASK_FRONT, 0xff) &
                                                    /* ETNA_MASKED(VIVS_PE_STENCIL_CONFIG_WRITE_MASK, 0xff) & */
                                                    ETNA_MASKED_INL(VIVS_PE_STENCIL_CONFIG_MODE, DISABLED));
        etna_set_state(ctx, VIVS_PE_STENCIL_OP, ETNA_MASKED(VIVS_PE_STENCIL_OP_FUNC_FRONT, COMPARE_FUNC_ALWAYS) &
                                                ETNA_MASKED(VIVS_PE_STENCIL_OP_FUNC_BACK, COMPARE_FUNC_ALWAYS) &
                                                ETNA_MASKED(VIVS_PE_STENCIL_OP_FAIL_FRONT, STENCIL_OP_KEEP) &
                                                ETNA_MASKED(VIVS_PE_STENCIL_OP_FAIL_BACK, STENCIL_OP_KEEP) &
                                                ETNA_MASKED(VIVS_PE_STENCIL_OP_DEPTH_FAIL_FRONT, STENCIL_OP_KEEP) &
                                                ETNA_MASKED(VIVS_PE_STENCIL_OP_DEPTH_FAIL_BACK, STENCIL_OP_KEEP) &
                                                ETNA_MASKED(VIVS_PE_STENCIL_OP_PASS_FRONT, STENCIL_OP_KEEP) &
                                                ETNA_MASKED(VIVS_PE_STENCIL_OP_PASS_BACK, STENCIL_OP_KEEP));

        etna_set_state(ctx, VIVS_PE_COLOR_FORMAT, ETNA_MASKED(VIVS_PE_COLOR_FORMAT_COMPONENTS, 0xf));

        etna_set_state(ctx, VIVS_PE_DEPTH_CONFIG, ETNA_MASKED_BIT(VIVS_PE_DEPTH_CONFIG_EARLY_Z, 0));
        etna_set_state(ctx, VIVS_SE_DEPTH_SCALE, 0x0);
        etna_set_state(ctx, VIVS_SE_DEPTH_BIAS, 0x0);

        etna_set_state(ctx, VIVS_PA_CONFIG, ETNA_MASKED_INL(VIVS_PA_CONFIG_FILL_MODE, SOLID));
        etna_set_state(ctx, VIVS_PA_CONFIG, ETNA_MASKED_INL(VIVS_PA_CONFIG_SHADE_MODEL, SMOOTH));
        etna_set_state(ctx, VIVS_PE_COLOR_FORMAT,
                ETNA_MASKED(VIVS_PE_COLOR_FORMAT_FORMAT, RS_FORMAT_X8R8G8B8) &
                ETNA_MASKED_BIT(VIVS_PE_COLOR_FORMAT_SUPER_TILED, supertiled));

        etna_set_state(ctx, VIVS_PE_COLOR_ADDR, etna_bo_gpu_address(rt)); /* ADDR_A */

        etna_set_state(ctx, VIVS_PE_COLOR_STRIDE, padded_width * 4);
        etna_set_state(ctx, VIVS_GL_MULTI_SAMPLE_CONFIG,
                ETNA_MASKED_INL(VIVS_GL_MULTI_SAMPLE_CONFIG_MSAA_SAMPLES, NONE) &
                ETNA_MASKED(VIVS_GL_MULTI_SAMPLE_CONFIG_MSAA_ENABLES, 0xf) &
                ETNA_MASKED(VIVS_GL_MULTI_SAMPLE_CONFIG_UNK12, 0x0) &
                ETNA_MASKED(VIVS_GL_MULTI_SAMPLE_CONFIG_UNK16, 0x0)
                );
        etna_set_state(ctx, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_COLOR);
        etna_set_state(ctx, VIVS_PE_COLOR_FORMAT, ETNA_MASKED_BIT(VIVS_PE_COLOR_FORMAT_OVERWRITE, 1));
        etna_set_state(ctx, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_COLOR);
        etna_set_state(ctx, VIVS_TS_COLOR_CLEAR_VALUE, 0);
        etna_set_state(ctx, VIVS_TS_COLOR_STATUS_BASE, etna_bo_gpu_address(rt_ts)); /* ADDR_B */
        etna_set_state(ctx, VIVS_TS_COLOR_SURFACE_BASE, etna_bo_gpu_address(rt)); /* ADDR_A */
        etna_set_state(ctx, VIVS_TS_MEM_CONFIG, VIVS_TS_MEM_CONFIG_COLOR_FAST_CLEAR);

        etna_set_state(ctx, VIVS_PE_DEPTH_CONFIG,
                ETNA_MASKED_INL(VIVS_PE_DEPTH_CONFIG_DEPTH_FORMAT, D16) &
                ETNA_MASKED_BIT(VIVS_PE_DEPTH_CONFIG_SUPER_TILED, supertiled)
                );
        etna_set_state(ctx, VIVS_PE_DEPTH_ADDR, etna_bo_gpu_address(z)); /* ADDR_C */
        etna_set_state(ctx, VIVS_PE_DEPTH_STRIDE, padded_width * 2);
        etna_set_state(ctx, VIVS_PE_HDEPTH_CONTROL, VIVS_PE_HDEPTH_CONTROL_FORMAT_DISABLED);
        etna_set_state_f32(ctx, VIVS_PE_DEPTH_NORMALIZE, 65535.0);
        etna_set_state(ctx, VIVS_PE_DEPTH_CONFIG, ETNA_MASKED_BIT(VIVS_PE_DEPTH_CONFIG_EARLY_Z, 0));
        etna_set_state(ctx, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_DEPTH);

        etna_set_state(ctx, VIVS_TS_DEPTH_CLEAR_VALUE, 0xffffffff);
        etna_set_state(ctx, VIVS_TS_DEPTH_STATUS_BASE, etna_bo_gpu_address(z_ts)); /* ADDR_D */
        etna_set_state(ctx, VIVS_TS_DEPTH_SURFACE_BASE, etna_bo_gpu_address(z)); /* ADDR_C */
        etna_set_state(ctx, VIVS_TS_MEM_CONFIG,
                VIVS_TS_MEM_CONFIG_DEPTH_FAST_CLEAR |
                VIVS_TS_MEM_CONFIG_COLOR_FAST_CLEAR |
                VIVS_TS_MEM_CONFIG_DEPTH_16BPP |
                VIVS_TS_MEM_CONFIG_DEPTH_COMPRESSION);
        etna_set_state(ctx, VIVS_PE_DEPTH_CONFIG, ETNA_MASKED_BIT(VIVS_PE_DEPTH_CONFIG_EARLY_Z, 1)); /* flip-flopping once again */

        /* Warm up RS on aux render target */
        etna_set_state(ctx, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_COLOR | VIVS_GL_FLUSH_CACHE_DEPTH);
#ifdef WARM2
        etna_warm_up_rs2(ctx, aux_rt, aux_rt_ts);
#else
        etna_warm_up_rs(ctx, etna_bo_gpu_address(aux_rt), etna_bo_gpu_address(aux_rt_ts));
#endif
#endif
        //etna_finish(ctx);
#if 0
	printf("trace %d\n", __LINE__);
        etna_finish(ctx);
	break;
#endif



        etna_set_state(ctx, VIVS_TS_COLOR_STATUS_BASE, etna_bo_gpu_address(rt_ts)); /* ADDR_B */
        etna_set_state(ctx, VIVS_TS_COLOR_SURFACE_BASE, etna_bo_gpu_address(rt)); /* ADDR_A */

        etna_set_state(ctx, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_COLOR | VIVS_GL_FLUSH_CACHE_DEPTH);
#ifdef WARM2
        etna_warm_up_rs2(ctx, aux_rt, aux_rt_ts);
#else
        etna_warm_up_rs(ctx, etna_bo_gpu_address(aux_rt), etna_bo_gpu_address(aux_rt_ts));
#endif
        etna_set_state(ctx, VIVS_TS_COLOR_STATUS_BASE, etna_bo_gpu_address(rt_ts)); /* ADDR_B */
        etna_set_state(ctx, VIVS_TS_COLOR_SURFACE_BASE, etna_bo_gpu_address(rt)); /* ADDR_A */
        etna_set_state(ctx, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_COLOR | VIVS_GL_FLUSH_CACHE_DEPTH);

        etna_set_state(ctx, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_COLOR | VIVS_GL_FLUSH_CACHE_DEPTH);
#ifdef WARM2
        etna_warm_up_rs2(ctx, aux_rt, aux_rt_ts);
#else
        etna_warm_up_rs(ctx, etna_bo_gpu_address(aux_rt), etna_bo_gpu_address(aux_rt_ts));
#endif
        etna_set_state(ctx, VIVS_TS_COLOR_STATUS_BASE, etna_bo_gpu_address(rt_ts)); /* ADDR_B */
        etna_set_state(ctx, VIVS_TS_COLOR_SURFACE_BASE, etna_bo_gpu_address(rt)); /* ADDR_A */

        etna_stall(ctx, SYNC_RECIPIENT_RA, SYNC_RECIPIENT_PE);

        /* Set up the resolve to clear tile status for main render target
         * Regard the TS as an image of width 16 with 4 bytes per pixel (64 bytes per row)
         * XXX need to clear the depth ts too.
         * */
        etna_set_state(ctx, VIVS_RS_CONFIG,
                (RS_FORMAT_X8R8G8B8 << VIVS_RS_CONFIG_SOURCE_FORMAT__SHIFT) |
                (RS_FORMAT_X8R8G8B8 << VIVS_RS_CONFIG_DEST_FORMAT__SHIFT)
                );
        etna_set_state_multi(ctx, VIVS_RS_DITHER(0), 2, (uint32_t[]){0xffffffff, 0xffffffff});
        etna_set_state(ctx, VIVS_RS_DEST_ADDR, etna_bo_gpu_address(rt_ts)); /* ADDR_B */
        etna_set_state(ctx, VIVS_RS_DEST_STRIDE, 0x40);
        etna_set_state(ctx, VIVS_RS_WINDOW_SIZE,
                //((rt_ts_size/0x40) << VIVS_RS_WINDOW_SIZE_HEIGHT__SHIFT) |
                ((2) << VIVS_RS_WINDOW_SIZE_HEIGHT__SHIFT) |
                (16 << VIVS_RS_WINDOW_SIZE_WIDTH__SHIFT));
        etna_set_state(ctx, VIVS_RS_FILL_VALUE(0), (bits_per_tile==4)?0x11111111:0x55555555);
        etna_set_state(ctx, VIVS_RS_CLEAR_CONTROL,
                VIVS_RS_CLEAR_CONTROL_MODE_ENABLED1 |
                (0xffff << VIVS_RS_CLEAR_CONTROL_BITS__SHIFT));
        etna_set_state(ctx, VIVS_RS_EXTRA_CONFIG,
                0); /* no AA, no endian switch */
        etna_set_state(ctx, VIVS_RS_KICKER,
                0xbeebbeeb); /* PROBLEM */
        /** Done */

        etna_set_state(ctx, VIVS_TS_COLOR_CLEAR_VALUE, 0xff7f7f7f);
        etna_set_state(ctx, VIVS_TS_COLOR_STATUS_BASE, etna_bo_gpu_address(rt_ts)); /* ADDR_B */
        etna_set_state(ctx, VIVS_TS_COLOR_SURFACE_BASE, etna_bo_gpu_address(rt)); /* ADDR_A */
        etna_set_state(ctx, VIVS_TS_MEM_CONFIG,
                VIVS_TS_MEM_CONFIG_DEPTH_FAST_CLEAR |
                VIVS_TS_MEM_CONFIG_COLOR_FAST_CLEAR |
                VIVS_TS_MEM_CONFIG_DEPTH_16BPP |
                VIVS_TS_MEM_CONFIG_DEPTH_COMPRESSION);
        etna_set_state(ctx, VIVS_PA_CONFIG, ETNA_MASKED_INL(VIVS_PA_CONFIG_CULL_FACE_MODE, CCW));
        etna_set_state(ctx, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_COLOR | VIVS_GL_FLUSH_CACHE_DEPTH);

        etna_set_state(ctx, VIVS_PE_DEPTH_CONFIG, ETNA_MASKED_BIT(VIVS_PE_DEPTH_CONFIG_WRITE_ENABLE, 0));
        etna_set_state(ctx, VIVS_PE_DEPTH_CONFIG, ETNA_MASKED_INL(VIVS_PE_DEPTH_CONFIG_DEPTH_MODE, NONE));
        etna_set_state(ctx, VIVS_PE_DEPTH_CONFIG, ETNA_MASKED_BIT(VIVS_PE_DEPTH_CONFIG_WRITE_ENABLE, 0));
        etna_set_state(ctx, VIVS_PE_DEPTH_CONFIG, ETNA_MASKED(VIVS_PE_DEPTH_CONFIG_DEPTH_FUNC, COMPARE_FUNC_ALWAYS));
        etna_set_state(ctx, VIVS_PE_DEPTH_CONFIG, ETNA_MASKED_INL(VIVS_PE_DEPTH_CONFIG_DEPTH_MODE, Z));
        etna_set_state_f32(ctx, VIVS_PE_DEPTH_NEAR, 0.0);
        etna_set_state_f32(ctx, VIVS_PE_DEPTH_FAR, 1.0);
        etna_set_state_f32(ctx, VIVS_PE_DEPTH_NORMALIZE, 65535.0);

        /* set up primitive assembly */
        etna_set_state_f32(ctx, VIVS_PA_VIEWPORT_OFFSET_Z, 0.0);
        etna_set_state_f32(ctx, VIVS_PA_VIEWPORT_SCALE_Z, 1.0);
        etna_set_state(ctx, VIVS_PE_DEPTH_CONFIG, ETNA_MASKED_BIT(VIVS_PE_DEPTH_CONFIG_ONLY_DEPTH, 0));
        etna_set_state_fixp(ctx, VIVS_PA_VIEWPORT_OFFSET_X, width << 15);
        etna_set_state_fixp(ctx, VIVS_PA_VIEWPORT_OFFSET_Y, height << 15);
        etna_set_state_fixp(ctx, VIVS_PA_VIEWPORT_SCALE_X, width << 15);
        etna_set_state_fixp(ctx, VIVS_PA_VIEWPORT_SCALE_Y, height << 15);
        etna_set_state_fixp(ctx, VIVS_SE_SCISSOR_LEFT, 0);
        etna_set_state_fixp(ctx, VIVS_SE_SCISSOR_TOP, 0);
        etna_set_state_fixp(ctx, VIVS_SE_SCISSOR_RIGHT, (width << 16) | 5);
        etna_set_state_fixp(ctx, VIVS_SE_SCISSOR_BOTTOM, (height << 16) | 5);

        /* shader setup */
        etna_set_state(ctx, VIVS_VS_END_PC, vs_size/16);
        etna_set_state_multi(ctx, VIVS_VS_INPUT_COUNT, 3, (uint32_t[]){
                /* VIVS_VS_INPUT_COUNT */ (1<<8) | 3,
                /* VIVS_VS_TEMP_REGISTER_CONTROL */ 6 << VIVS_VS_TEMP_REGISTER_CONTROL_NUM_TEMPS__SHIFT,
                /* VIVS_VS_OUTPUT(0) */ 4});
        etna_set_state(ctx, VIVS_VS_START_PC, 0x0);
        etna_set_state_f32(ctx, VIVS_VS_UNIFORMS(45), 0.5); /* u11.y */
        etna_set_state_f32(ctx, VIVS_VS_UNIFORMS(44), 1.0); /* u11.x */
        etna_set_state_f32(ctx, VIVS_VS_UNIFORMS(27), 0.0); /* u6.w */
        etna_set_state_f32(ctx, VIVS_VS_UNIFORMS(23), 20.0); /* u5.w */
        etna_set_state_f32(ctx, VIVS_VS_UNIFORMS(19), 2.0); /* u4.w */

        /* Now load the shader itself */
        etna_set_state_multi(ctx, VIVS_VS_INST_MEM(0), vs_size/4, vs);
        etna_set_state(ctx, VIVS_RA_CONTROL, 0x1);
        etna_set_state_multi(ctx, VIVS_PS_END_PC, 2, (uint32_t[]){
                /* VIVS_PS_END_PC */ ps_size/16,
                /* VIVS_PS_OUTPUT_REG */ 0x1});
        etna_set_state(ctx, VIVS_PS_START_PC, 0x0);
        etna_set_state(ctx, VIVS_PA_SHADER_ATTRIBUTES(0), 0x200);
        etna_set_state(ctx, VIVS_GL_VARYING_NUM_COMPONENTS,  /* one varying, with four components */
                (4 << VIVS_GL_VARYING_NUM_COMPONENTS_VAR0__SHIFT)
                );
        etna_set_state_multi(ctx, VIVS_GL_VARYING_COMPONENT_USE(0), 2, (uint32_t[]){ /* one varying, with four components */
                (VARYING_COMPONENT_USE_USED << VIVS_GL_VARYING_COMPONENT_USE_COMP0__SHIFT) |
                (VARYING_COMPONENT_USE_USED << VIVS_GL_VARYING_COMPONENT_USE_COMP1__SHIFT) |
                (VARYING_COMPONENT_USE_USED << VIVS_GL_VARYING_COMPONENT_USE_COMP2__SHIFT) |
                (VARYING_COMPONENT_USE_USED << VIVS_GL_VARYING_COMPONENT_USE_COMP3__SHIFT)
                , 0
                });
        etna_set_state_multi(ctx, VIVS_PS_INST_MEM(0), ps_size/4, ps);
        etna_set_state(ctx, VIVS_PS_INPUT_COUNT, (31<<8)|2);
        etna_set_state(ctx, VIVS_PS_TEMP_REGISTER_CONTROL,
                (2 << VIVS_PS_TEMP_REGISTER_CONTROL_NUM_TEMPS__SHIFT));
        etna_set_state(ctx, VIVS_PS_CONTROL,
                VIVS_PS_CONTROL_UNK1
                );
        etna_set_state(ctx, VIVS_PA_ATTRIBUTE_ELEMENT_COUNT, 0x100);
        etna_set_state(ctx, VIVS_GL_VARYING_TOTAL_COMPONENTS,  /* one varying, with four components */
                VIVS_GL_VARYING_TOTAL_COMPONENTS_NUM(4)
                );
        etna_set_state(ctx, VIVS_VS_LOAD_BALANCING, 0xf3f0582);
        etna_set_state(ctx, VIVS_VS_OUTPUT_COUNT, 2);
        etna_set_state(ctx, VIVS_PA_CONFIG, ETNA_MASKED_BIT(VIVS_PA_CONFIG_POINT_SIZE_ENABLE, 0));

        /*   Compute transform matrices in the same way as cube egl demo */
        ESMatrix modelview;
        esMatrixLoadIdentity(&modelview);
        esTranslate(&modelview, 0.0f, 0.0f, -8.0f);
        esRotate(&modelview, 45.0f, 1.0f, 0.0f, 0.0f);
        esRotate(&modelview, 45.0f, 0.0f, 1.0f, 0.0f);
        esRotate(&modelview, frame*0.5f, 0.0f, 0.0f, 1.0f);

        float aspect = (float)(height) / (float)(width);

        ESMatrix projection;
        esMatrixLoadIdentity(&projection);
        esFrustum(&projection, -2.8f, +2.8f, -2.8f * aspect, +2.8f * aspect, 6.0f, 10.0f);

        ESMatrix modelviewprojection;
        esMatrixLoadIdentity(&modelviewprojection);
        esMatrixMultiply(&modelviewprojection, &modelview, &projection);

        ESMatrix inverse, normal; /* compute inverse transpose normal transformation matrix */
        esMatrixInverse3x3(&inverse, &modelview);
        esMatrixTranspose(&normal, &inverse);

        etna_set_state_multi(ctx, VIVS_VS_UNIFORMS(0), 16, (uint32_t*)&modelviewprojection.m[0][0]);
        etna_set_state_multi(ctx, VIVS_VS_UNIFORMS(16), 3, (uint32_t*)&normal.m[0][0]); /* u4.xyz */
        etna_set_state_multi(ctx, VIVS_VS_UNIFORMS(20), 3, (uint32_t*)&normal.m[1][0]); /* u5.xyz */
        etna_set_state_multi(ctx, VIVS_VS_UNIFORMS(24), 3, (uint32_t*)&normal.m[2][0]); /* u6.xyz */
        etna_set_state_multi(ctx, VIVS_VS_UNIFORMS(28), 16, (uint32_t*)&modelview.m[0][0]);
        etna_set_state(ctx, VIVS_FE_VERTEX_STREAM_BASE_ADDR, etna_bo_gpu_address(vtx)); /* ADDR_E */
        etna_set_state(ctx, VIVS_FE_VERTEX_STREAM_CONTROL,
                0x24 << 0 /*VIVS_FE_VERTEX_STREAM_CONTROL_VERTEX_STRIDE__SHIFT*/);
        etna_set_state(ctx, VIVS_FE_VERTEX_ELEMENT_CONFIG(0),
                VIVS_FE_VERTEX_ELEMENT_CONFIG_TYPE_FLOAT |
                (ENDIAN_MODE_NO_SWAP << VIVS_FE_VERTEX_ELEMENT_CONFIG_ENDIAN__SHIFT) |
                (0 << VIVS_FE_VERTEX_ELEMENT_CONFIG_STREAM__SHIFT) |
                (3 <<VIVS_FE_VERTEX_ELEMENT_CONFIG_NUM__SHIFT) |
                VIVS_FE_VERTEX_ELEMENT_CONFIG_NORMALIZE_OFF |
                (0x0 << VIVS_FE_VERTEX_ELEMENT_CONFIG_START__SHIFT) |
                (0xc << VIVS_FE_VERTEX_ELEMENT_CONFIG_END__SHIFT));
        etna_set_state(ctx, VIVS_FE_VERTEX_ELEMENT_CONFIG(1),
                VIVS_FE_VERTEX_ELEMENT_CONFIG_TYPE_FLOAT |
                (ENDIAN_MODE_NO_SWAP << VIVS_FE_VERTEX_ELEMENT_CONFIG_ENDIAN__SHIFT) |
                (0 << VIVS_FE_VERTEX_ELEMENT_CONFIG_STREAM__SHIFT) |
                (3 <<VIVS_FE_VERTEX_ELEMENT_CONFIG_NUM__SHIFT) |
                VIVS_FE_VERTEX_ELEMENT_CONFIG_NORMALIZE_OFF |
                (0xc << VIVS_FE_VERTEX_ELEMENT_CONFIG_START__SHIFT) |
                (0x18 << VIVS_FE_VERTEX_ELEMENT_CONFIG_END__SHIFT));
        etna_set_state(ctx, VIVS_FE_VERTEX_ELEMENT_CONFIG(2),
                VIVS_FE_VERTEX_ELEMENT_CONFIG_TYPE_FLOAT |
                (ENDIAN_MODE_NO_SWAP << VIVS_FE_VERTEX_ELEMENT_CONFIG_ENDIAN__SHIFT) |
                VIVS_FE_VERTEX_ELEMENT_CONFIG_NONCONSECUTIVE |
                (0 << VIVS_FE_VERTEX_ELEMENT_CONFIG_STREAM__SHIFT) |
                (3 <<VIVS_FE_VERTEX_ELEMENT_CONFIG_NUM__SHIFT) |
                VIVS_FE_VERTEX_ELEMENT_CONFIG_NORMALIZE_OFF |
                (0x18 << VIVS_FE_VERTEX_ELEMENT_CONFIG_START__SHIFT) |
                (0x24 << VIVS_FE_VERTEX_ELEMENT_CONFIG_END__SHIFT));
        etna_set_state(ctx, VIVS_VS_INPUT(0), 0x20100);
        etna_set_state(ctx, VIVS_PA_CONFIG, ETNA_MASKED_BIT(VIVS_PA_CONFIG_POINT_SPRITE_ENABLE, 0));

        for(int prim=0; prim<6; ++prim)
        {
            etna_draw_primitives(ctx, PRIMITIVE_TYPE_TRIANGLE_STRIP, prim*4, 2);
        }
        etna_set_state(ctx, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_COLOR | VIVS_GL_FLUSH_CACHE_DEPTH);

	printf("trace %d\n", __LINE__);
        etna_flush(ctx, NULL);
	printf("trace %d\n", __LINE__);

        etna_set_state(ctx, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_COLOR | VIVS_GL_FLUSH_CACHE_DEPTH);
        etna_set_state(ctx, VIVS_RS_CONFIG,
                VIVS_RS_CONFIG_SOURCE_FORMAT(RS_FORMAT_X8R8G8B8) |
                VIVS_RS_CONFIG_SOURCE_TILED |
                VIVS_RS_CONFIG_DEST_FORMAT(RS_FORMAT_X8R8G8B8) |
                VIVS_RS_CONFIG_DEST_TILED);
        etna_set_state(ctx, VIVS_RS_SOURCE_STRIDE, (padded_width * 4 * 4) | (supertiled?VIVS_RS_SOURCE_STRIDE_TILING:0));
        etna_set_state(ctx, VIVS_RS_DEST_STRIDE, (padded_width * 4 * 4) | (supertiled?VIVS_RS_DEST_STRIDE_TILING:0));
        etna_set_state(ctx, VIVS_RS_DITHER(0), 0xffffffff);
        etna_set_state(ctx, VIVS_RS_DITHER(1), 0xffffffff);
        etna_set_state(ctx, VIVS_RS_CLEAR_CONTROL, VIVS_RS_CLEAR_CONTROL_MODE_DISABLED);
        etna_set_state(ctx, VIVS_RS_EXTRA_CONFIG, 0); /* no AA, no endian switch */
        etna_set_state(ctx, VIVS_RS_SOURCE_ADDR, etna_bo_gpu_address(rt)); /* ADDR_A */
        etna_set_state(ctx, VIVS_RS_DEST_ADDR, etna_bo_gpu_address(rt)); /* ADDR_A */
        etna_set_state(ctx, VIVS_RS_WINDOW_SIZE,
                VIVS_RS_WINDOW_SIZE_HEIGHT(padded_height) |
                VIVS_RS_WINDOW_SIZE_WIDTH(padded_width));
        etna_set_state(ctx, VIVS_RS_KICKER, 0xbeebbeeb);

        /* Submit second command buffer */
	printf("trace %d\n", __LINE__);
        etna_flush(ctx, NULL);

#ifdef WARM2
        etna_warm_up_rs2(ctx, aux_rt, aux_rt_ts);
#else
        etna_warm_up_rs(ctx, etna_bo_gpu_address(aux_rt), etna_bo_gpu_address(aux_rt_ts));
#endif
        etna_set_state(ctx, VIVS_TS_COLOR_STATUS_BASE, etna_bo_gpu_address(rt_ts)); /* ADDR_B */
        etna_set_state(ctx, VIVS_TS_COLOR_SURFACE_BASE, etna_bo_gpu_address(rt)); /* ADDR_A */
        etna_set_state(ctx, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_COLOR);
        etna_set_state(ctx, VIVS_TS_MEM_CONFIG,
                VIVS_TS_MEM_CONFIG_DEPTH_FAST_CLEAR |
                VIVS_TS_MEM_CONFIG_DEPTH_16BPP |
                VIVS_TS_MEM_CONFIG_DEPTH_COMPRESSION);
        etna_set_state(ctx, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_COLOR);
        etna_set_state(ctx, VIVS_PE_COLOR_FORMAT,
                ETNA_MASKED_BIT(VIVS_PE_COLOR_FORMAT_OVERWRITE, 0));

        /* Submit third command buffer, wait for pixel engine to finish */
        etna_finish(ctx);

        etna_set_state(ctx, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_COLOR | VIVS_GL_FLUSH_CACHE_DEPTH);
        etna_set_state(ctx, VIVS_RS_CONFIG,
                VIVS_RS_CONFIG_SOURCE_FORMAT(RS_FORMAT_X8R8G8B8) |
                VIVS_RS_CONFIG_SOURCE_TILED |
                VIVS_RS_CONFIG_DEST_FORMAT(RS_FORMAT_X8R8G8B8) /*|
                VIVS_RS_CONFIG_SWAP_RB*/);
        etna_set_state(ctx, VIVS_RS_SOURCE_STRIDE, (padded_width * 4 * 4) | (supertiled?VIVS_RS_SOURCE_STRIDE_TILING:0));
        etna_set_state(ctx, VIVS_RS_DEST_STRIDE, width * 4);
        etna_set_state(ctx, VIVS_RS_DITHER(0), 0xffffffff);
        etna_set_state(ctx, VIVS_RS_DITHER(1), 0xffffffff);
        etna_set_state(ctx, VIVS_RS_CLEAR_CONTROL, VIVS_RS_CLEAR_CONTROL_MODE_DISABLED);
        etna_set_state(ctx, VIVS_RS_EXTRA_CONFIG, 0); /* no AA, no endian switch */
        etna_set_state(ctx, VIVS_RS_SOURCE_ADDR, etna_bo_gpu_address(rt));
        etna_set_state(ctx, VIVS_RS_DEST_ADDR, etna_bo_gpu_address(bmp));
        etna_set_state(ctx, VIVS_RS_WINDOW_SIZE,
                VIVS_RS_WINDOW_SIZE_HEIGHT(height) |
                VIVS_RS_WINDOW_SIZE_WIDTH(width));
        //etna_set_state(ctx, VIVS_RS_KICKER, 0xbeebbeeb);
	printf("trace %d\n", __LINE__);
        etna_finish(ctx);
    }

        etna_flush(ctx, NULL);
	uint32_t *ptr = etna_bo_map(bmp);
	ptr = etna_bo_map(rt_ts);
	for (int i = 0; i < height; i++) {
		pix_fmt_convert(ptr, &ipu_fb[i * 1280], width, RGB888, RGB565);
		ptr += width;
	}

	printf("Look for non-zero elements...\n");

	ptr = etna_bo_map(rt_ts);
	for (int i = 0; i < rt_ts_size; i++, ptr++) {
		if (*ptr != 0) {
			printf("rt_ts non-zero %d=%08x\n", i, *ptr);
		}
	}

	ptr = etna_bo_map(rt);
	for (int i = 0; i < rt_size; i++, ptr++) {
		if (*ptr != 0) {
			printf("rt non-zero %d=%08x\n", i, *ptr);
		}
	}

	ptr = etna_bo_map(z);
	for (int i = 0; i < z_size; i++, ptr++) {
		if (*ptr != 0) {
			printf("z non-zero %d=%08x\n", i, *ptr);
		}
	}

	ptr = etna_bo_map(z_ts);
	for (int i = 0; i < z_ts_size; i++, ptr++) {
		if (*ptr != 0) {
			printf("z_ts non-zero %d=%08x\n", i, *ptr);
		}
	}

	ptr = etna_bo_map(bmp);
	for (int i = 0; i < bmp_size; i++, ptr++) {
		if (*ptr != 0) {
			printf("tmp non-zero %d=%08x\n", i, *ptr);
		}
	}

    //bmp_dump32(etna_bo_map(bmp), width, height, false, "/tmp/fb.bmp");
    printf("Dump complete\n");

    //etna_free(ctx);
 //   viv_close(conn);
    return 0;
}