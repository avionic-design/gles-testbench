/*
 * Copyright (C) 2011 Julian Scheel <julian@jusst.de>
 * Copyright (C) 2011 Soeren Grunewald <soeren.grunewald@avionic-design.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _GST_GLES_SINK_H__
#define _GST_GLES_SINK_H__

#include <GLES2/gl2.h>
#include <EGL/egl.h>

#include <X11/Xlib.h>

#include "shader.h"

typedef struct _GstGLESSink GstGLESSink;
typedef struct _GstGLESWindow GstGLESWindow;
typedef struct _GstGLESContext GstGLESContext;
typedef struct _GstGLESThread GstGLESThread;

struct _GstGLESWindow {
	/* thread context */
	GThread *thread;
	volatile gboolean running;

	gint width;
	gint height;

	/* x11 context */
	Display *display;
	Window window;
	gboolean external_window;
};

struct _GstGLESContext {
	gboolean initialized;

	/* egl context */
	EGLDisplay display;
	EGLSurface surface;
	EGLContext context;

	/* shader programs */
	GstGLESShader deinterlace;
	GstGLESShader scale;

	/* textures for yuv input planes */
	GstGLESTexture y_tex;
	GstGLESTexture u_tex;
	GstGLESTexture v_tex;

	GstGLESTexture rgb_tex;

	/* framebuffer object */
	GLuint framebuffer;
};

struct _GstGLESThread {
	/* thread context */
	GThread *handle;
	volatile gboolean render_done;
	volatile gboolean running;

	GstGLESContext gles;
};

enum render_mode {
	GLES_BLANK,
	GLES_COPY,
	GLES_COLOR_CORRECT,
	GLES_ONE_SOURCE,
	GLES_DEINTERLACE
};

struct _GstGLESSink {
	GstGLESWindow x11;
	GstGLESThread gl_thread;

	gint par_n;
	gint par_d;

	gint video_width;
	gint video_height;

	/* properties */
	guint crop_top;
	guint crop_bottom;
	guint crop_left;
	guint crop_right;

	/* options for color correction shader */
	gfloat add[3];
	gfloat factor[3];

	gfloat keystone;

	gboolean silent;

	guint drop_first;
	guint dropped;

	enum render_mode mode;
	int depth;
};

int gst_gles_sink_init(GstGLESSink * sink, unsigned int depth);
void gst_gles_sink_preroll(GstGLESSink * sink, bool regenerate);
void gst_gles_sink_render(GstGLESSink * sink, bool regenerate);
void gst_gles_sink_finalize(GstGLESSink * sink);

#endif				/* _GST_GLES_SINK_H__ */
