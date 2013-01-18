/*
 * gsttipriority.c
 *
 * This file declares the "priority" element, which modifies the priorities
 * and scheduling of the gstreamer threads.
 *
 * Original Author:
 *     Diego Dompe, RidgeRun
 *
 * Copyright (C) 2009 Karlstorz
 * Copyritht (C) 2010 RidgeRun
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation version 2.1 of the License.
 *
 * This program is distributed #as is# WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sched.h>
#include <gst/gst.h>

#include "gstomx_priority.h"

/* Declare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY_STATIC (gst_tipriority_debug);
#define GST_CAT_DEFAULT gst_tipriority_debug

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

enum
{
  ARG_0,
  ARG_NICE,
  ARG_SCHEDULER,
  ARG_RTPRIORITY,
};

/* Static Function Declarations */
static void gst_tipriority_base_init (gpointer g_class);
static void gst_tipriority_class_init (GstTIPriorityClass * g_class);
static GstFlowReturn gst_tipriority_transform_ip (GstBaseTransform * trans,
    GstBuffer * buf);
static gboolean gst_tipriority_start (GstBaseTransform * trans);
static void gst_tipriority_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_tipriority_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static void
gst_tipriority_init (GstTIPriority * priority)
{
  gst_base_transform_set_qos_enabled (GST_BASE_TRANSFORM (priority), TRUE);
  priority->nice = 0;
  priority->nice_changed = 0;
  priority->scheduler = 0;
  priority->rtpriority = 0;
  priority->rt_changed = FALSE;
  priority->rtmin = 0;
  priority->rtmax = 0;
  priority->hasBeenPrioritized = FALSE;
}

GType
gst_tipriority_get_type (void)
{
  static GType object_type = 0;

  GST_LOG ("Begin\n");
  if (G_UNLIKELY (object_type == 0)) {
    static const GTypeInfo object_info = {
      sizeof (GstTIPriorityClass),
      gst_tipriority_base_init,
      NULL,
      (GClassInitFunc) gst_tipriority_class_init,
      NULL,
      NULL,
      sizeof (GstTIPriority),
      0,
      (GInstanceInitFunc) gst_tipriority_init
    };

    object_type = g_type_register_static (GST_TYPE_BASE_TRANSFORM,
        "GstTIPriority", &object_info, (GTypeFlags) 0);

    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT (gst_tipriority_debug, "TIPriority", 0,
        "TI Priority adjuster");

    GST_LOG ("initialized get_type\n");

  }

  GST_LOG ("Finish\n");
  return object_type;
};

static void
gst_tipriority_base_init (gpointer gclass)
{
  static GstElementDetails element_details = {
    "TI Priority adjuster",
    "Misc",
    "This element can change the priorites of segments of a pipeline",
    "Diego Dompe; RidgeRun"
  };
  GST_LOG ("Begin\n");

  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));
  gst_element_class_set_details (element_class, &element_details);
  GST_LOG ("Finish\n");
}

static void
gst_tipriority_class_init (GstTIPriorityClass * klass)
{
  GstBaseTransformClass *trans_class;
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) klass;
  trans_class = (GstBaseTransformClass *) klass;

  GST_LOG ("Begin\n");
  trans_class->transform_ip = GST_DEBUG_FUNCPTR (gst_tipriority_transform_ip);
  trans_class->start = GST_DEBUG_FUNCPTR (gst_tipriority_start);
  trans_class->passthrough_on_same_caps = TRUE;
  gobject_class->set_property = gst_tipriority_set_property;
  gobject_class->get_property = gst_tipriority_get_property;

  g_object_class_install_property (G_OBJECT_CLASS (klass),
      ARG_NICE,
      g_param_spec_int ("nice",
          "nice",
          "Nice value for the thread. Only valid for scheduler OTHER",
          -20, 19, 0, G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      ARG_SCHEDULER,
      g_param_spec_int ("scheduler",
          "scheduler",
          "Scheduler to use: 0 - OTHER, 1 - RT FIFO, 2 - RT RoundRobin",
          0, 2, 0, G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      ARG_RTPRIORITY,
      g_param_spec_int ("rtpriority",
          "rtpriority",
          "Real time priority: 1 (lower), 99 (higher). "
          "Only valid for scheduler RT FIFO or RT RR ",
          1, 99, 1, G_PARAM_READWRITE));

  GST_LOG ("Finish\n");
}

static gboolean
gst_tipriority_start (GstBaseTransform * trans)
{
  GstTIPriority *priority = GST_TIPRIORITY (trans);
  struct sched_param param;

  priority->nice = getpriority (PRIO_PROCESS, 0);
  priority->rtmin = sched_get_priority_min (SCHED_FIFO);
  priority->rtmax = sched_get_priority_max (SCHED_FIFO);
  if (!priority->scheduler) {
	priority->scheduler = sched_getscheduler (0);
  }
  if (priority->scheduler != SCHED_OTHER && !priority->rtpriority) {
    sched_getparam (0, &param);
    priority->rtpriority = param.sched_priority;
  }

  if (priority->rtmax < priority->rtmin) {
    GST_ELEMENT_WARNING (priority, RESOURCE, FAILED, (NULL),
        ("Your kernel rt max priority is less than your min priority"));
  }
  GST_INFO ("RT priorities: min %d, max %d", priority->rtmin, priority->rtmax);

  return TRUE;
}

static void
gst_tipriority_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIPriority *priority = GST_TIPRIORITY (object);

  switch (prop_id) {
    case ARG_NICE:{
      g_value_set_int (value, priority->nice);
      break;
    }
    case ARG_SCHEDULER:{
      g_value_set_int (value, priority->scheduler);
      break;
    }
    case ARG_RTPRIORITY:{
      g_value_set_int (value, priority->rtpriority);
      break;
    }

    default:{
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
  }
}

static void
gst_tipriority_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIPriority *priority = GST_TIPRIORITY (object);

  switch (prop_id) {
    case ARG_NICE:{
      gint a = g_value_get_int (value) + 20;
      gint b = getpriority (PRIO_PROCESS, 0) + 20;
      priority->nice_changed = a - b;
      priority->hasBeenPrioritized = FALSE;
      GST_WARNING_OBJECT(priority,"Nice values: a %d, b %d, nice change %d",a,b,priority->nice_changed);
      break;
    }
    case ARG_SCHEDULER:{
      priority->scheduler = g_value_get_int (value);
      priority->rt_changed = TRUE;
      priority->hasBeenPrioritized = FALSE;
      break;
    }
    case ARG_RTPRIORITY:{
      priority->rtpriority = g_value_get_int (value);
      priority->rt_changed = TRUE;
      priority->hasBeenPrioritized = FALSE;
      break;
    }

    default:{
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
  }
}

static GstFlowReturn
gst_tipriority_transform_ip (GstBaseTransform * trans, GstBuffer * buf)
{
  gint newnice;
  GstTIPriority *priority = GST_TIPRIORITY (trans);
  if (priority->hasBeenPrioritized == FALSE ) {
	  if (priority->nice_changed) {
		errno = 0;
		newnice = nice (priority->nice_changed);
		if ((newnice == -1) && (errno)) {
		  GST_ELEMENT_WARNING (priority, RESOURCE, FAILED, (NULL),
			  ("Failed to set the request nice level, errno %d", errno));
		} else {
		  GST_INFO ("%s: nice value is now %d",
			  gst_element_get_name (priority), newnice);
		}
		priority->nice_changed = 0;
		priority->nice = newnice;
	  }

	  if (priority->rt_changed) {
		int policy;
		struct sched_param param;

		switch (priority->scheduler) {
		  case 2:
			policy = SCHED_RR;
			break;
		  case 1:
			policy = SCHED_FIFO;
			break;
		  case 0:
		  default:
			policy = SCHED_OTHER;
			break;
		}
		param.sched_priority = priority->rtpriority;
		if (sched_setscheduler (0, policy, &param) == -1) {
		  GST_ELEMENT_WARNING (priority, RESOURCE, FAILED, (NULL),
			  ("Failed to set the request rt scheduler (%d) or priority (%d),"
				  " errno %d", priority->scheduler,priority->rtpriority,errno));
		}
		priority->rt_changed = FALSE;
	  }
	  priority->hasBeenPrioritized = TRUE;
  }
  return GST_FLOW_OK;
}
