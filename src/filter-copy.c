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

struct simple_copy {
	struct pipeline_stage base;

	struct geometry *geometry;
	struct framebuffer *source;
	struct framebuffer *target;

	struct glsl_shader *vertex, *fragment;
	struct glsl_program *program;

	/* attribute locations */
	GLint pos, tex;

	/* uniform locations */
	GLint input;
};

static const GLchar *simple_copy_vs[] = {
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

static const GLchar *simple_copy_fs[] = {
	"precision mediump float;\n",
	"uniform sampler2D source;\n",
	"varying vec2 vtex;\n",
	"\n",
	"void main()\n",
	"{\n",
	"    gl_FragColor = texture2D(source, vtex);\n",
	"}"
};

static inline struct simple_copy *to_simple_copy(struct pipeline_stage *stage)
{
	return (struct simple_copy *)stage;
}

static void simple_copy_release(struct pipeline_stage *stage)
{
	struct simple_copy *copy = to_simple_copy(stage);

	glsl_program_free(copy->program);
	free(copy);
}

static void simple_copy_render(struct pipeline_stage *stage)
{
	struct simple_copy *copy = to_simple_copy(stage);
	struct geometry *geometry = copy->geometry;

	glBindFramebuffer(GL_FRAMEBUFFER, copy->target->id);
	glUseProgram(copy->program->id);

	glVertexAttribPointer(copy->pos, 3, GL_FLOAT, GL_FALSE,
			      3 * sizeof(GLfloat), geometry->vertices);
	glEnableVertexAttribArray(copy->pos);

	glVertexAttribPointer(copy->tex, 2, GL_FLOAT, GL_FALSE,
			      2 * sizeof(GLfloat), geometry->uv);
	glEnableVertexAttribArray(copy->tex);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, copy->source->texture->id);
	glUniform1i(copy->input, 0);

	glDrawElements(GL_TRIANGLES, geometry->num_indices, GL_UNSIGNED_SHORT,
		       geometry->indices);
}

struct pipeline_stage *simple_copy_new(struct gles *gles,
				       struct geometry *geometry,
				       struct framebuffer *source,
				       struct framebuffer *target)
{
	struct simple_copy *stage;

	stage = calloc(1, sizeof(*stage));
	if (!stage)
		return NULL;

	stage->base.name = "simple texture copy operation";
	stage->base.release = simple_copy_release;
	stage->base.render = simple_copy_render;

	stage->geometry = geometry;
	stage->source = source;
	stage->target = target;

	stage->vertex = glsl_shader_new(GL_VERTEX_SHADER, simple_copy_vs,
					ARRAY_SIZE(simple_copy_vs));
	if (!stage->vertex) {
		fprintf(stderr, "failed to create vertex shader\n");
		return NULL;
	}

	stage->fragment = glsl_shader_new(GL_FRAGMENT_SHADER, simple_copy_fs,
					  ARRAY_SIZE(simple_copy_fs));
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
	stage->input = glGetUniformLocation(stage->program->id, "source");

	return &stage->base;
}
