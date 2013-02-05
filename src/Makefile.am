bin_PROGRAMS = gles-standalone

gles_standalone_CFLAGS = \
	$(GLESV2_CFLAGS) \
	$(EGL_CFLAGS) \
	$(X11_CFLAGS)

gles_standalone_SOURCES = \
	filter-color-correct.c \
	filter-copy.c \
	filter-copy-one.c \
	filter-deinterlace.c \
	generator-checkerboard.c \
	generator-clear.c \
	generator-fill.c \
	geometry.c \
	geometry.h \
	gles-standalone.c \
	gles.c \
	gles.h \
	glsl.c \
	pipeline.c \
	pipeline.h

gles_standalone_LDADD = \
	$(GLESV2_LIBS) \
	$(EGL_LIBS) \
	$(X11_LIBS)
