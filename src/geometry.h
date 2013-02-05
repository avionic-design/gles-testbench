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

#ifndef GLES_TESTBENCH_GEOMETRY_H
#define GLES_TESTBENCH_GEOMETRY_H 1

#include <GLES2/gl2.h>

struct geometry {
	unsigned int num_cols;
	unsigned int num_rows;

	unsigned int num_vertices;
	GLfloat *vertices;
	GLfloat *uv;

	unsigned int num_indices;
	GLushort *indices;
};

struct geometry *grid_new(unsigned int subdivisions);
void geometry_free(struct geometry *geometry);
void grid_randomize(struct geometry *grid);

#endif
