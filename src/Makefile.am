bin_PROGRAMS = gles-standalone

gles_standalone_SOURCES = gles-standalone.c gstglessink.c shader.c

gles_standalone_CFLAGS = $(GLIB_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GLESV2_CFLAGS) \
	$(EGL_CFLAGS) \
	$(X11_CFLAGS)
gles_standalone_LDADD = $(GLIB_LIBS) \
	$(GIO_LIBS) \
	$(GLESV2_LIBS) \
	$(EGL_LIBS) \
	$(X11_LIBS)
