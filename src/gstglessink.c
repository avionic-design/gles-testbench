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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <X11/Xatom.h>

#include "gstglessink.h"
#include "shader.h"

static gint setup_gl_context(GstGLESSink * sink);

/* OpenGL ES 2.0 implementation */
static GLuint gl_create_texture(GLuint tex_filter)
{
	GLuint tex_id = 0;

	glGenTextures(1, &tex_id);
	glBindTexture(GL_TEXTURE_2D, tex_id);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, tex_filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, tex_filter);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	return tex_id;
}

static void gl_gen_framebuffer(GstGLESSink * sink)
{
	GstGLESContext *gles = &sink->gles;

	glGenFramebuffers(1, &gles->framebuffer);

	gles->rgb_tex.id = gl_create_texture(GL_LINEAR);
	if (!gles->rgb_tex.id)
		g_error("Could not create RGB texture");

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, sink->width, sink->height, 0,
		     GL_RGB, GL_UNSIGNED_BYTE, NULL);

	glBindFramebuffer(GL_FRAMEBUFFER, gles->framebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			       GL_TEXTURE_2D, gles->rgb_tex.id, 0);
}

/* bind the uniforms used in the color correction filter */
static void gl_bind_uniform_cc(GstGLESSink * sink)
{
	GstGLESContext *gles = &sink->gles;
	GLint factor_loc;
	GLint add_loc;

	if (sink->mode == GLES_COLOR_CORRECT) {
		add_loc = glGetUniformLocation(gles->scale.program, "add");
		factor_loc = glGetUniformLocation(gles->scale.program,
						  "factor");

		g_debug("uniform locations: factor=%d, add=%d", factor_loc,
			add_loc);
		g_debug("factor values: %f, %f, %f", sink->factor[0],
			sink->factor[1], sink->factor[2]);
		glUniform3f(add_loc, sink->add[0], sink->add[1], sink->add[2]);
		glUniform3f(factor_loc, sink->factor[0], sink->factor[1],
			    sink->factor[2]);
	}
}

static void gl_draw_fbo(GstGLESSink * sink)
{
	GstGLESContext *gles = &sink->gles;
	GLushort indices[] = { 0, 1, 2, 0, 2, 3 };
	GLint line_height_loc;

	GLfloat vVertices[] = {
		-1.0f, -1.0f,
		0.0f, 1.0f,

		1.0f, -1.0f,
		1.0f, 1.0f,

		1.0f, 1.0f,
		1.0f, 0.0f,

		-1.0f, 1.0f,
		0.0f, 0.0f,
	};

	GLfloat texVertices[] = {
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,
	};

	glBindFramebuffer(GL_FRAMEBUFFER, gles->framebuffer);
	glUseProgram(gles->deinterlace.program);

	glViewport(0, 0, sink->width, sink->height);

	glClear(GL_COLOR_BUFFER_BIT);

	glVertexAttribPointer(gles->deinterlace.position_loc, 2, GL_FLOAT,
			      GL_FALSE, 4 * sizeof(GLfloat), vVertices);
	glVertexAttribPointer(gles->deinterlace.texcoord_loc, 2, GL_FLOAT,
			      GL_FALSE, 2 * sizeof(GLfloat), texVertices);

	glEnableVertexAttribArray(gles->deinterlace.position_loc);
	glEnableVertexAttribArray(gles->deinterlace.texcoord_loc);

// FIXME: bind fbo as texture
	line_height_loc = glGetUniformLocation(gles->deinterlace.program,
					       "line_height");
	glUniform1f(line_height_loc, 1.0 / sink->height);

	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
}

void gl_draw_onscreen(GstGLESSink * sink)
{
	GLfloat texVertices[] = {
		0.0f, 0.0f, 0.0f, 1.0f,
		1.0f, 0.0f, 0.0f, 1.0f,
		1.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
	};

	GLfloat posVertices[] = {
		-1.0f, -1.0f,
		1.0f, -1.0f,
		1.0f, 1.0f,
		-1.0f, 1.0f
	};

	GLushort indices[] = { 0, 1, 2, 0, 2, 3 };

	GstGLESContext *gles = &sink->gles;

	/* add cropping to texture coordinates */
	float crop_left = (float)sink->crop_left / sink->width;
	float crop_right = (float)sink->crop_right / sink->width;
	float crop_top = (float)sink->crop_top / sink->height;
	float crop_bottom = (float)sink->crop_bottom / sink->height;

	texVertices[0] += crop_left;
	texVertices[1] += crop_bottom;
	texVertices[2] -= crop_right;
	texVertices[3] += crop_bottom;
	texVertices[4] -= crop_right;
	texVertices[5] -= crop_top;
	texVertices[6] += crop_left;
	texVertices[7] -= crop_top;

	/* add keystone offsets */
	if (sink->keystone > 0) {
		float offset = sink->keystone / 2;
		posVertices[6] += offset;	/* top left */
		posVertices[4] -= offset;	/* top right */
	} else if (sink->keystone < 0) {
		//float offset = -1 * sink->keystone / 2;
		//posVertices[14] += offset;
		//posVertices[10] -= offset;
	}

	glUseProgram(gles->scale.program);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glViewport(0, 0, sink->width, sink->height);

	glClear(GL_COLOR_BUFFER_BIT);

	glVertexAttribPointer(gles->scale.position_loc, 2, GL_FLOAT,
			      GL_FALSE, 2 * sizeof(GLfloat), posVertices);

	glVertexAttribPointer(gles->scale.texcoord_loc, 4, GL_FLOAT,
			      GL_FALSE, 4 * sizeof(GLfloat), texVertices);

	glEnableVertexAttribArray(gles->scale.position_loc);
	glEnableVertexAttribArray(gles->scale.texcoord_loc);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gles->rgb_tex.id);
	glUniform1i(gles->rgb_tex.loc, 0);

	gl_bind_uniform_cc(sink);

	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
	eglSwapBuffers(gles->display, gles->surface);
}

void gl_clear_draw(GstGLESSink * sink)
{
	GstGLESContext *gles = &sink->gles;
	static int red = 0;
	static int blue = 1;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, sink->width, sink->height);

	if (red == 0.0) {
		red = 1.0;
		blue = 0.0;
	} else {
		red = 0.0;
		blue = 1.0;
	}
	glClearColor(red, 0.0, blue, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	eglSwapBuffers(gles->display, gles->surface);
}

/* EGL implementation */

static gint egl_init(GstGLESSink * sink)
{
	const EGLint configAttribs[] = {
		EGL_BUFFER_SIZE, sink->depth,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};

	const EGLint contextAttribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	EGLConfig config;
	EGLint num_configs;
	EGLint major;
	EGLint minor;

	GstGLESContext *gles = &sink->gles;

	g_debug("egl get display");
	gles->display = eglGetDisplay((EGLNativeDisplayType)
				      sink->x11.display);
	if (gles->display == EGL_NO_DISPLAY) {
		g_error("Could not get EGL display");
		return -1;
	}

	g_debug("egl initialize");
	if (!eglInitialize(gles->display, &major, &minor)) {
		g_error("Could not initialize EGL context");
		return -1;
	}
	g_debug("Have EGL version: %d.%d", major, minor);

	g_debug("choose config");
	if (!eglChooseConfig(gles->display, configAttribs, &config, 1,
			     &num_configs)) {
		g_error("Could not choose EGL config");
		return -1;
	}

	if (num_configs != 1) {
		g_warning("Did not get exactly one config, but %d",
			  num_configs);
	}

	g_debug("create window surface");
	gles->surface = eglCreateWindowSurface(gles->display, config,
					       sink->x11.window, NULL);
	if (gles->surface == EGL_NO_SURFACE) {
		g_error("Could not create EGL surface");
		return -1;
	}

	g_debug("egl create context");
	gles->context = eglCreateContext(gles->display, config,
					 EGL_NO_CONTEXT, contextAttribs);
	if (gles->context == EGL_NO_CONTEXT) {
		g_error("Could not create EGL context");
		return -1;
	}

	g_debug("egl make context current");
	if (!eglMakeCurrent(gles->display, gles->surface,
			    gles->surface, gles->context)) {
		g_error("Could not set EGL context to current");
		return -1;
	}

	g_debug("egl init done");

	return 0;
}

static void egl_close(GstGLESSink * sink)
{
	GstGLESContext *context = &sink->gles;

	const GLuint framebuffers[] = {
		context->framebuffer
	};

	const GLuint textures[] = {
		context->y_tex.id,
		context->u_tex.id,
		context->v_tex.id,
		context->rgb_tex.id
	};

	glDeleteFramebuffers(G_N_ELEMENTS(framebuffers), framebuffers);
	glDeleteTextures(G_N_ELEMENTS(textures), textures);
	gl_delete_shader(&context->scale);
	gl_delete_shader(&context->deinterlace);

	if (context->context) {
		eglDestroyContext(context->display, context->context);
		context->context = NULL;
	}

	if (context->surface) {
		eglDestroySurface(context->display, context->surface);
		context->surface = NULL;
	}

	if (context->display) {
		eglTerminate(context->display);
		context->display = NULL;
	}
}

static gint x11_init(GstGLESSink * sink)
{
	XSetWindowAttributes swa;
	Atom fullscreen;
	XWMHints hints;
	Atom wm_state;
	Window root;
	XEvent xev;
	int screen;

	sink->x11.display = XOpenDisplay(NULL);
	if (!sink->x11.display) {
		g_error("Could not create X display");
		return -1;
	}

	root = DefaultRootWindow(sink->x11.display);
	screen = DefaultScreen(sink->x11.display);

	sink->width = DisplayWidth(sink->x11.display, screen);
	sink->height = DisplayHeight(sink->x11.display, screen);
	g_print("\tResolution: %ux%u\n", sink->width, sink->height);

	XLockDisplay(sink->x11.display);

	memset(&swa, 0, sizeof(swa));
	swa.event_mask = StructureNotifyMask | ExposureMask |
			 VisibilityChangeMask;

	sink->x11.window = XCreateWindow(sink->x11.display, root, 0, 0,
					 sink->width, sink->height, 0,
					 CopyFromParent, InputOutput,
					 CopyFromParent, CWEventMask,
					 &swa);

	XSetWindowBackgroundPixmap(sink->x11.display, sink->x11.window, None);

	memset(&hints, 0, sizeof(hints));
	hints.input = True;
	hints.flags = InputHint;

	XSetWMHints(sink->x11.display, sink->x11.window, &hints);

	fullscreen = XInternAtom(sink->x11.display, "_NET_WM_STATE_FULLSCREEN",
				 False);
	wm_state = XInternAtom(sink->x11.display, "_NET_WM_STATE", False);

	memset(&xev, 0, sizeof(xev));
	xev.type = ClientMessage;
	xev.xclient.window = sink->x11.window;
	xev.xclient.message_type = wm_state;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = 1;
	xev.xclient.data.l[1] = fullscreen;
	xev.xclient.data.l[2] = 0;

	XMapWindow(sink->x11.display, sink->x11.window);

	XSendEvent(sink->x11.display, DefaultRootWindow(sink->x11.display),
		   False, SubstructureRedirectMask | SubstructureNotifyMask,
		   &xev);

	XFlush(sink->x11.display);
	XStoreName(sink->x11.display, sink->x11.window, "GLESSink");

	XUnlockDisplay(sink->x11.display);

	return 0;
}

static void x11_close(GstGLESSink * sink)
{
	if (sink->x11.display) {
		XLockDisplay(sink->x11.display);

		/* only destroy the window if we created it, windows
		   owned by the application stay untouched */
		if (!sink->x11.external_window) {
			XDestroyWindow(sink->x11.display, sink->x11.window);
			sink->x11.window = 0;
		} else
			XSelectInput(sink->x11.display, sink->x11.window, 0);

		XSync(sink->x11.display, FALSE);
		XUnlockDisplay(sink->x11.display);
		XCloseDisplay(sink->x11.display);
		sink->x11.display = NULL;
	}
}

static gint setup_gl_context(GstGLESSink * sink)
{
	GstGLESContext *gles = &sink->gles;
	int shader;
	gint ret;

	if (x11_init(sink) < 0) {
		g_error("X11 init failed, abort");
		return -ENOMEM;
	}

	if (egl_init(sink) < 0) {
		g_error("EGL init failed, abort");
		x11_close(sink);
		return -ENOMEM;
	}

	if (sink->mode != GLES_BLANK) {
		ret = gl_init_shader(&gles->deinterlace, SHADER_PATTERN);
		if (ret < 0) {
			g_error("Could not initialize shader: %d", ret);
			egl_close(sink);
			x11_close(sink);
			return -ENOMEM;
		}

		if (sink->mode == GLES_COPY)
			shader = SHADER_COPY;
		else if (sink->mode == GLES_DEINTERLACE)
			shader = SHADER_DEINT_LINEAR;
		else if (sink->mode == GLES_ONE_SOURCE)
			shader = SHADER_ONE_SOURCE;
		else
			shader = SHADER_COPY;

		ret = gl_init_shader(&gles->scale, shader);
		if (ret < 0) {
			g_error("Could not initialize shader: %d", ret);
			egl_close(sink);
			x11_close(sink);
			return -ENOMEM;
		}
		gles->rgb_tex.loc =
		    glGetUniformLocation(gles->scale.program, "s_tex");
	}

	return 0;
}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
int gst_gles_sink_init(GstGLESSink * sink, unsigned int depth)
{
	Status ret;
	int i;

	if (depth != 16 && depth != 24 && depth != 30)
		return -EINVAL;

	for (i = 0; i < 3; i++)
		sink->factor[i] = 1.0;

	sink->silent = FALSE;
	sink->depth = depth;
	sink->mode = GLES_BLANK;

	ret = XInitThreads();
	if (ret == 0) {
		g_error("XInitThreads failed");
		return -ENOSYS;
	}

	return 0;
}

/* GstElement vmethod implementations */

void gst_gles_sink_preroll(GstGLESSink * sink, bool regenerate)
{
	setup_gl_context(sink);
	gl_gen_framebuffer(sink);

	if (!regenerate)
		gl_draw_fbo(sink);
}

void gst_gles_sink_render(GstGLESSink * sink, bool regenerate)
{
	if (sink->mode == GLES_BLANK) {
		gl_clear_draw(sink);
	} else {
		if (regenerate)
			gl_draw_fbo(sink);

		gl_draw_onscreen(sink);
	}
}

void gst_gles_sink_finalize(GstGLESSink * sink)
{
	egl_close(sink);
	x11_close(sink);
}
