/*
 * Copyright (C) 2007-2009 Nokia Corporation.
 * Copyright (C) 2008 NXP.
 *
 * Authors:
 * Felipe Contreras <felipe.contreras@nokia.com>
 * Frederik Vernelen <frederik.vernelen@tass.be>
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

#include "gstomx_base_sink.h"
#include "gstomx.h"
#include "gstomx_interface.h"
#include "gstomx_buffertransport.h"

#include <string.h> /* for memset, memcpy */

static inline gboolean omx_init (GstOmxBaseSink *self);

enum
{
    ARG_0,
    ARG_COMPONENT_ROLE,
    ARG_COMPONENT_NAME,
    ARG_LIBRARY_NAME,
};

static void init_interfaces (GType type);
GSTOMX_BOILERPLATE_FULL (GstOmxBaseSink, gst_omx_base_sink, GstBaseSink, GST_TYPE_BASE_SINK, init_interfaces);

static void
setup_ports (GstOmxBaseSink *self)
{
    OMX_PARAM_PORTDEFINITIONTYPE param;

    /* Input port configuration. */
    self->in_port = g_omx_core_get_port (self->gomx, "in", 0);
    G_OMX_PORT_GET_DEFINITION (self->in_port, &param);
    g_omx_port_setup (self->in_port, &param);
    gst_pad_set_element_private (self->sinkpad, self->in_port);
}

static void
setup_input_buffer (GstOmxBaseSink *self, GstBuffer *buf)
{
    if (GST_IS_OMXBUFFERTRANSPORT (buf))
    {
        OMX_PARAM_PORTDEFINITIONTYPE param;
        GOmxPort *port, *in_port;
        gint i;

        /* retrieve incoming buffer port information */
        port = GST_GET_OMXPORT (buf);

        /* configure input buffer size to match with upstream buffer */
        G_OMX_PORT_GET_DEFINITION (self->in_port, &param);
        param.nBufferSize =  GST_BUFFER_SIZE (buf);
        param.nBufferCountActual = port->num_buffers;
        G_OMX_PORT_SET_DEFINITION (self->in_port, &param);


        /* allocate resource to save the incoming buffer port pBuffer pointer in
         * OmxBufferInfo structure.
         */
        in_port =  self->in_port;
        in_port->share_buffer_info = malloc (sizeof(OmxBufferInfo));
        in_port->share_buffer_info->pBuffer = malloc (sizeof(int) * port->num_buffers);
        for (i=0; i < port->num_buffers; i++) {
            in_port->share_buffer_info->pBuffer[i] = port->buffers[i]->pBuffer;
        }

        /* disable omx_allocate alloc flag, so that we can fall back to shared method */
        self->in_port->omx_allocate = FALSE;
        self->in_port->always_copy = FALSE;
        self->in_port->share_buffer = FALSE;
    }
    else
    {
        /* ask openmax to allocate input buffer */
        self->in_port->omx_allocate = TRUE;
        self->in_port->always_copy = TRUE;
        self->in_port->share_buffer = FALSE;
    }
}

static GstStateChangeReturn
change_state (GstElement *element,
              GstStateChange transition)
{
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
    GstOmxBaseSink *self;

    self = GST_OMX_BASE_SINK (element);

    GST_LOG_OBJECT (self, "begin");

    GST_INFO_OBJECT (self, "changing state %s - %s",
                     gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
                     gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

    switch (transition)
    {
        case GST_STATE_CHANGE_NULL_TO_READY:
            if (!self->initialized)
            {
                if (!omx_init (self))
                    return GST_PAD_LINK_REFUSED;

                self->initialized = TRUE;
            }

            if (self->port_initialized)
                g_omx_core_prepare (self->gomx);
            break;

        case GST_STATE_CHANGE_READY_TO_PAUSED:
            if (self->port_initialized)
                g_omx_core_start (self->gomx);
            break;

        case GST_STATE_CHANGE_PAUSED_TO_READY:
            g_omx_port_finish (self->in_port);
            break;

        default:
            break;
    }

    ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

    if (ret == GST_STATE_CHANGE_FAILURE)
        goto leave;

    switch (transition)
    {
        case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
            g_omx_port_pause (self->in_port);
            break;

        case GST_STATE_CHANGE_PAUSED_TO_READY:
            g_omx_core_stop (self->gomx);
            break;

        case GST_STATE_CHANGE_READY_TO_NULL:
            g_omx_core_unload (self->gomx);
            break;

        default:
            break;
    }

leave:
    GST_LOG_OBJECT (self, "end");

    return ret;
}

static void
finalize (GObject *obj)
{
    GstOmxBaseSink *self;

    self = GST_OMX_BASE_SINK (obj);

    g_omx_core_free (self->gomx);

    g_free (self->omx_role);
    g_free (self->omx_component);
    g_free (self->omx_library);

    G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static GstFlowReturn
render (GstBaseSink *gst_base,
        GstBuffer *buf)
{
    GOmxCore *gomx;
    GOmxPort *in_port;
    GstOmxBaseSink *self;
    GstFlowReturn ret = GST_FLOW_OK;

    self = GST_OMX_BASE_SINK (gst_base);

    gomx = self->gomx;

    GST_LOG_OBJECT (self, "begin");
    //PRINT_BUFFER (self, buf);

    GST_LOG_OBJECT (self, "state: %d", gomx->omx_state);

    if (G_UNLIKELY (gomx->omx_state == OMX_StateLoaded))
    {
        setup_input_buffer (self, buf);
        g_omx_core_prepare (self->gomx);
        g_omx_core_start (self->gomx);
    }
    
    in_port = self->in_port;

    if (G_LIKELY (in_port->enabled))
    {
        while (TRUE)
        {
            gint sent = g_omx_port_send (in_port, buf);

            if (G_UNLIKELY (sent < 0))
            {
                ret = GST_FLOW_UNEXPECTED;
                break;
            }
            else if (sent < GST_BUFFER_SIZE (buf))
            {
                GstBuffer *subbuf = gst_buffer_create_sub (buf, sent,
                        GST_BUFFER_SIZE (buf) - sent);
                gst_buffer_unref (buf);
                buf = subbuf;
            }
            else
            {
                break;
            }
        }
    }
    else
    {
        GST_WARNING_OBJECT (self, "done");
        ret = GST_FLOW_UNEXPECTED;
    }

    GST_LOG_OBJECT (self, "end");

    return ret;
}

static gboolean
handle_event (GstBaseSink *gst_base,
              GstEvent *event)
{
    GstOmxBaseSink *self;
    GOmxCore *gomx;
    GOmxPort *in_port;

    self = GST_OMX_BASE_SINK (gst_base);
    gomx = self->gomx;
    in_port = self->in_port;

    GST_LOG_OBJECT (self, "begin");

    GST_DEBUG_OBJECT (self, "event: %s", GST_EVENT_TYPE_NAME (event));

    switch (GST_EVENT_TYPE (event))
    {
        case GST_EVENT_EOS:
            /* Close the inpurt port. */
            g_omx_core_set_done (gomx);
            break;

        case GST_EVENT_FLUSH_START:
            /* unlock loops */
            g_omx_port_pause (in_port);

            /* flush all buffers */
            OMX_SendCommand (gomx->omx_handle, OMX_CommandFlush, OMX_ALL, NULL);
            break;

        case GST_EVENT_FLUSH_STOP:
            g_sem_down (gomx->flush_sem);

            g_omx_port_resume (in_port);
            break;

        default:
            break;
    }

    GST_LOG_OBJECT (self, "end");

    return TRUE;
}

#if 0
static GstFlowReturn
buffer_alloc(GstBaseSink *base, guint64 offset, guint size, GstCaps *caps, 
    GstBuffer **buf)
{

    GstOmxBaseSink *self;
    OMX_BUFFERHEADERTYPE *omx_buffer;
    int i;
    static int count = 0;

    self = GST_OMX_BASE_SINK (base);

    if (!self->port_initialized) 
    {
        self->omx_setup (base, caps);
    }

    if (G_UNLIKELY (self->gomx->omx_state == OMX_StateLoaded))
    {
        self->in_port->omx_allocate = TRUE;
        self->in_port->always_copy = FALSE;
        self->in_port->share_buffer = FALSE;
        g_omx_core_prepare (self->gomx);
        g_omx_core_start (self->gomx);
        
        /* queue all the buffers */
        for (i=0; i < self->in_port->num_buffers; i++)
            g_omx_port_push_buffer (self->in_port, self->in_port->buffers[i]);
    }

    omx_buffer = async_queue_pop (self->in_port->queue);
    omx_buffer->nFilledLen = size;
    *buf = gst_omxbuffertransport_new (self->in_port, omx_buffer);

    return GST_FLOW_OK;
}
#endif

static void
set_property (GObject *obj,
              guint prop_id,
              const GValue *value,
              GParamSpec *pspec)
{
    GstOmxBaseSink *self;

    self = GST_OMX_BASE_SINK (obj);

    switch (prop_id)
    {
        case ARG_COMPONENT_ROLE:
            g_free (self->omx_role);
            self->omx_role = g_value_dup_string (value);
            break;
        case ARG_COMPONENT_NAME:
            g_free (self->omx_component);
            self->omx_component = g_value_dup_string (value);
            break;
        case ARG_LIBRARY_NAME:
            g_free (self->omx_library);
            self->omx_library = g_value_dup_string (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
            break;
    }
}

static void
get_property (GObject *obj,
              guint prop_id,
              GValue *value,
              GParamSpec *pspec)
{
    GstOmxBaseSink *self;

    self = GST_OMX_BASE_SINK (obj);

    switch (prop_id)
    {
        case ARG_COMPONENT_ROLE:
            g_value_set_string (value, self->omx_role);
            break;
        case ARG_COMPONENT_NAME:
            g_value_set_string (value, self->omx_component);
            break;
        case ARG_LIBRARY_NAME:
            g_value_set_string (value, self->omx_library);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
            break;
    }
}

static void
type_base_init (gpointer g_class)
{
}

static void
type_class_init (gpointer g_class,
                 gpointer class_data)
{
    GObjectClass *gobject_class;
    GstBaseSinkClass *gst_base_sink_class;
    GstElementClass *gstelement_class;

    gobject_class = G_OBJECT_CLASS (g_class);
    gst_base_sink_class = GST_BASE_SINK_CLASS (g_class);
    gstelement_class = GST_ELEMENT_CLASS (g_class);

    gobject_class->finalize = finalize;

    gstelement_class->change_state = change_state;

    gst_base_sink_class->event = handle_event;
    gst_base_sink_class->preroll = NULL;
    gst_base_sink_class->render = render;
//    gst_base_sink_class->buffer_alloc = buffer_alloc;

    /* Properties stuff */
    {
        gobject_class->set_property = set_property;
        gobject_class->get_property = get_property;

        g_object_class_install_property (gobject_class, ARG_COMPONENT_ROLE,
                                         g_param_spec_string ("component-role", "Component role",
                                                              "Role of the OpenMAX IL component",
                                                              NULL, G_PARAM_READWRITE));

        g_object_class_install_property (gobject_class, ARG_COMPONENT_NAME,
                                         g_param_spec_string ("component-name", "Component name",
                                                              "Name of the OpenMAX IL component to use",
                                                              NULL, G_PARAM_READWRITE));

        g_object_class_install_property (gobject_class, ARG_LIBRARY_NAME,
                                         g_param_spec_string ("library-name", "Library name",
                                                              "Name of the OpenMAX IL implementation library to use",
                                                              NULL, G_PARAM_READWRITE));
    }
}

static gboolean
activate_push (GstPad *pad,
               gboolean active)
{
    gboolean result = TRUE;
    GstOmxBaseSink *self;

    self = GST_OMX_BASE_SINK (gst_pad_get_parent (pad));

    if (active)
    {
        GST_DEBUG_OBJECT (self, "activate");

        /* we do not start the task yet if the pad is not connected */
        if (gst_pad_is_linked (pad))
        {
            /** @todo link callback function also needed */
            g_omx_port_resume (self->in_port);
        }
    }
    else
    {
        GST_DEBUG_OBJECT (self, "deactivate");

        /** @todo disable this until we properly reinitialize the buffers. */
#if 0
        /* flush all buffers */
        OMX_SendCommand (self->gomx->omx_handle, OMX_CommandFlush, OMX_ALL, NULL);
#endif

        /* unlock loops */
        g_omx_port_pause (self->in_port);
    }

    gst_object_unref (self);

    if (result)
        result = self->base_activatepush (pad, active);

    return result;
}

static inline gboolean
omx_init (GstOmxBaseSink *self)
{
    g_omx_core_init (self->gomx);

    if (self->gomx->omx_error)
        return FALSE;

    setup_ports (self);

    return TRUE;
}

static GstPadLinkReturn
pad_sink_link (GstPad *pad,
               GstPad *peer)
{
    GOmxCore *gomx;
    GstOmxBaseSink *self;

    self = GST_OMX_BASE_SINK (GST_OBJECT_PARENT (pad));

    GST_INFO_OBJECT (self, "link");

    gomx = self->gomx;

    if (!self->initialized)
    {
        if (!omx_init (self))
            return GST_PAD_LINK_REFUSED;
        self->initialized = TRUE;
    }

    return GST_PAD_LINK_OK;
}

static void
type_instance_init (GTypeInstance *instance,
                    gpointer g_class)
{
    GstOmxBaseSink *self;

    self = GST_OMX_BASE_SINK (instance);

    GST_LOG_OBJECT (self, "begin");

    /* GOmx */
    self->gomx = g_omx_core_new (self, g_class);

    {
        GstPad *sinkpad;
        self->sinkpad = sinkpad = GST_BASE_SINK_PAD (self);
        self->base_activatepush = GST_PAD_ACTIVATEPUSHFUNC (sinkpad);
        gst_pad_set_activatepush_function (sinkpad, activate_push);
        gst_pad_set_link_function (sinkpad, pad_sink_link);
    }

    GST_LOG_OBJECT (self, "end");
}

static void
omx_interface_init (GstImplementsInterfaceClass *klass)
{
}

static gboolean
interface_supported (GstImplementsInterface *iface,
                     GType type)
{
    g_assert (type == GST_TYPE_OMX);
    return TRUE;
}

static void
interface_init (GstImplementsInterfaceClass *klass)
{
    klass->supported = interface_supported;
}
static void
init_interfaces (GType type)
{
    GInterfaceInfo *iface_info;
    GInterfaceInfo *omx_info;

    iface_info = g_new0 (GInterfaceInfo, 1);
    iface_info->interface_init = (GInterfaceInitFunc) interface_init;

    g_type_add_interface_static (type, GST_TYPE_IMPLEMENTS_INTERFACE, iface_info);
    g_free (iface_info);

    omx_info = g_new0 (GInterfaceInfo, 1);
    omx_info->interface_init = (GInterfaceInitFunc) omx_interface_init;

    g_type_add_interface_static (type, GST_TYPE_OMX, omx_info);
    g_free (omx_info);
}
