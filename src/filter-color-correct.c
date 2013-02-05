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

struct color_correct {
	struct pipeline_stage base;

	struct framebuffer *source;
	struct framebuffer *target;

	struct glsl_shader *vertex, *fragment;
	struct glsl_program *program;
	struct geometry *geometry;

	/* attribute locations */
	GLint pos, tex;

	/* uniform locations */
	GLint input, add, factor;

	GLfloat vadd[3], vfactor[3];
};

static const GLchar *color_correct_vs[] = {
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

static const GLchar *color_correct_fs[] = {
	"precision mediump float;\n",
	"uniform sampler2D source;\n",
	"uniform vec3 factor;\n",
	"uniform vec3 add;\n",
	"varying vec2 vtex;\n",
	"\n",
	"void main()\n",
	"{\n",
	"    vec3 color = texture2D(source, vtex).rgb;\n",
	"    color = (color + add) * factor;\n",
	"    gl_FragColor = vec4(color, 1.0);\n",
	"}"
};

static inline struct color_correct *to_color_correct(struct pipeline_stage *stage)
{
	return (struct color_correct *)stage;
}

static void color_correct_release(struct pipeline_stage *stage)
{
	struct color_correct *cc = to_color_correct(stage);

	glsl_program_free(cc->program);
	geometry_free(cc->geometry);
	free(cc);
}

static void color_correct_render(struct pipeline_stage *stage)
{
	struct color_correct *cc = to_color_correct(stage);
	const GLfloat *vertices = cc->geometry->vertices;
	const GLfloat *uv = cc->geometry->uv;
	const GLushort *indices = cc->geometry->indices;
	const GLsizei num_indices = cc->geometry->num_indices;

	glBindFramebuffer(GL_FRAMEBUFFER, cc->target->id);
	glUseProgram(cc->program->id);

	glVertexAttribPointer(cc->pos, 3, GL_FLOAT, GL_FALSE,
			      3 * sizeof(GLfloat), vertices);
	glEnableVertexAttribArray(cc->pos);

	glVertexAttribPointer(cc->tex, 2, GL_FLOAT, GL_FALSE,
			      2 * sizeof(GLfloat), uv);
	glEnableVertexAttribArray(cc->tex);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, cc->source->texture->id);
	glUniform1i(cc->input, 0);

	glUniform3fv(cc->factor, 1, cc->vfactor);
	glUniform3fv(cc->add, 1, cc->vadd);

	glDrawElements(GL_TRIANGLES, num_indices, GL_UNSIGNED_SHORT, indices);
}

struct pipeline_stage *color_correct_new(struct gles *gles,
					 struct geometry *geometry,
					 struct framebuffer *source,
					 struct framebuffer *target)
{
	struct color_correct *stage;

	stage = calloc(1, sizeof(*stage));
	if (!stage)
		return NULL;

	stage->base.name = "color correction operation";
	stage->base.release = color_correct_release;
	stage->base.render = color_correct_render;

	stage->geometry = geometry;
	stage->source = source;
	stage->target = target;

	stage->vertex = glsl_shader_new(GL_VERTEX_SHADER, color_correct_vs,
					ARRAY_SIZE(color_correct_vs));
	if (!stage->vertex) {
		fprintf(stderr, "failed to create vertex shader\n");
		return NULL;
	}

	stage->fragment = glsl_shader_new(GL_FRAGMENT_SHADER, color_correct_fs,
					  ARRAY_SIZE(color_correct_fs));
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
	stage->factor = glGetUniformLocation(stage->program->id, "factor");
	stage->add = glGetUniformLocation(stage->program->id, "add");

	stage->vfactor[0] = 1.0f;
	stage->vfactor[1] = 1.0f;
	stage->vfactor[2] = 1.0f;

	stage->vadd[0] = 0.0f;
	stage->vadd[1] = 0.0f;
	stage->vadd[2] = 0.0f;

	return &stage->base;
}
