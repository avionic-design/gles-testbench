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

#ifndef GLES_TESTBENCH_GLES_H
#define GLES_TESTBENCH_GLES_H

#include <stdbool.h>

#include <GLES2/gl2.h>
#include <EGL/egl.h>

#include <X11/Xlib.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

enum glsl_program_type {
	GLSL_PROGRAM_DEINT_LINEAR,
	GLSL_PROGRAM_COPY,
	GLSL_PROGRAM_COLOR_CORRECT,
	GLSL_PROGRAM_PATTERN,
	GLSL_PROGRAM_ONE_SOURCE
};

struct glsl_shader {
	GLuint id;
};

struct glsl_shader *glsl_shader_new(GLenum type, const GLchar *lines[],
				    GLint count);
void glsl_shader_free(struct glsl_shader *shader);

struct glsl_program {
	GLuint id;

	struct glsl_shader *vs;
	struct glsl_shader *fs;

	/* standard locations, used in most shaders */
	GLint position_loc;
	GLint texcoord_loc;
};

struct glsl_program *glsl_program_new(struct glsl_shader *vertex,
				      struct glsl_shader *fragment);
int glsl_program_link(struct glsl_program *program);
void glsl_program_free(struct glsl_program *program);

struct texture {
	GLuint id;
	GLint loc;
};

struct texture *texture_new(GLuint filter);
void texture_free(struct texture *texture);

struct framebuffer {
	GLuint id;
	GLuint width;
	GLuint height;
	struct texture *texture;
};

struct framebuffer *framebuffer_new(unsigned int width, unsigned int height);
void framebuffer_free(struct framebuffer *framebuffer);

struct framebuffer *display_framebuffer_new(unsigned int width,
					    unsigned int height);
void display_framebuffer_free(struct framebuffer *framebuffer);

struct gles {
	unsigned int width;
	unsigned int height;
	unsigned int depth;

	struct {
		Display *display;
		Window window;
	} x;

	/* egl context */
	struct {
		EGLDisplay display;
		EGLSurface surface;
		EGLContext context;
	} egl;

	/* properties */
	struct {
		unsigned int top;
		unsigned int bottom;
		unsigned int left;
		unsigned int right;
	} crop;

	/* options for color correction shader */
	float add[3];
	float factor[3];

	float keystone;
};

struct gles *gles_new(unsigned int depth, bool regenerate);
void gles_free(struct gles *gles);

#endif
