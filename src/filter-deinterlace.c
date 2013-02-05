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

struct deinterlace {
	struct pipeline_stage base;

	struct geometry *geometry;
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

static const GLchar *deinterlace_fs[] = {
	"precision mediump float;\n",
	"uniform sampler2D source;\n",
	"uniform float offset;\n"
	"varying vec2 vtex;\n",
	"\n",
	"void main()\n",
	"{\n",
	"    vec2 above, below;\n",
	"    vec4 sum;\n",
	"\n",
	"    above.x = vtex.x;\n",
	"    above.y = vtex.y - offset;\n",
	"    below.x = vtex.x;\n",
	"    below.y = vtex.y + offset;\n",
	"\n",
	"    gl_FragColor = texture2D(source, above) * 0.3 +\n",
	"                   texture2D(source, vtex) * 0.4 +\n",
	"                   texture2D(source, below) * 0.3;\n",
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
	struct geometry *geometry = deinterlace->geometry;
	struct gles *gles = stage->pipeline->gles;

	glBindFramebuffer(GL_FRAMEBUFFER, deinterlace->target->id);
	glUseProgram(deinterlace->program->id);

	glVertexAttribPointer(deinterlace->pos, 3, GL_FLOAT, GL_FALSE,
			      3 * sizeof(GLfloat), geometry->vertices);
	glEnableVertexAttribArray(deinterlace->pos);

	glVertexAttribPointer(deinterlace->tex, 2, GL_FLOAT, GL_FALSE,
			      2 * sizeof(GLfloat), geometry->uv);
	glEnableVertexAttribArray(deinterlace->tex);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, deinterlace->source->texture->id);
	glUniform1i(deinterlace->input, 0);

	glUniform1f(deinterlace->offset, 1.0f / gles->width);

	glDrawElements(GL_TRIANGLES, geometry->num_indices, GL_UNSIGNED_SHORT,
		       geometry->indices);
}

struct pipeline_stage *deinterlace_new(struct gles *gles,
				       struct geometry *geometry,
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

	stage->geometry = geometry;
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
