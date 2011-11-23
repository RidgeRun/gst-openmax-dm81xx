/*
 * Copyright (C) 2006-2009 Texas Instruments, Incorporated
 * Copyright (C) 2007-2009 Nokia Corporation.
 *
 * Author: Felipe Contreras <felipe.contreras@nokia.com>
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

#ifndef GSTOMX_PORT_H
#define GSTOMX_PORT_H

#include <string.h> /* for memset, memcpy */
#include <gst/gst.h>

#include "gstomx_util.h"

G_BEGIN_DECLS

/* Typedefs. */

typedef enum GOmxPortType GOmxPortType;
typedef struct OmxBufferInfo OmxBufferInfo;

/* Enums. */

enum GOmxPortType
{
    GOMX_PORT_INPUT,
    GOMX_PORT_OUTPUT
};

struct OmxBufferInfo 
{
    /** number of pBuffer pointer */
    guint num_buffers;
    
    /* arrary of OMX pBuffer pointer */
    OMX_U8 **pBuffer;
};

struct GOmxPort
{
    GOmxCore *core;
    gchar *name;
    GOmxPortType type;

    guint num_buffers;
    guint port_index;
    OMX_BUFFERHEADERTYPE **buffers;

    GMutex *mutex;
    gboolean enabled;
    gboolean omx_allocate; /**< Setup with OMX_AllocateBuffer rather than OMX_UseBuffer */
    AsyncQueue *queue;

    GstBuffer * (*buffer_alloc)(GOmxPort *port, gint len); /**< allows elements to override shared buffer allocation for output ports */

    /** @todo this is a hack.. OpenMAX IL spec should be revised. */
    gboolean share_buffer;

    gint ignore_count;  /* XXX hack to work around seek bug w/ codec */

    /** nOffset value of the last received (input) or next sent (output) port */
    guint n_offset;     /* a bit ugly.. but..  */

    /** variable to indicate if the conversion from elementary to intermediate video data is done */
    gboolean vp6_hack;  /* only needed for vp6 */

    /** varaible to indicate if we need to perform memcpy of incoming or outgoing gstreamer buffer into OMX buffer. */
    gboolean always_copy;

    /** variable to store caps for sinkpad */
    GstCaps *caps;

    /** if omx_allocate flag is not set then structure will contain upstream omx buffer pointer information */
    OmxBufferInfo *share_buffer_info;   
};

/* Macros. */

#define G_OMX_PORT_GET_PARAM(port, idx, param) G_STMT_START {  \
		_G_OMX_INIT_PARAM (param);                         \
        (param)->nPortIndex = (port)->port_index;          \
        OMX_GetParameter (g_omx_core_get_handle ((port)->core), idx, (param)); \
    } G_STMT_END

#define G_OMX_PORT_SET_PARAM(port, idx, param)                      \
        OMX_SetParameter (                                          \
            g_omx_core_get_handle ((port)->core), idx, (param))

#define G_OMX_PORT_GET_CONFIG(port, idx, param) G_STMT_START {  \
        _G_OMX_INIT_PARAM (param);                         \
        (param)->nPortIndex = (port)->port_index;          \
        OMX_GetConfig (g_omx_core_get_handle ((port)->core), idx, (param)); \
    } G_STMT_END

#define G_OMX_PORT_SET_CONFIG(port, idx, param)                     \
        OMX_SetConfig (                                             \
            g_omx_core_get_handle ((port)->core), idx, (param))

#define G_OMX_PORT_GET_DEFINITION(port, param) \
        G_OMX_PORT_GET_PARAM (port, OMX_IndexParamPortDefinition, param)

#define G_OMX_PORT_SET_DEFINITION(port, param) \
        G_OMX_PORT_SET_PARAM (port, OMX_IndexParamPortDefinition, param)


/* Functions. */

GOmxPort *g_omx_port_new (GOmxCore *core, const gchar *name, guint index);
void g_omx_port_free (GOmxPort *port);

void g_omx_port_setup (GOmxPort *port, OMX_PARAM_PORTDEFINITIONTYPE *omx_port);
void g_omx_port_prepare (GOmxPort *port);
void g_omx_port_allocate_buffers (GOmxPort *port);
void g_omx_port_free_buffers (GOmxPort *port);
void g_omx_port_start_buffers (GOmxPort *port);
void g_omx_port_resume (GOmxPort *port);
void g_omx_port_pause (GOmxPort *port);
void g_omx_port_flush (GOmxPort *port);
void g_omx_port_enable (GOmxPort *port);
void g_omx_port_disable (GOmxPort *port);
void g_omx_port_finish (GOmxPort *port);
void g_omx_port_push_buffer (GOmxPort *port, OMX_BUFFERHEADERTYPE *omx_buffer);
gint g_omx_port_send (GOmxPort *port, gpointer obj);
gpointer g_omx_port_recv (GOmxPort *port);

/*
 * Some domain specific port related utility functions:
 */

#define GSTOMX_ALL_FORMATS  "{ NV12, I420, YUY2, UYVY }"

GstCaps * g_omx_port_set_video_formats (GOmxPort *port, GstCaps *caps);
GstCaps * g_omx_port_set_image_formats (GOmxPort *port, GstCaps *caps);

G_END_DECLS

#endif /* GSTOMX_PORT_H */
