#https://mesa.freedesktop.org/archive/mesa-17.0.3.tar.xz

PKG_NAME := mesa
PKG_VER  := 17.0.3

PKG_ARCHIVE_NAME := $(PKG_NAME)-$(PKG_VER).tar.xz

PKG_SOURCES := https://mesa.freedesktop.org/archive/$(PKG_ARCHIVE_NAME)

PKG_MD5     := 003072c044b3fb5eca8be2037f15e4ec

PKG_PATCHES := patch.txt

include $(EXTBLD_LIB)


MESA_CPPFLAGS := -I$(SRC_DIR)/include -I$(SRC_DIR)/compat/cxx/include 
MESA_CPPFLAGS += -I$(SRC_DIR)/compat/libc/include -I$(SRC_DIR)/compat/posix/include
MESA_CPPFLAGS += $(SRC_DIR))/arch/$(ARCH)/include

$(CONFIGURE) :
	export EMBOX_GCC_LINK=full; \
	export ETNAVIV_CFLAGS="-I$(ROOT_DIR)/build/extbld/third_party/freedesktop/mesa/libdrm_imx6/libdrm-2.4.79 -I$(ROOT_DIR)/build/extbld/third_party/freedesktop/mesa/libdrm_imx6/libdrm-2.4.79/etnaviv -I$(ROOT_DIR)/build/extbld/third_party/freedesktop/mesa/libdrm_imx6/libdrm-2.4.79/include/drm"; \
	export ETNAVIV_LIBS="-L$(ROOT_DIR)/build/extbld/third_party/freedesktop/mesa/libdrm_imx6/libdrm-2.4.79/etnaviv -ldrm_etnaviv"; \
	export LIBDRM_CFLAGS="-I$(ROOT_DIR)/build/extbld/third_party/freedesktop/mesa/libdrm_imx6/libdrm-2.4.79 -I$(ROOT_DIR)/build/extbld/third_party/freedesktop/mesa/libdrm_imx6/libdrm-2.4.79/etnaviv -I$(ROOT_DIR)/build/extbld/third_party/freedesktop/mesa/libdrm_imx6/libdrm-2.4.79/include/drm"; \
	export LIBDRM_LIBS="-L$(ROOT_DIR)/build/extbld/third_party/freedesktop/mesa/libdrm_imx6/libdrm-2.4.79/ -ldrm"; \
	cd $(PKG_SOURCE_DIR) && aclocal && autoconf &&( \
		./configure --host=$(AUTOCONF_TARGET_TRIPLET) \
			--target=$(AUTOCONF_TARGET_TRIPLET) \
			--enable-static  --disable-shared \
			--with-gallium-drivers=etnaviv \
			--enable-osmesa \
			--disable-driglx-direct --disable-dri \
			--disable-egl \
			--disable-largefile --disable-valgrind \
			--disable-gallium-llvm --disable-gallium-tests \
			--disable-va \
			--disable-gbm \
			--disable-vdpau \
			--disable-glx --disable-glx-tls \
			--disable-asm \
			--disable-gles1 --disable-gles2 \
			CC=$(EMBOX_GCC) \
			CXX=$(EMBOX_GXX) \
			CPPFLAGS='$(MESA_CPPFLAGS)' \
	)
	touch $@

$(BUILD) :
	cd $(PKG_SOURCE_DIR) && ( \
		$(MAKE); \
	)
	touch $@

$(INSTALL) :
	cp $(PKG_SOURCE_DIR)/.libs/libdrm.a $(PKG_INSTALL_DIR)/libdrm.a
	cp $(PKG_SOURCE_DIR)/etnaviv/.libs/libdrm_etnaviv.a $(PKG_INSTALL_DIR)/libdrm_etnaviv.a
	touch $@
