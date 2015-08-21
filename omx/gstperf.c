/*
 * Copyright (C) 2011-2012 Texas Instruments Inc
 *
 * Author: Brijesh Singh <bksingh@ti.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gstperf.h"

/* Declare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY_STATIC (gst_perf_debug);
#define GST_CAT_DEFAULT gst_perf_debug

/* Property default */
#define DEFAULT_INTERVAL  1
#define PRINT_ARM_LOAD    TRUE
#define PRINT_FPS         TRUE
#define MEASURE_TIME      FALSE

enum
{
  PROP_0,
  PROP_PRINT_ARM_LOAD,
  PROP_PRINT_FPS,
  PROP_MEASURE_TIME
};

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
  GST_PAD_SINK,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS ("ANY")
  );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
  GST_PAD_SRC,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS ("ANY")
  );

static GstElementClass *parent_class = NULL;

static void gst_perf_base_init (gpointer g_class);
static void gst_perf_class_init (GstperfClass * g_class);
static void gst_perf_init (Gstperf * object, GstperfClass * g_class );
static GstFlowReturn gst_perf_transform_ip (GstBaseTransform * trans,GstBuffer * buf);
static gboolean gst_perf_start (GstBaseTransform * trans);
static gboolean gst_perf_stop (GstBaseTransform * trans);
static void gst_perf_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static GstStateChangeReturn gst_perf_change_state (GstElement * element,
    GstStateChange transition);

static gboolean
gst_perf_handle_time_event (Gstperf * perf)
{
	double x_ms , y_ms , diff;

	gettimeofday(&perf->time2,NULL);
	x_ms = (double)perf->time1.tv_sec*1000000 + (double)perf->time1.tv_usec;
    y_ms = (double)perf->time2.tv_sec*1000000 + (double)perf->time2.tv_usec;

    diff = (double)y_ms - (double)x_ms;

    diff = (double) diff / 1000000;

    GST_DEBUG_OBJECT (perf, "Total time elapsed : %f s\n" , diff );

    perf->time1 = (struct timeval){0};
    perf->time2 = (struct timeval){0};
    return GST_FLOW_OK;

}

static gboolean
gst_perf_handle_event (GstBaseTransform * trans, GstEvent * event)
{
  gboolean res = TRUE;
  Gstperf *perf = GST_PERF (trans);


  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
		if (perf->measure_time) {
		  gettimeofday(&perf->time1,NULL);
		  perf->after_seek=TRUE;
		  perf->playing=FALSE;
		  perf->count=0;
	    }
		gst_pad_push_event (trans->sinkpad, event);
      break;
    case GST_EVENT_CUSTOM_TIME:
		if (perf->measure_time && perf->after_seek && perf->playing) {
		  if (perf->count == 1) {
			  res = gst_perf_handle_time_event (perf);
			  perf->after_seek=FALSE;
			  perf->playing= FALSE;
			  perf->count= perf->count+1;
		  }
		  else
			perf->count= perf->count+1;
	  }
      gst_event_unref (event);
      break;
    default:
      gst_pad_push_event (trans->sinkpad, event);
	  break;
  }

  return res;
}

static void
gst_perf_init (Gstperf * perf, GstperfClass * gclass)
{
    Gstperf *self = perf;
    GstBaseTransformClass *trans_class;

    trans_class = (GstBaseTransformClass *) gclass;

    gst_base_transform_set_qos_enabled (GST_BASE_TRANSFORM (perf), TRUE);
    self->fps_update_interval = GST_SECOND * DEFAULT_INTERVAL;
    self->print_arm_load = PRINT_ARM_LOAD;
    self->print_fps = PRINT_FPS;
    self->measure_time = MEASURE_TIME;
    self->after_seek=FALSE;
	self->playing=FALSE;
	self->count=0;
	self->time1 = (struct timeval){0};
	self->time2 = (struct timeval){0};
	trans_class->src_event = gst_perf_handle_event;
}

static gboolean
display_current_fps (gpointer data)
{
    Gstperf *self = GST_PERF (data);
    guint64 frames_count;
    gdouble rr, average_fps;
    gchar fps_message[256];
    gdouble time_diff, time_elapsed;
    GstClockTime current_ts = gst_util_get_timestamp ();
	char *name = GST_OBJECT_NAME(self);

    frames_count = self->frames_count;

    time_diff = (gdouble) (current_ts - self->last_ts) / GST_SECOND;
    time_elapsed = (gdouble) (current_ts - self->start_ts) / GST_SECOND;

    rr = (gdouble) (frames_count - self->last_frames_count) / time_diff;

    average_fps = (gdouble) frames_count / time_elapsed;

    g_snprintf (fps_message, 255, "%s: frames: %" G_GUINT64_FORMAT " \tcurrent: %.2f\t average: %.2f",  
    name, frames_count, rr, average_fps);
    g_print ("%s", fps_message);

    self->last_frames_count = frames_count;
    self->last_ts = current_ts;

    return TRUE;
}

GType
gst_perf_get_type (void)
{
    static GType object_type = 0;

    if (G_UNLIKELY (object_type == 0)) {
        static const GTypeInfo object_info = {
        sizeof (GstperfClass),
        gst_perf_base_init,
        NULL,
        (GClassInitFunc) gst_perf_class_init,
        NULL,
        NULL,
        sizeof (Gstperf),
        0,
        (GInstanceInitFunc) gst_perf_init
    };

    object_type = g_type_register_static (GST_TYPE_BASE_TRANSFORM,
        "Gstperf", &object_info, (GTypeFlags) 0);

    GST_DEBUG_CATEGORY_INIT (gst_perf_debug, "perf", 0, " performance tool");

  }

  return object_type;
};


static void
gst_perf_base_init (gpointer gclass)
{
    static GstElementDetails element_details = {
        " Performance element",
        "Filter/Perf",
        "display performance data",
        "Brijesh Singh, bksingh@ti.com"
    };

    GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

    gst_element_class_add_pad_template (element_class,
        gst_static_pad_template_get (&src_factory));
    gst_element_class_add_pad_template (element_class,
        gst_static_pad_template_get (&sink_factory));
    gst_element_class_set_details (element_class, &element_details);
}

static GstFlowReturn
gst_perf_prepare_output_buffer (GstBaseTransform * trans,
  GstBuffer * in_buf, gint out_size, GstCaps * out_caps, GstBuffer ** out_buf)
{
    *out_buf = gst_buffer_ref (in_buf);
    return GST_FLOW_OK;
}

static void
gst_perf_class_init (GstperfClass * klass)
{
    GObjectClass *gobject_class;
    GstBaseTransformClass *trans_class;
    GstElementClass *element_class;

    element_class = GST_ELEMENT_CLASS (klass);
	element_class->change_state = gst_perf_change_state;

    gobject_class = (GObjectClass *) klass;

    gobject_class->set_property = gst_perf_set_property;
    gobject_class = (GObjectClass *) klass;
    trans_class = (GstBaseTransformClass *) klass;

    trans_class->transform_ip = GST_DEBUG_FUNCPTR (gst_perf_transform_ip);
    trans_class->prepare_output_buffer = GST_DEBUG_FUNCPTR (gst_perf_prepare_output_buffer);
    trans_class->start = GST_DEBUG_FUNCPTR (gst_perf_start);
    trans_class->stop = GST_DEBUG_FUNCPTR (gst_perf_stop);

    trans_class->passthrough_on_same_caps = FALSE;

    parent_class = g_type_class_peek_parent (klass);

    g_object_class_install_property (gobject_class, PROP_PRINT_ARM_LOAD,
        g_param_spec_boolean ("print-arm-load", "print-arm-load",
          "Print the CPU load info", PRINT_ARM_LOAD, G_PARAM_WRITABLE));

    g_object_class_install_property (gobject_class, PROP_PRINT_FPS,
      g_param_spec_boolean ("print-fps", "print-fps",
          "Print framerate", PRINT_FPS, G_PARAM_WRITABLE));

    g_object_class_install_property (gobject_class, PROP_MEASURE_TIME,
      g_param_spec_boolean ("measure-time", "measure-time",
          "Measure time", MEASURE_TIME, G_PARAM_WRITABLE));
}

static void
gst_perf_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
    Gstperf *perf = GST_PERF (object);

    switch (prop_id) {
        case PROP_PRINT_ARM_LOAD:
            perf->print_arm_load = g_value_get_boolean(value);
            break;

        case PROP_PRINT_FPS:
            perf->print_fps = g_value_get_boolean(value);
            break;

        case PROP_MEASURE_TIME:
            perf->measure_time = g_value_get_boolean(value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static gboolean
gst_perf_start (GstBaseTransform * trans)
{
    Gstperf *self = (Gstperf *) trans;

    /* Init counters */
    self->frames_count = G_GUINT64_CONSTANT (0);
    self->total_size = G_GUINT64_CONSTANT (0);
    self->last_frames_count = G_GUINT64_CONSTANT (0);

    /* init time stamps */
    self->last_ts = self->start_ts = self->interval_ts = GST_CLOCK_TIME_NONE;

    return TRUE;
}

static gboolean
gst_perf_stop (GstBaseTransform * trans)
{

    return TRUE;
}

static int 
print_cpu_load (Gstperf *perf)
{
    int   cpuLoadFound = FALSE;
    unsigned long   nice, sys, idle, iowait, irq, softirq, steal;  
    unsigned long   deltaTotal;
    char  textBuf[4];
    FILE *fptr;
    int load;

    /* Read the overall system information */
    fptr = fopen("/proc/stat", "r");

    if (fptr == NULL) {
        return -1;
    }

    perf->prevTotal = perf->total;
    perf->prevuserTime = perf->userTime;

    /* Scan the file line by line */
    while (fscanf(fptr, "%4s %lu %lu %lu %lu %lu %lu %lu %lu", textBuf,
                  &perf->userTime, &nice, &sys, &idle, &iowait, &irq, &softirq,
                  &steal) != EOF) {
        if (strcmp(textBuf, "cpu") == 0) {
            cpuLoadFound = TRUE;
            break;
        }
    }

    if (fclose(fptr) != 0) {
        return -1;
    }

    if (!cpuLoadFound) {
        return -1;
    }

    perf->total = perf->userTime + nice + sys + idle + iowait + irq + softirq +
                  steal;
    perf->userTime += nice + sys + iowait + irq + softirq + steal;
    deltaTotal = perf->total - perf->prevTotal;

    if(deltaTotal) {
        load = 100 * (perf->userTime - perf->prevuserTime) / deltaTotal;
    } else {
        load = 0;
    }

    g_print ("\tarm-load: %d", load);
    return 0;
}

static GstFlowReturn
gst_perf_transform_ip (GstBaseTransform * trans, GstBuffer * buf)
{
    Gstperf *self = GST_PERF (trans);
    GstClockTime ts;

    {
        self->frames_count++;
        self->total_size += GST_BUFFER_SIZE(buf);

        ts = gst_util_get_timestamp ();
        if (G_UNLIKELY (!GST_CLOCK_TIME_IS_VALID (self->start_ts))) {
            self->interval_ts = self->last_ts = self->start_ts = ts;
        }

        if (GST_CLOCK_DIFF (self->interval_ts, ts) > self->fps_update_interval) {

            if (self->print_fps) 
                display_current_fps (self);

            if (self->print_arm_load) 
                print_cpu_load (self);

            g_print ("\n");
            self->interval_ts = ts;
        }
    }

    return GST_FLOW_OK;;
}

static GstStateChangeReturn
gst_perf_change_state (GstElement * element, GstStateChange transition)
{
  Gstperf *perf;
  GstStateChangeReturn ret;

  perf = GST_PERF (element);
  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      if (perf->measure_time)
		perf->playing = TRUE;
      break;
    default:
      break;
  }

  return ret;
}
