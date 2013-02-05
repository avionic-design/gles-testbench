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

#include <stdlib.h>

#include "pipeline.h"
#include "gles.h"

void pipeline_stage_free(struct pipeline_stage *stage)
{
	if (stage && stage->release)
		stage->release(stage);
}

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


