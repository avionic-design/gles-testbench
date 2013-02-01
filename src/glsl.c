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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gles.h"

struct glsl_shader *glsl_shader_new(GLenum type, const GLchar *lines[],
				    GLint count)
{
	struct glsl_shader *shader;
	GLint status;

	shader = calloc(1, sizeof(*shader));
	if (!shader)
		return NULL;

	shader->id = glCreateShader(type);
	if (!shader->id) {
		free(shader);
		return NULL;
	}

	glShaderSource(shader->id, count, lines, NULL);
	glCompileShader(shader->id);

	glGetShaderiv(shader->id, GL_COMPILE_STATUS, &status);
	if (!status) {
		GLint size;

		fprintf(stderr, "failed to compile GLSL shader:\n");

		glGetShaderiv(shader->id, GL_INFO_LOG_LENGTH, &size);
		if (size > 0) {
			GLint length;
			GLchar *log;

			log = malloc(sizeof(GLchar) * size);
			if (!log) {
				fprintf(stderr, "out of memory\n");
				goto delete;
			}

			glGetShaderInfoLog(shader->id, size, &length, log);
			fprintf(stderr, "%.*s\n", length, log);
			free(log);
		}

delete:
		glDeleteShader(shader->id);
		free(shader);
		return NULL;
	}

	return shader;
}

void glsl_shader_free(struct glsl_shader *shader)
{
	glDeleteShader(shader->id);
	free(shader);
}

struct glsl_program *glsl_program_new(struct glsl_shader *vertex,
				      struct glsl_shader *fragment)
{
	struct glsl_program *program;
	GLint err;

	program = calloc(1, sizeof(*program));
	if (!program)
		return NULL;

	program->id = glCreateProgram();
	if (!program->id) {
		free(program);
		return NULL;
	}

	program->vs = vertex;
	program->fs = fragment;

	glAttachShader(program->id, program->vs->id);

	err = glGetError();
	if (err != GL_NO_ERROR) {
		fprintf(stderr, "failed to attach vertex shader: %04x\n", err);
		glDeleteProgram(program->id);
		free(program);
		return NULL;
	}

	glAttachShader(program->id, program->fs->id);

	err = glGetError();
	if (err != GL_NO_ERROR) {
		fprintf(stderr, "failed to attach fragment shader: %04x\n", err);
		glDeleteProgram(program->id);
		free(program);
		return NULL;
	}

	return program;
}

int glsl_program_link(struct glsl_program *program)
{
	GLint status;

	glBindAttribLocation(program->id, 0, "vPosition");
	glLinkProgram(program->id);

	/* check linker status */
	glGetProgramiv(program->id, GL_LINK_STATUS, &status);
	if (!status) {
		GLint size;

		fprintf(stderr, "failed to link GLSL program:\n");

		glGetProgramiv(program->id, GL_INFO_LOG_LENGTH, &size);
		if (size > 0) {
			GLsizei length;
			GLchar *log;

			log = malloc(sizeof(GLchar) * size);
			if (!log) {
				fprintf(stderr, "out of memory\n");
				return -1;
			}

			glGetProgramInfoLog(program->id, size, &length, log);
			fprintf(stderr, "%.*s\n", length, log);
			free(log);
		}

		glDeleteProgram(program->id);
		free(program);
		return -1;
	}

	return 0;
}

void glsl_program_free(struct glsl_program *program)
{
	glsl_shader_free(program->fs);
	glsl_shader_free(program->vs);
	glDeleteProgram(program->id);
	free(program);
}
