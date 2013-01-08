/*
 * gstomxbuffertransport.h
 * 
 * This file declares the "OMXBufferTransport" buffer object, which is used
 * to encapsulate an existing OMX buffer object inside of a gStreamer
 * buffer so it can be passed along the gStreamer pipeline.
 * 
 * Copyright (C) 2010-2011 Texas Instruments Incorporated - http://www.ti.com/
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

#ifndef __GST_OMXBUFFERTRANSPORT_H__
#define __GST_OMXBUFFERTRANSPORT_H__

#include <gst/gst.h>

#include "gstomx_util.h"

G_BEGIN_DECLS


/* Type macros for GST_TYPE_OMXBUFFERTRANSPORT */
#define GST_TYPE_OMXBUFFERTRANSPORT \
    (gst_omxbuffertransport_get_type())
#define GST_OMXBUFFERTRANSPORT(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_OMXBUFFERTRANSPORT, \
    GstOmxBufferTransport))
#define GST_IS_OMXBUFFERTRANSPORT(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_OMXBUFFERTRANSPORT))
#define GST_OMXBUFFERTRANSPORT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_OMXBUFFERTRANSPORT, \
    GstOmxBufferTransportClass))
#define GST_IS_OMXBUFFERTRANSPORT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_OMXBUFFERTRANSPORT))
#define GST_OMXBUFFERTRANSPORT_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_OMXBUFFERTRANSPORT, \
    GstOmxBufferTransportClass))

typedef struct _GstOmxBufferTransport      GstOmxBufferTransport;
typedef struct _GstOmxBufferTransportClass GstOmxBufferTransportClass;

/* Utility macros for GST_TYPE_OMXBUFFERTRANSPORT */
#define GST_GET_OMXBUFFER(obj) \
    ((obj) ? GST_OMXBUFFERTRANSPORT(obj)->omxbuffer : NULL)
#define GST_GET_OMXPORT(obj) \
    ((obj) ? GST_OMXBUFFERTRANSPORT(obj)->portptr->port : NULL)


/* _GstOmxBufferTransport object */
struct _GstOmxBufferTransport {
    GstBuffer  parent_instance;
    OMX_BUFFERHEADERTYPE *omxbuffer;
    GstOmxPortPtr *portptr;
	guint numAdditionalHeaders;
	OMX_BUFFERHEADERTYPE **addHeader;
	GstBuffer *parent;
	GSem *bufSem;
};

struct _GstOmxBufferTransportClass {
    GstBufferClass    derived_methods;
};

/* External function declarations */
GType      gst_omxbuffertransport_get_type(void);
GstBuffer* gst_omxbuffertransport_new(GOmxPort *port, OMX_BUFFERHEADERTYPE *buffer);
void gst_omxbuffertransport_set_additional_headers (GstOmxBufferTransport *self ,guint numHeaders,OMX_BUFFERHEADERTYPE **buffer);


G_END_DECLS 

#endif /* __GST_OMXBUFFERTRANSPORT_H__ */
