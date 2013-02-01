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
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "gles.h"

#define FRAME_COUNT 600

static unsigned int subdivisions = 0;
static bool transform = false;
struct pipeline;

struct pipeline_stage {
	const char *name;

	void (*release)(struct pipeline_stage *stage);
	void (*render)(struct pipeline_stage *stage);

	struct pipeline_stage *next;
	struct pipeline_stage *prev;

	struct pipeline *pipeline;
};

static void pipeline_stage_free(struct pipeline_stage *stage)
{
	if (stage && stage->release)
		stage->release(stage);
}

struct pipeline {
	struct pipeline_stage *first;
	struct pipeline_stage *last;

	struct gles *gles;
};

struct pipeline *pipeline_new(struct gles *gles)
{
	struct pipeline *pipeline;

	pipeline = calloc(1, sizeof(*pipeline));
	if (!pipeline)
		return NULL;

	pipeline->gles = gles;

	return pipeline;
}

void pipeline_free(struct pipeline *pipeline)
{
	struct pipeline_stage *stage = pipeline->first;

	while (stage) {
		struct pipeline_stage *next = stage->next;
		pipeline_stage_free(stage);
		stage = next;
	}

	free(pipeline);
}

void pipeline_add_stage(struct pipeline *pipeline, struct pipeline_stage *stage)
{
	if (pipeline->first == NULL && pipeline->last == NULL) {
		pipeline->first = stage;
		pipeline->last = stage;
	} else {
		pipeline->last->next = stage;
		pipeline->last = stage;
	}

	stage->pipeline = pipeline;
	stage->next = NULL;
}

void pipeline_render(struct pipeline *pipeline)
{
	struct gles *gles = pipeline->gles;
	struct pipeline_stage *stage;

	glViewport(0, 0, gles->width, gles->height);

	for (stage = pipeline->first; stage; stage = stage->next)
		stage->render(stage);

	eglSwapBuffers(gles->egl.display, gles->egl.surface);
}

struct simple_fill {
	struct pipeline_stage base;

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
	"attribute vec2 position;\n",
	"attribute vec2 tex;\n",
	"varying vec2 vtex;\n",
	"\n",
	"void main()\n",
	"{\n",
	"   gl_Position = vec4(position, 0.0, 1.0);\n",
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
	static const GLfloat vertices[] = {
		-1.0f, -1.0f,
		 1.0f, -1.0f,
		 1.0f,  1.0f,
		-1.0f,  1.0f,
	};
	static const GLfloat uv[] = {
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,
	};
	static const GLushort indices[] = {
		0, 1, 2,
		0, 2, 3
	};

	glBindFramebuffer(GL_FRAMEBUFFER, fill->target->id);
	glUseProgram(fill->program->id);

	glVertexAttribPointer(fill->pos, 2, GL_FLOAT, GL_FALSE,
			      2 * sizeof(GLfloat), vertices);
	glEnableVertexAttribArray(fill->pos);

	glVertexAttribPointer(fill->tex, 2, GL_FLOAT, GL_FALSE,
			      2 * sizeof(GLfloat), uv);
	glEnableVertexAttribArray(fill->tex);

	glUniform3f(fill->color, fill->red, fill->green, fill->blue);

	glDrawElements(GL_TRIANGLES, ARRAY_SIZE(indices), GL_UNSIGNED_SHORT,
		       indices);
}

struct pipeline_stage *simple_fill_new(struct gles *gles,
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

struct checkerboard {
	struct pipeline_stage base;

	struct framebuffer *target;

	struct glsl_shader *vertex, *fragment;
	struct glsl_program *program;

	/* attribute locations */
	GLint pos, tex;

	/* uniform locations */
	GLint c1, c2, freq;
};

static const GLchar *checkerboard_vs[] = {
	"attribute vec2 position;\n",
	"attribute vec2 tex;\n",
	"varying vec2 vtex;\n",
	"\n",
	"void main()\n",
	"{\n",
	"   gl_Position = vec4(position, 0.0, 1.0);\n",
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
	static const GLfloat vertices[] = {
		-1.0f, -1.0f,
		 1.0f, -1.0f,
		 1.0f,  1.0f,
		-1.0f,  1.0f,
	};
	static const GLfloat uv[] = {
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,
	};
	static const GLushort indices[] = {
		0, 1, 2,
		0, 2, 3
	};
	static const GLfloat frequency = 16.0f;

	glBindFramebuffer(GL_FRAMEBUFFER, board->target->id);
	glUseProgram(board->program->id);

	glVertexAttribPointer(board->pos, 2, GL_FLOAT, GL_FALSE,
			      2 * sizeof(GLfloat), vertices);
	glEnableVertexAttribArray(board->pos);

	glVertexAttribPointer(board->tex, 2, GL_FLOAT, GL_FALSE,
			      2 * sizeof(GLfloat), uv);
	glEnableVertexAttribArray(board->tex);

	glUniform3fv(board->c1, 1, red);
	glUniform3fv(board->c2, 1, blue);
	glUniform1f(board->freq, frequency);

	glDrawElements(GL_TRIANGLES, ARRAY_SIZE(indices), GL_UNSIGNED_SHORT,
		       indices);
}

struct pipeline_stage *checkerboard_new(struct gles *gles,
					struct framebuffer *target)
{
	struct checkerboard *stage;

	stage = calloc(1, sizeof(*stage));
	if (!stage)
		return NULL;

	stage->base.name = "checkerboard pattern generator";
	stage->base.release = checkerboard_release;
	stage->base.render = checkerboard_render;

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

struct simple_copy {
	struct pipeline_stage base;

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
	"attribute vec2 position;\n",
	"attribute vec2 tex;\n",
	"varying vec2 vtex;\n",
	"\n",
	"void main()\n",
	"{\n",
	"   gl_Position = vec4(position, 0.0, 1.0);\n",
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
	static const GLfloat vertices[] = {
		-1.0f, -1.0f,
		 1.0f, -1.0f,
		 1.0f,  1.0f,
		-1.0f,  1.0f,
	};
	static const GLfloat uv[] = {
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,
	};
	static const GLushort indices[] = {
		0, 1, 2,
		0, 2, 3
	};

	glBindFramebuffer(GL_FRAMEBUFFER, copy->target->id);
	glUseProgram(copy->program->id);

	glVertexAttribPointer(copy->pos, 2, GL_FLOAT, GL_FALSE,
			      2 * sizeof(GLfloat), vertices);
	glEnableVertexAttribArray(copy->pos);

	glVertexAttribPointer(copy->tex, 2, GL_FLOAT, GL_FALSE,
			      2 * sizeof(GLfloat), uv);
	glEnableVertexAttribArray(copy->tex);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, copy->source->texture->id);
	glUniform1i(copy->input, 0);

	glDrawElements(GL_TRIANGLES, ARRAY_SIZE(indices), GL_UNSIGNED_SHORT,
		       indices);
}

struct pipeline_stage *simple_copy_new(struct gles *gles,
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

struct deinterlace {
	struct pipeline_stage base;

	struct framebuffer *source;
	struct framebuffer *target;

	struct glsl_shader *vertex, *fragment;
	struct glsl_program *program;

	/* attribute locations */
	GLint pos, tex;

	/* uniform locations */
	GLint input, offset;
};

static const GLchar *deinterlace_vs[] = {
	"attribute vec2 position;\n",
	"attribute vec2 tex;\n",
	"varying vec2 vtex;\n",
	"\n",
	"void main()\n",
	"{\n",
	"   gl_Position = vec4(position, 0.0, 1.0);\n",
	"   vtex = tex;\n",
	"}"
};

static const GLchar *deinterlace_fs[] = {
	"precision mediump float;\n",
	"uniform sampler2D source;\n",
	"uniform float offset;\n"
	"varying vec2 vtex;\n",
	"\n",
	"void main()\n",
	"{\n",
	"    vec4 factor = vec4(0.3, 0.3, 0.3, 1.0);\n"
	"    vec2 above, below;\n",
	"    vec4 sum;\n",
	"\n",
	"    above.x = vtex.x;\n",
	"    above.y = vtex.y - offset;\n",
	"    below.x = vtex.x;\n",
	"    below.y = vtex.y + offset;\n",
	"\n",
	"    sum = texture2D(source, above) + texture2D(source, vtex) +\n",
	"          texture2D(source, below);\n",
	"\n",
	"    gl_FragColor = sum * factor;\n",
	"}"
};

static inline struct deinterlace *to_deinterlace(struct pipeline_stage *stage)
{
	return (struct deinterlace *)stage;
}

static void deinterlace_release(struct pipeline_stage *stage)
{
	struct deinterlace *deinterlace = to_deinterlace(stage);

	glsl_program_free(deinterlace->program);
	free(deinterlace);
}

static void deinterlace_render(struct pipeline_stage *stage)
{
	struct deinterlace *deinterlace = to_deinterlace(stage);
	struct gles *gles = stage->pipeline->gles;
	static const GLfloat vertices[] = {
		-1.0f, -1.0f,
		 1.0f, -1.0f,
		 1.0f,  1.0f,
		-1.0f,  1.0f,
	};
	static const GLfloat uv[] = {
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,
	};
	static const GLushort indices[] = {
		0, 1, 2,
		0, 2, 3
	};

	glBindFramebuffer(GL_FRAMEBUFFER, deinterlace->target->id);
	glUseProgram(deinterlace->program->id);

	glVertexAttribPointer(deinterlace->pos, 2, GL_FLOAT, GL_FALSE,
			      2 * sizeof(GLfloat), vertices);
	glEnableVertexAttribArray(deinterlace->pos);

	glVertexAttribPointer(deinterlace->tex, 2, GL_FLOAT, GL_FALSE,
			      2 * sizeof(GLfloat), uv);
	glEnableVertexAttribArray(deinterlace->tex);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, deinterlace->source->texture->id);
	glUniform1i(deinterlace->input, 0);

	glUniform1f(deinterlace->offset, 1.0f / gles->width);

	glDrawElements(GL_TRIANGLES, ARRAY_SIZE(indices), GL_UNSIGNED_SHORT,
		       indices);
}

struct pipeline_stage *deinterlace_new(struct gles *gles,
				       struct framebuffer *source,
				       struct framebuffer *target)
{
	struct deinterlace *stage;

	stage = calloc(1, sizeof(*stage));
	if (!stage)
		return NULL;

	stage->base.name = "linear deinterlace operation";
	stage->base.release = deinterlace_release;
	stage->base.render = deinterlace_render;

	stage->source = source;
	stage->target = target;

	stage->vertex = glsl_shader_new(GL_VERTEX_SHADER, deinterlace_vs,
					ARRAY_SIZE(deinterlace_vs));
	if (!stage->vertex) {
		fprintf(stderr, "failed to create vertex shader\n");
		return NULL;
	}

	stage->fragment = glsl_shader_new(GL_FRAGMENT_SHADER, deinterlace_fs,
					  ARRAY_SIZE(deinterlace_fs));
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
	stage->offset = glGetUniformLocation(stage->program->id, "offset");

	return &stage->base;
}

struct geometry {
	unsigned int num_vertices;
	GLfloat *vertices;
	GLfloat *uv;

	unsigned int num_indices;
	GLushort *indices;
};

static struct geometry *grid_new(unsigned int subdivisions)
{
	unsigned int num_rows = 1, num_cols, num_quads = 1, num_triangles;
	struct geometry *grid;
	unsigned int i, j;

	for (i = 0; i < subdivisions; i++)
		num_rows *= 2;

	num_cols = num_rows;

	num_quads = num_rows * num_cols;
	num_triangles = num_quads * 2;

	grid = calloc(1, sizeof(*grid));
	if (!grid)
		return NULL;

	grid->num_vertices = (num_rows + 1) * (num_cols + 1);

	grid->vertices = calloc(grid->num_vertices * 3, sizeof(GLfloat));
	if (!grid->vertices) {
		free(grid);
		return NULL;
	}

	grid->uv = calloc(grid->num_vertices * 2, sizeof(GLfloat));
	if (!grid->uv) {
		free(grid->vertices);
		free(grid);
		return NULL;
	}

	for (j = 0; j <= num_rows; j++) {
		GLfloat *v = grid->vertices + j * (num_cols + 1) * 3;
		GLfloat *t = grid->uv + j * (num_cols + 1) * 2;

		for (i = 0; i <= num_cols; i++) {
			float x, y;

			if (transform) {
				x = -1.0f + (2.0f * i / num_cols);
				y = -1.0f + (2.0f * j / num_rows);

				if ((i > 0) && (i < num_cols))
					x += rand() / (2.0f * num_cols) / RAND_MAX;

				if ((j > 0) && (j < num_rows))
					y += rand() / (2.0f * num_cols) / RAND_MAX;
			} else {
				x = -1.0f + (2.0f * i / num_cols);
				y = -1.0f + (2.0f * j / num_rows);
			}

			v[(i * 3) + 0] = x;
			v[(i * 3) + 1] = y;
			v[(i * 3) + 2] = 0;

			t[(i * 2) + 0] = 0.0f + (1.0f * i / num_cols);
			t[(i * 2) + 1] = 0.0f + (1.0f * j / num_rows);
		}
	}

	grid->num_indices = num_triangles * 3;

	grid->indices = calloc(grid->num_indices, sizeof(GLshort));
	if (!grid->indices) {
		free(grid->uv);
		free(grid->vertices);
		free(grid);
		return NULL;
	}

	for (j = 0; j < num_rows; j++) {
		unsigned int sx = (j + 0) * (num_cols + 1);
		unsigned int ex = (j + 1) * (num_cols + 1);

		for (i = 0; i < num_cols; i++) {
			unsigned int quad = (j * num_cols) + i;
			GLushort *v = grid->indices + quad * 6;

			v[0] = sx + i + 0;
			v[1] = sx + i + 1;
			v[2] = ex + i + 0;

			v[3] = sx + i + 1;
			v[4] = ex + i + 1;
			v[5] = ex + i + 0;
		}
	}

	return grid;
}

static void geometry_free(struct geometry *geometry)
{
	if (geometry) {
		free(geometry->vertices);
		free(geometry->indices);
		free(geometry->uv);
	}

	free(geometry);
}

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
	"attribute vec2 position;\n",
	"attribute vec2 tex;\n",
	"varying vec2 vtex;\n",
	"\n",
	"void main()\n",
	"{\n",
	"   gl_Position = vec4(position, 0.0, 1.0);\n",
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

	stage->geometry = grid_new(subdivisions);
	if (!stage->geometry) {
		fprintf(stderr, "failed to create geometry\n");
		return NULL;
	}

	stage->vfactor[0] = 1.0f;
	stage->vfactor[1] = 1.0f;
	stage->vfactor[2] = 1.0f;

	stage->vadd[0] = 0.0f;
	stage->vadd[1] = 0.0f;
	stage->vadd[2] = 0.0f;

	return &stage->base;
}

static struct pipeline *create_pipeline(struct gles *gles, int argc,
					char *argv[], bool regenerate)
{
	struct framebuffer *source = NULL, *target = NULL;
	struct pipeline *pipeline;
	int i;

	pipeline = pipeline_new(gles);
	if (!pipeline)
		return NULL;

	for (i = 0; i < argc; i++) {
		struct pipeline_stage *stage = NULL;

		/*
		 * FIXME: Keep a reference to the created target framebuffers
		 *        so that they can be properly disposed of.
		 */
		if (i < argc - 1) {
			target = framebuffer_new(gles->width, gles->height);
			if (!target) {
				fprintf(stderr, "failed to create framebuffer\n");
				goto error;
			}
		} else {
			target = display_framebuffer_new(gles->width, gles->height);
			if (!target) {
				fprintf(stderr, "failed to create display\n");
				goto error;
			}
		}

		if (strcmp(argv[i], "fill") == 0) {
			stage = simple_fill_new(gles, target, 1.0, 0.0, 1.0);
			if (!stage) {
				fprintf(stderr, "simple_fill_new() failed\n");
				goto error;
			}
		} else if (strcmp(argv[i], "checkerboard") == 0) {
			stage = checkerboard_new(gles, target);
			if (!stage) {
				fprintf(stderr, "checkerboard_new() failed\n");
				goto error;
			}
		} else if (strcmp(argv[i], "copy") == 0) {
			stage = simple_copy_new(gles, source, target);
			if (!stage) {
				fprintf(stderr, "simple_copy_new() failed\n");
				goto error;
			}
		} else if (strcmp(argv[i], "deinterlace") == 0) {
			stage = deinterlace_new(gles, source, target);
			if (!stage) {
				fprintf(stderr, "deinterlace_new() failed\n");
				goto error;
			}
		} else if (strcmp(argv[i], "cc") == 0) {
			stage = color_correct_new(gles, source, target);
			if (!stage) {
				fprintf(stderr, "color_correct_new() failed\n");
				goto error;
			}
		} else {
			fprintf(stderr, "unsupported pipeline stage: %s\n",
				argv[i]);
			goto error;
		}

		/*
		 * Only add the generator to the pipeline if the regenerate
		 * flag was passed. Otherwise, render it only once.
		 */
		if (i > 0 || regenerate) {
			pipeline_add_stage(pipeline, stage);
		} else {
			stage->pipeline = pipeline;
			stage->render(stage);
			pipeline_stage_free(stage);
		}

		if (i < argc - 1)
			source = target;
	}

	return pipeline;

error:
	framebuffer_free(target);
	pipeline_free(pipeline);
	return NULL;
}

static void usage(FILE *fp, const char *program)
{
	fprintf(fp, "Usage: %s [options] TESTCASE\n", program);
	fprintf(fp, "Options:\n");
	fprintf(fp, "  -d, --depth DEPTH  Set color depth.\n");
	fprintf(fp, "  -h, --help         Display help screen and exit.\n");
	fprintf(fp, "  -r, --regenerate   Regenerate test pattern for every frame.\n");
	fprintf(fp, "  -V, --version      Display program version and exit.\n");
	fprintf(fp, "\n");
	fprintf(fp, "Test cases:\n");
	fprintf(fp, "  blank         \n");
	fprintf(fp, "  copy          \n");
	fprintf(fp, "  one_source    \n");
	fprintf(fp, "  deinterlace   \n");
}

static inline uint64_t timespec_to_usec(const struct timespec *tp)
{
	return tp->tv_sec * 1000000 + tp->tv_nsec / 1000;
}

int main(int argc, char **argv)
{
	static const struct option options[] = {
		{ "depth", 1, NULL, 'd' },
		{ "help", 0, NULL, 'h' },
		{ "regenerate", 0, NULL, 'r' },
		{ "subdivisions", 1, NULL, 's' },
		{ "transform", 0, NULL, 't' },
		{ "version", 0, NULL, 'V' },
		{ NULL, 0, NULL, 0 },
	};
	struct framebuffer *display;
	struct framebuffer *source;
	struct pipeline *pipeline;
	unsigned long depth = 24;
	bool regenerate = false;
	float duration, texels;
	unsigned int frames;
	uint64_t start, end;
	struct timespec ts;
	struct gles *gles;
	int opt;

	while ((opt = getopt_long(argc, argv, "d:hs:tV", options, NULL)) != -1) {
		switch (opt) {
		case 'd':
			depth = strtoul(optarg, NULL, 10);
			if (!depth) {
				fprintf(stderr, "invalid depth: %s\n", optarg);
				return 1;
			}
			break;

		case 'h':
			usage(stdout, argv[0]);
			return 0;

		case 'r':
			regenerate = true;
			break;

		case 's':
			subdivisions = strtoul(optarg, NULL, 10);
			break;

		case 't':
			transform = true;
			break;

		case 'V':
			printf("%s %s\n", argv[0], PACKAGE_VERSION);
			return 0;

		default:
			fprintf(stderr, "invalid option: '%c'\n", opt);
			return 1;
		}
	}

	if (optind >= argc) {
		usage(stderr, argv[0]);
		return 1;
	}

	gles = gles_new(depth, regenerate);
	if (!gles) {
		fprintf(stderr, "gles_new() failed\n");
		return 1;
	}

	display = display_framebuffer_new(gles->width, gles->height);
	if (!display) {
		fprintf(stderr, "display_framebuffer_new() failed\n");
		return 1;
	}

	source = framebuffer_new(gles->width, gles->height);
	if (!source) {
		fprintf(stderr, "failed to create framebuffer\n");
		return 1;
	}

	pipeline = create_pipeline(gles, argc - optind, &argv[optind],
				   regenerate);
	if (!pipeline) {
		fprintf(stderr, "failed to create pipeline\n");
		return 1;
	}

	texels = gles->width * gles->height * FRAME_COUNT;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	start = timespec_to_usec(&ts);

	for (frames = 0; frames < FRAME_COUNT; frames++)
		pipeline_render(pipeline);

	clock_gettime(CLOCK_MONOTONIC, &ts);
	end = timespec_to_usec(&ts);

	pipeline_free(pipeline);
	framebuffer_free(source);
	display_framebuffer_free(display);
	gles_free(gles);

	duration = (end - start) / 1000000.0f;
	printf("\tRendered %d frames in %fs\n", FRAME_COUNT, duration);
	printf("\tAverage fps was %.02f\n", FRAME_COUNT / duration);
	printf("\tMTexels/s: %fs\n", (texels / 1000000.0f) / duration);

	return 0;
}
