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
#include <glib.h>
#include <errno.h>

#include "gstglessink.h"

int main(int argc, char **argv)
{
	GstGLESSink *sink;
	int i = 0;

	g_type_init();
	sink = g_new0(GstGLESSink, 1);
	if (!sink) {
		g_error("Out of memory");
		return -ENOMEM;
	}

	gst_gles_sink_init(sink);
	gst_gles_sink_preroll(sink);

	while (i++ < 600) {
		gst_gles_sink_render(sink);
	}
}
