#
# Makefile for the drm device driver.  This driver provides support for the
# Direct Rendering Infrvboxructure (DRI) in XFree86 4.1.0 and higher.

ccflags-y := -Iinclude/drm

vbox-y := vbox_drv.o vbox_main.o vbox_mode.o vbox_fb.o vbox_ttm.o vbox_post.o

obj-$(CONFIG_DRM_AST) := vbox.o