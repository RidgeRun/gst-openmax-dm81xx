/*
 * gsttipriority.h
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

#ifndef __GST_TIPRIORITY_H__
#define __GST_TIPRIORITY_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS
/* Standard macros for manipulating TIPriority objects */
#define GST_TYPE_TIPRIORITY \
  (gst_tipriority_get_type())
#define GST_TIPRIORITY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TIPRIORITY,GstTIPriority))
#define GST_TIPRIORITY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TIPRIORITY,GstTIPriorityClass))
#define GST_IS_TIPRIORITY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TIPRIORITY))
#define GST_IS_TIPRIORITY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TIPRIORITY))
typedef struct _GstTIPriority GstTIPriority;
typedef struct _GstTIPriorityClass GstTIPriorityClass;

/* _GstTIPriority object */
struct _GstTIPriority
{
  /* gStreamer infrastructure */
  GstBaseTransform element;
  GstPad *sinkpad;
  GstPad *srcpad;

  gint nice;
  gint nice_changed;
  gint rtpriority;
  gint scheduler;
  gboolean rt_changed;
  gint rtmin;
  gint rtmax;
  gboolean hasBeenPrioritized;
};

/* _GstTIPriorityClass object */
struct _GstTIPriorityClass
{
  GstBaseTransformClass parent_class;
};

/* External function declarations */
GType gst_tipriority_get_type (void);

G_END_DECLS
#endif /* __GST_TIPRIORITY_H__ */
