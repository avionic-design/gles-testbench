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

struct checkerboard {
	struct pipeline_stage base;

	struct geometry *geometry;
	struct framebuffer *target;

	struct glsl_shader *vertex, *fragment;
	struct glsl_program *program;

	/* attribute locations */
	GLint pos, tex;

	/* uniform locations */
	GLint c1, c2, freq;
};

static const GLchar *checkerboard_vs[] = {
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

static const GLchar *checkerboard_fs[] = {
	"precision mediump float;\n",
	"uniform vec3 color1, color2;\n",
	"uniform float frequency;\n",
	"varying vec2 vtex;\n",
	"\n",
	"void main()\n",
	"{\n",
	"    vec2 position, pattern, threshold = vec2(0.5);\n",
	"    vec3 color;\n",
	"\n",
	"    position = vtex * frequency;\n",
	"    position = fract(position);\n",
	"\n",
	"    pattern = step(position, threshold);\n",
	"\n",
	"    if (pattern.y > 0.0)\n",
	"        color = mix(color1, color2, pattern.x);\n",
	"    else\n",
	"        color = mix(color2, color1, pattern.x);\n",
	"\n",
	"    gl_FragColor = vec4(color, 1.0);\n",
	"}"
};

static inline struct checkerboard *to_checkerboard(struct pipeline_stage *stage)
{
	return (struct checkerboard *)stage;
}

static void checkerboard_release(struct pipeline_stage *stage)
{
	struct checkerboard *board = to_checkerboard(stage);

	glsl_program_free(board->program);
	free(board);
}

static void checkerboard_render(struct pipeline_stage *stage)
{
	struct checkerboard *board = to_checkerboard(stage);
	static const GLfloat red[3] = { 1.0f, 0.0f, 0.0f };
	static const GLfloat blue[3] = { 0.0f, 0.0f, 1.0f };
	struct geometry *geometry = board->geometry;
	static const GLfloat frequency = 16.0f;

	glBindFramebuffer(GL_FRAMEBUFFER, board->target->id);
	glUseProgram(board->program->id);

	glVertexAttribPointer(board->pos, 3, GL_FLOAT, GL_FALSE,
			      3 * sizeof(GLfloat), geometry->vertices);
	glEnableVertexAttribArray(board->pos);

	glVertexAttribPointer(board->tex, 2, GL_FLOAT, GL_FALSE,
			      2 * sizeof(GLfloat), geometry->uv);
	glEnableVertexAttribArray(board->tex);

	glUniform3fv(board->c1, 1, red);
	glUniform3fv(board->c2, 1, blue);
	glUniform1f(board->freq, frequency);

	glDrawElements(GL_TRIANGLES, geometry->num_indices, GL_UNSIGNED_SHORT,
		       geometry->indices);
}

struct pipeline_stage *checkerboard_new(struct gles *gles,
					struct geometry *geometry,
					struct framebuffer *target)
{
	struct checkerboard *stage;

	stage = calloc(1, sizeof(*stage));
	if (!stage)
		return NULL;

	stage->base.name = "checkerboard pattern generator";
	stage->base.release = checkerboard_release;
	stage->base.render = checkerboard_render;

	stage->geometry = geometry;
	stage->target = target;

	stage->vertex = glsl_shader_new(GL_VERTEX_SHADER, checkerboard_vs,
					ARRAY_SIZE(checkerboard_vs));
	if (!stage->vertex) {
		fprintf(stderr, "failed to create vertex shader\n");
		return NULL;
	}

	stage->fragment = glsl_shader_new(GL_FRAGMENT_SHADER, checkerboard_fs,
					  ARRAY_SIZE(checkerboard_fs));
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
	stage->c1 = glGetUniformLocation(stage->program->id, "color1");
	stage->c2 = glGetUniformLocation(stage->program->id, "color2");
	stage->freq = glGetUniformLocation(stage->program->id, "frequency");

	return &stage->base;
}
