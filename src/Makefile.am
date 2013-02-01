bin_PROGRAMS = gles-standalone

gles_standalone_CFLAGS = \
	$(GLESV2_CFLAGS) \
	$(EGL_CFLAGS) \
	$(X11_CFLAGS)

gles_standalone_SOURCES = \
	gles-standalone.c \
	gles.c \
	gles.h \
	glsl.c

gles_standalone_LDADD = \
	$(GLESV2_LIBS) \
	$(EGL_LIBS) \
	$(X11_LIBS)
