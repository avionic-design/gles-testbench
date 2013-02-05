/*
 * Copyright (C) 2011 Julian Scheel <julian@jusst.de>
 * Copyright (C) 2011 Soeren Grunewald <soeren.grunewald@avionic-design.de>
 * Copyright (C) 2013 Avionic Design GmbH
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <X11/Xatom.h>

#include "gles.h"

struct texture *texture_new(GLuint filter)
{
	struct texture *texture;

	texture = calloc(1, sizeof(*texture));
	if (!texture)
		return NULL;

	glGenTextures(1, &texture->id);
	glBindTexture(GL_TEXTURE_2D, texture->id);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	return texture;
}

void texture_free(struct texture *texture)
{
	glDeleteTextures(1, &texture->id);
	free(texture);
}

struct framebuffer *framebuffer_new(unsigned int width, unsigned int height)
{
	struct framebuffer *framebuffer;

	framebuffer = calloc(1, sizeof(*framebuffer));
	if (!framebuffer)
		return NULL;

	glGenFramebuffers(1, &framebuffer->id);
	framebuffer->width = width;
	framebuffer->height = height;

	framebuffer->texture = texture_new(GL_LINEAR);
	if (!framebuffer->texture) {
		free(framebuffer);
		return NULL;
	}

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
		     GL_UNSIGNED_BYTE, NULL);

	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->id);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			       GL_TEXTURE_2D, framebuffer->texture->id, 0);

	return framebuffer;
}

void framebuffer_free(struct framebuffer *framebuffer)
{
	texture_free(framebuffer->texture);
	glDeleteFramebuffers(1, &framebuffer->id);
	free(framebuffer);
}

struct framebuffer *display_framebuffer_new(unsigned int width,
					    unsigned int height)
{
	struct framebuffer *display;

	display = calloc(1, sizeof(*display));
	if (!display)
		return NULL;

	display->id = 0;
	display->width = width;
	display->height = height;
	display->texture = NULL;

	return display;
}

void display_framebuffer_free(struct framebuffer *framebuffer)
{
	free(framebuffer);
}

/* EGL implementation */

static int gles_egl_init(struct gles *gles)
{
	const EGLint config_attribs[] = {
		EGL_BUFFER_SIZE, gles->depth,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};
	const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};
	EGLNativeDisplayType display = gles->x.display;
	EGLint num_configs, major, minor;
	EGLConfig config;

	gles->egl.display = eglGetDisplay(display);
	if (gles->egl.display == EGL_NO_DISPLAY) {
		fprintf(stderr, "Could not get EGL display\n");
		return -1;
	}

	if (!eglInitialize(gles->egl.display, &major, &minor)) {
		fprintf(stderr, "Could not initialize EGL context\n");
		return -1;
	}

	printf("EGL: %d.%d\n", major, minor);

	if (!eglChooseConfig(gles->egl.display, config_attribs, &config, 1,
			     &num_configs)) {
		fprintf(stderr, "Could not choose EGL config\n");
		return -1;
	}

	if (num_configs != 1)
		printf("Found %d configurations\n", num_configs);

	gles->egl.surface = eglCreateWindowSurface(gles->egl.display, config,
						   gles->x.window, NULL);
	if (gles->egl.surface == EGL_NO_SURFACE) {
		fprintf(stderr, "Could not create EGL surface\n");
		return -1;
	}

	gles->egl.context = eglCreateContext(gles->egl.display, config,
					     EGL_NO_CONTEXT, context_attribs);
	if (gles->egl.context == EGL_NO_CONTEXT) {
		fprintf(stderr, "Could not create EGL context\n");
		return -1;
	}

	if (!eglMakeCurrent(gles->egl.display, gles->egl.surface,
			    gles->egl.surface, gles->egl.context)) {
		fprintf(stderr, "Could not set EGL context to current\n");
		return -1;
	}

	return 0;
}

static void gles_egl_close(struct gles *gles)
{
#if 0
	const GLuint framebuffers[] = {
		gles->framebuffer->id
	};
	const GLuint textures[] = {
		gles->framebuffer->texture->id
	};

	glDeleteFramebuffers(ARRAY_SIZE(framebuffers), framebuffers);
	glDeleteTextures(ARRAY_SIZE(textures), textures);
#endif

	eglDestroyContext(gles->egl.display, gles->egl.context);
	eglDestroySurface(gles->egl.display, gles->egl.surface);
	eglTerminate(gles->egl.display);
}

static int gles_x_init(struct gles *gles)
{
	XSetWindowAttributes swa;
	Atom fullscreen;
	XWMHints hints;
	Atom wm_state;
	Window root;
	XEvent xev;
	int screen;

	gles->x.display = XOpenDisplay(NULL);
	if (!gles->x.display) {
		fprintf(stderr, "Could not open X display\n");
		return -1;
	}

	root = DefaultRootWindow(gles->x.display);
	screen = DefaultScreen(gles->x.display);

	gles->width = DisplayWidth(gles->x.display, screen);
	gles->height = DisplayHeight(gles->x.display, screen);
	printf("Resolution: %ux%u\n", gles->width, gles->height);

	memset(&swa, 0, sizeof(swa));
	swa.event_mask = StructureNotifyMask | ExposureMask |
			 VisibilityChangeMask;

	gles->x.window = XCreateWindow(gles->x.display, root, 0, 0,
				       gles->width, gles->height, 0,
				       CopyFromParent, InputOutput,
				       CopyFromParent, CWEventMask,
				       &swa);

	XSetWindowBackgroundPixmap(gles->x.display, gles->x.window, None);

	memset(&hints, 0, sizeof(hints));
	hints.input = True;
	hints.flags = InputHint;

	XSetWMHints(gles->x.display, gles->x.window, &hints);

	fullscreen = XInternAtom(gles->x.display, "_NET_WM_STATE_FULLSCREEN",
				 False);
	wm_state = XInternAtom(gles->x.display, "_NET_WM_STATE", False);

	memset(&xev, 0, sizeof(xev));
	xev.type = ClientMessage;
	xev.xclient.window = gles->x.window;
	xev.xclient.message_type = wm_state;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = 1;
	xev.xclient.data.l[1] = fullscreen;
	xev.xclient.data.l[2] = 0;

	XMapWindow(gles->x.display, gles->x.window);

	XSendEvent(gles->x.display, DefaultRootWindow(gles->x.display),
		   False, SubstructureRedirectMask | SubstructureNotifyMask,
		   &xev);

	XFlush(gles->x.display);
	XStoreName(gles->x.display, gles->x.window, "GLES testbench");

	return 0;
}

static void gles_x_close(struct gles *gles)
{
	XDestroyWindow(gles->x.display, gles->x.window);
	XCloseDisplay(gles->x.display);
}

struct gles *gles_new(unsigned int depth, bool regenerate)
{
	struct gles *gles;
	int i;

	if (depth != 16 && depth != 24 && depth != 30)
		return NULL;

	gles = calloc(1, sizeof(*gles));
	if (!gles)
		return NULL;

	for (i = 0; i < 3; i++)
		gles->factor[i] = 1.0;

	gles->depth = depth;

	if (gles_x_init(gles) < 0) {
		fprintf(stderr, "X11 init failed, abort\n");
		free(gles);
		return NULL;
	}

	if (gles_egl_init(gles) < 0) {
		fprintf(stderr, "EGL init failed, abort\n");
		gles_x_close(gles);
		free(gles);
		return NULL;
	}

	return gles;
}

void gles_free(struct gles *gles)
{
	gles_egl_close(gles);
	gles_x_close(gles);
}
