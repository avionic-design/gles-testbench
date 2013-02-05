/*
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

#ifndef GLES_TESTBENCH_PIPELINE_H
#define GLES_TESTBENCH_PIPELINE_H

#include <GLES2/gl2.h>

struct framebuffer;
struct pipeline;
struct geometry;
struct gles;

struct pipeline_stage {
	const char *name;

	void (*release)(struct pipeline_stage *stage);
	void (*render)(struct pipeline_stage *stage);

	struct pipeline_stage *next;
	struct pipeline_stage *prev;

	struct pipeline *pipeline;
};

void pipeline_stage_free(struct pipeline_stage *stage);

struct pipeline {
	struct pipeline_stage *first;
	struct pipeline_stage *last;

	struct gles *gles;
};

struct pipeline *pipeline_new(struct gles *gles);
void pipeline_free(struct pipeline *pipeline);
void pipeline_add_stage(struct pipeline *pipeline,
			struct pipeline_stage *stage);
void pipeline_render(struct pipeline *pipeline);

struct pipeline_stage *simple_fill_new(struct gles *gles,
				       struct geometry *geometry,
				       struct framebuffer *target,
				       GLfloat red, GLfloat green,
				       GLfloat blue);
struct pipeline_stage *checkerboard_new(struct gles *gles,
					struct geometry *geometry,
					struct framebuffer *target);
struct pipeline_stage *simple_copy_new(struct gles *gles,
				       struct geometry *geometry,
				       struct framebuffer *source,
				       struct framebuffer *target);
struct pipeline_stage *copy_one_new(struct gles *gles,
				    struct geometry *geometry,
				    struct framebuffer *source,
				    struct framebuffer *target);
struct pipeline_stage *deinterlace_new(struct gles *gles,
				       struct geometry *geometry,
				       struct framebuffer *source,
				       struct framebuffer *target);
struct pipeline_stage *color_correct_new(struct gles *gles,
					 struct geometry *geometry,
					 struct framebuffer *source,
					 struct framebuffer *target);

#endif
