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

#include <stdio.h>
#include <stdlib.h>

#include "pipeline.h"
#include "gles.h"

struct clear {
	struct pipeline_stage base;
	struct framebuffer *target;
	GLfloat red, green, blue;
	bool black;
};

static inline struct clear *to_clear(struct pipeline_stage *stage)
{
	return (struct clear *)stage;
}

static void clear_release(struct pipeline_stage *stage)
{
	struct clear *clear = to_clear(stage);

	free(clear);
}

static void clear_render(struct pipeline_stage *stage)
{
	struct clear *clear = to_clear(stage);

	glBindFramebuffer(GL_FRAMEBUFFER, clear->target->id);
	glViewport(0, 0, clear->target->width, clear->target->height);

	if (clear->black)
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	else
		glClearColor(clear->red, clear->green, clear->blue, 1.0f);

	glClear(GL_COLOR_BUFFER_BIT);
	clear->black = !clear->black;
}

struct pipeline_stage *clear_new(struct gles *gles, struct framebuffer *target,
				 GLfloat red, GLfloat green, GLfloat blue)
{
	struct clear *stage;

	stage = calloc(1, sizeof(*stage));
	if (!stage)
		return NULL;

	stage->base.name = "clear generator";
	stage->base.release = clear_release;
	stage->base.render = clear_render;

	stage->target = target;
	stage->red = red;
	stage->green = green;
	stage->blue = blue;
	stage->black = true;

	return &stage->base;
}
