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
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <errno.h>

#include "gstglessink.h"
#define FRAME_COUNT 600

int main(int argc, char **argv)
{
	GstGLESSink *sink;
	gint64 start, stop;
	float duration;
	int i = 0;

	if (argc < 2) {
		printf("usage: %s [blank|copy|one_source|deinterlace]\n",
		       argv[0]);
		exit(1);
	}
	g_type_init();
	sink = g_new0(GstGLESSink, 1);
	if (!sink) {
		g_error("Out of memory");
		return -ENOMEM;
	}

	gst_gles_sink_init(sink);

	if (strcmp(argv[1], "blank") == 0)
		sink->mode = GLES_BLANK;
	else if (strcmp(argv[1], "copy") == 0)
		sink->mode = GLES_COPY;
	else if (strcmp(argv[1], "one_source") == 0)
		sink->mode = GLES_ONE_SOURCE;
	else if (strcmp(argv[1], "deinterlace") == 0)
		sink->mode = GLES_DEINTERLACE;

	gst_gles_sink_preroll(sink);

	start = g_get_monotonic_time();
	while (i++ < FRAME_COUNT) {
		gst_gles_sink_render(sink);
	}
	stop = g_get_monotonic_time();

	duration = (stop - start) / 1000000.0;
	g_print("\tRendered %d frames in %fs\n", FRAME_COUNT, duration);
	g_print("\tAverage fps was %.02f\n", FRAME_COUNT / duration);
}
