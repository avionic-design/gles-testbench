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

#include "pipeline.h"
#include "geometry.h"
#include "gles.h"

#define FRAME_COUNT 600

static unsigned int subdivisions = 0;
static bool transform = false;

static struct pipeline *create_pipeline(struct gles *gles, int argc,
					char *argv[], bool regenerate)
{
	struct framebuffer *source = NULL, *target = NULL;
	struct geometry *plane, *output, *geometry;
	struct pipeline *pipeline;
	int i;

	pipeline = pipeline_new(gles);
	if (!pipeline)
		return NULL;

	/*
	 * FIXME: Keep a reference to the created geometry so that it can be
	 *        properly disposed of.
	 */
	plane = grid_new(0);
	if (!plane)
		return NULL;

	output = grid_new(subdivisions);
	if (!output)
		return NULL;

	if (transform)
		grid_randomize(output);

	for (i = 0; i < argc; i++) {
		struct pipeline_stage *stage = NULL;

		/*
		 * Render intermediate stages to a plane (2 triangles) geometry
		 * and the final one to a randomized grid to simulate geometric
		 * adaption.
		 */
		if (i >= argc - 1)
			geometry = output;
		else
			geometry = plane;

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
			stage = simple_fill_new(gles, geometry, target,
						1.0, 0.0, 1.0);
			if (!stage) {
				fprintf(stderr, "simple_fill_new() failed\n");
				goto error;
			}
		} else if (strcmp(argv[i], "checkerboard") == 0) {
			stage = checkerboard_new(gles, geometry, target);
			if (!stage) {
				fprintf(stderr, "checkerboard_new() failed\n");
				goto error;
			}
		} else if (strcmp(argv[i], "clear") == 0) {
			stage = clear_new(gles, target, 1.0f, 1.0f, 0.0f);
			if (!stage) {
				fprintf(stderr, "clear_new() failed\n");
				goto error;
			}
		} else if (strcmp(argv[i], "copy") == 0) {
			stage = simple_copy_new(gles, geometry, source, target);
			if (!stage) {
				fprintf(stderr, "simple_copy_new() failed\n");
				goto error;
			}
		} else if (strcmp(argv[i], "copyone") == 0) {
			stage = copy_one_new(gles, geometry, source, target);
			if (!stage) {
				fprintf(stderr, "copy_one_new() failed\n");
				goto error;
			}
		} else if (strcmp(argv[i], "deinterlace") == 0) {
			stage = deinterlace_new(gles, geometry, source, target);
			if (!stage) {
				fprintf(stderr, "deinterlace_new() failed\n");
				goto error;
			}
		} else if (strcmp(argv[i], "cc") == 0) {
			stage = color_correct_new(gles, geometry, source,
						  target);
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
		if (i > 0 || regenerate || argc == 1) {
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
	fprintf(fp, "Usage: %s [options] PIPELINE...\n", program);
	fprintf(fp, "Options:\n");
	fprintf(fp, "  -d, --depth DEPTH     Set color depth.\n");
	fprintf(fp, "  -h, --help            Display help screen and exit.\n");
	fprintf(fp, "  -r, --regenerate      Regenerate test pattern for every frame.\n");
	fprintf(fp, "  -s, --subdivisions N  Use N subdivisions to generate geometry.\n");
	fprintf(fp, "  -t, --transform       Transform generated geometry.\n");
	fprintf(fp, "  -V, --version         Display program version and exit.\n");
	fprintf(fp, "\n");
	fprintf(fp, "Pipeline Stages:\n");
	fprintf(fp, "  fill          simple uniform fill generator\n");
	fprintf(fp, "  checkerboard  checkerboard generator\n");
	fprintf(fp, "  clear         clear generator\n");
	fprintf(fp, "  copy          simple copy\n");
	fprintf(fp, "  copyone       copy a single source pixel\n");
	fprintf(fp, "  deinterlace   linear deinterlacer\n");
	fprintf(fp, "  cc            color correction\n");
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

	while ((opt = getopt_long(argc, argv, "d:hrs:tV", options, NULL)) != -1) {
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
	printf("Rendered %d frames in %fs\n", FRAME_COUNT, duration);
	printf("Average fps was %.02f\n", FRAME_COUNT / duration);
	printf("MTexels/s: %fs\n", (texels / 1000000.0f) / duration);

	return 0;
}
