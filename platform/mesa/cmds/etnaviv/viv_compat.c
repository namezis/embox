#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdarg.h>

#include <drivers/video/fb.h>

#include <errno.h>

#include "common.xml.h"
#include "state.xml.h"
#include "state_3d.xml.h"
#include "cmdstream.xml.h"

#include "write_bmp.h"
#include "viv.h"
//#include "etna.h"
//#include "etna_state.h"
//#include "etna_rs.h"
//#include "etna_fb.h"
//#include "etna_bo.h"
#include "etna_util.h"

//#include "xf86drm.h"
//#include "etnaviv_drmif.h"
#include <etnaviv/etnaviv_drm.h>

//#include "gc_hal_options.h"
//#include "gc_abi.h"
//#include "gc_hal_enum.h"

int viv_open(enum viv_hw_type type, struct viv_conn **conn) {
	static struct viv_conn viv_conn;
	struct drm_etnaviv_param prm;

	*conn = &viv_conn;

	prm.pipe = type == VIV_HW_3D ? PIPE_ID_PIPE_3D : PIPE_ID_PIPE_2D;

	viv_conn = (struct viv_conn) {
		.fd = open("/dev/card", O_RDWR),
		.hw_type = type,
	};

	prm.param = ETNAVIV_PARAM_GPU_MODEL;
	ioctl(viv_conn.fd, DRM_COMMAND_BASE + DRM_ETNAVIV_GET_PARAM, &prm);
	viv_conn.chip.chip_model = prm.value;

	prm.param = ETNAVIV_PARAM_GPU_REVISION;
	ioctl(viv_conn.fd, DRM_COMMAND_BASE + DRM_ETNAVIV_GET_PARAM, &prm);
	viv_conn.chip.chip_revision = prm.value;

	prm.param = ETNAVIV_PARAM_GPU_FEATURES_0;
	ioctl(viv_conn.fd, DRM_COMMAND_BASE + DRM_ETNAVIV_GET_PARAM, &prm);
	viv_conn.chip.chip_features[0] = prm.value;

	prm.param = ETNAVIV_PARAM_GPU_FEATURES_1;
	ioctl(viv_conn.fd, DRM_COMMAND_BASE + DRM_ETNAVIV_GET_PARAM, &prm);
	viv_conn.chip.chip_features[1] = prm.value;

	prm.param = ETNAVIV_PARAM_GPU_FEATURES_2;
	ioctl(viv_conn.fd, DRM_COMMAND_BASE + DRM_ETNAVIV_GET_PARAM, &prm);
	viv_conn.chip.chip_features[2] = prm.value;

	prm.param = ETNAVIV_PARAM_GPU_FEATURES_3;
	ioctl(viv_conn.fd, DRM_COMMAND_BASE + DRM_ETNAVIV_GET_PARAM, &prm);
	viv_conn.chip.chip_features[3] = prm.value;

	prm.param = ETNAVIV_PARAM_GPU_FEATURES_4;
	ioctl(viv_conn.fd, DRM_COMMAND_BASE + DRM_ETNAVIV_GET_PARAM, &prm);
	viv_conn.chip.chip_features[3] = prm.value;

	prm.param = ETNAVIV_PARAM_GPU_FEATURES_5;
	ioctl(viv_conn.fd, DRM_COMMAND_BASE + DRM_ETNAVIV_GET_PARAM, &prm);
	viv_conn.chip.chip_features[3] = prm.value;

	prm.param = ETNAVIV_PARAM_GPU_FEATURES_6;
	ioctl(viv_conn.fd, DRM_COMMAND_BASE + DRM_ETNAVIV_GET_PARAM, &prm);
	viv_conn.chip.chip_features[3] = prm.value;

	return 0;
}

