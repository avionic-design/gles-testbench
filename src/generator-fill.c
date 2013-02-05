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
#include "geometry.h"
#include "gles.h"

struct simple_fill {
	struct pipeline_stage base;

	struct geometry *geometry;
	struct framebuffer *target;

	struct glsl_shader *vertex, *fragment;
	struct glsl_program *program;

	GLfloat red, green, blue;

	/* attribute locations */
	GLint pos, tex;

	/* uniform locations */
	GLint color;
};

static const GLchar *simple_fill_vs[] = {
	"attribute vec3 position;\n",
	"attribute vec2 tex;\n",
	"varying vec2 vtex;\n",
	"\n",
	"void main()\n",
	"{\n",
	"   gl_Position = vec4(position, 1.0);\n",
	"   vtex = tex;\n",
	"}"
};

static const GLchar *simple_fill_fs[] = {
	"precision mediump float;\n",
	"uniform vec3 color;\n",
	"varying vec2 vtex;\n",
	"\n",
	"void main()\n",
	"{\n",
	"    gl_FragColor = vec4(color, 1.0);\n",
	"}"
};

static inline struct simple_fill *to_simple_fill(struct pipeline_stage *stage)
{
	return (struct simple_fill *)stage;
}

static void simple_fill_release(struct pipeline_stage *stage)
{
	struct simple_fill *fill = to_simple_fill(stage);

	glsl_program_free(fill->program);
	free(fill);
}

static void simple_fill_render(struct pipeline_stage *stage)
{
	struct simple_fill *fill = to_simple_fill(stage);
	struct geometry *geometry = fill->geometry;

	glBindFramebuffer(GL_FRAMEBUFFER, fill->target->id);
	glUseProgram(fill->program->id);

	glVertexAttribPointer(fill->pos, 3, GL_FLOAT, GL_FALSE,
			      3 * sizeof(GLfloat), geometry->vertices);
	glEnableVertexAttribArray(fill->pos);

	glVertexAttribPointer(fill->tex, 2, GL_FLOAT, GL_FALSE,
			      2 * sizeof(GLfloat), geometry->uv);
	glEnableVertexAttribArray(fill->tex);

	glUniform3f(fill->color, fill->red, fill->green, fill->blue);

	glDrawElements(GL_TRIANGLES, geometry->num_indices, GL_UNSIGNED_SHORT,
		       geometry->indices);
}

struct pipeline_stage *simple_fill_new(struct gles *gles,
				       struct geometry *geometry,
				       struct framebuffer *target,
				       GLfloat red, GLfloat green,
				       GLfloat blue)
{
	struct simple_fill *stage;

	stage = calloc(1, sizeof(*stage));
	if (!stage)
		return NULL;

	stage->base.name = "simple fill pattern generator";
	stage->base.release = simple_fill_release;
	stage->base.render = simple_fill_render;

	stage->geometry = geometry;
	stage->target = target;
	stage->red = red;
	stage->green = green;
	stage->blue = blue;

	stage->vertex = glsl_shader_new(GL_VERTEX_SHADER, simple_fill_vs,
					ARRAY_SIZE(simple_fill_vs));
	if (!stage->vertex) {
		fprintf(stderr, "failed to create vertex shader\n");
		return NULL;
	}

	stage->fragment = glsl_shader_new(GL_FRAGMENT_SHADER, simple_fill_fs,
					  ARRAY_SIZE(simple_fill_fs));
	if (!stage->fragment) {
		fprintf(stderr, "failed to create fragment shader\n");
		return NULL;
	}

	stage->program = glsl_program_new(stage->vertex, stage->fragment);
	if (!stage->program) {
		fprintf(stderr, "failed to create GLSL program\n");
		return NULL;
	}

	if (glsl_program_link(stage->program) < 0) {
		fprintf(stderr, "failed to link GLSL program\n");
		return NULL;
	}

	stage->pos = glGetAttribLocation(stage->program->id, "position");
	stage->tex = glGetAttribLocation(stage->program->id, "tex");
	stage->color = glGetUniformLocation(stage->program->id, "color");

	return &stage->base;
}
