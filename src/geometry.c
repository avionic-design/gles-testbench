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

#include "geometry.h"

static GLfloat gluRandom(GLfloat min, GLfloat max)
{
	return min + (max - min) * rand() / RAND_MAX;
}

struct geometry *grid_new(unsigned int subdivisions)
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
	grid->num_cols = num_cols;
	grid->num_rows = num_rows;

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
			v[(i * 3) + 0] = -1.0f + (2.0f * i / num_cols);
			v[(i * 3) + 1] = -1.0f + (2.0f * j / num_rows);
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

void geometry_free(struct geometry *geometry)
{
	if (geometry) {
		free(geometry->vertices);
		free(geometry->indices);
		free(geometry->uv);
	}

	free(geometry);
}

void grid_randomize(struct geometry *grid)
{
	GLfloat dx = 0.25f / grid->num_cols;
	GLfloat dy = 0.25f / grid->num_rows;
	unsigned int i, j;

	for (j = 0; j <= grid->num_rows; j++) {
		GLfloat *v = grid->vertices + j * (grid->num_cols + 1) * 3;

		for (i = 0; i <= grid->num_cols; i++) {
			if (i > 0 && i < grid->num_cols)
				v[i * 3 + 0] += gluRandom(-1.0f, 1.0f) * dx;

			if (j > 0 && j < grid->num_rows)
				v[i * 3 + 1] += gluRandom(-1.0f, 1.0f) * dy;
		}
	}
}
