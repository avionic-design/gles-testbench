/*
 * =====================================================================================
 *
 *       Filename:  gles-standalone.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  11/21/12 09:50:34
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
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

#include <glib.h>
#include <glib-object.h>

#include "gstglessink.h"
#define FRAME_COUNT 600

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

int main(int argc, char **argv)
{
	static const struct option options[] = {
		{ "depth", 1, NULL, 'd' },
		{ "help", 0, NULL, 'h' },
		{ "regenerate", 0, NULL, 'r' },
		{ "version", 0, NULL, 'V' },
		{ NULL, 0, NULL, 0 },
	};
	unsigned long depth = 24;
	bool regenerate = false;
	GstGLESSink *sink;
	gint64 start, stop;
	float duration;
	int i = 0, opt, err;

	while ((opt = getopt_long(argc, argv, "d:hV", options, NULL)) != -1) {
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

	g_type_init();

	sink = g_new0(GstGLESSink, 1);
	if (!sink) {
		g_error("Out of memory");
		return -ENOMEM;
	}

	err = gst_gles_sink_init(sink, depth);
	if (err < 0) {
		fprintf(stderr, "failed to initialize GLES: %s\n",
			strerror(-err));
		return 1;
	}

	if (strcmp(argv[optind], "blank") == 0)
		sink->mode = GLES_BLANK;
	else if (strcmp(argv[optind], "copy") == 0)
		sink->mode = GLES_COPY;
	else if (strcmp(argv[optind], "one_source") == 0)
		sink->mode = GLES_ONE_SOURCE;
	else if (strcmp(argv[optind], "deinterlace") == 0)
		sink->mode = GLES_DEINTERLACE;

	gst_gles_sink_preroll(sink, regenerate);

	start = g_get_monotonic_time();
	while (i++ < FRAME_COUNT) {
		gst_gles_sink_render(sink, regenerate);
	}
	stop = g_get_monotonic_time();

	gst_gles_sink_finalize(sink);

	duration = (stop - start) / 1000000.0;
	g_print("\tRendered %d frames in %fs\n", FRAME_COUNT, duration);
	g_print("\tAverage fps was %.02f\n", FRAME_COUNT / duration);

	return 0;
}
