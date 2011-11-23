/*
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

#include "gstomx_base_src.h"
#include "gstomx.h"

#include <string.h> /* for memset, memcpy */

enum
{
    ARG_0,
    ARG_COMPONENT_ROLE,
    ARG_COMPONENT_NAME,
    ARG_LIBRARY_NAME,
    ARG_NUM_OUTPUT_BUFFERS,
};

GSTOMX_BOILERPLATE (GstOmxBaseSrc, gst_omx_base_src, GstBaseSrc, GST_TYPE_BASE_SRC);

void
gst_omx_base_src_setup_ports (GstOmxBaseSrc *self)
{
    OMX_PARAM_PORTDEFINITIONTYPE param;

    /* Output port configuration. */
    G_OMX_PORT_GET_DEFINITION (self->out_port, &param);
    g_omx_port_setup (self->out_port, &param);

    if (self->setup_ports)
    {
        self->setup_ports (self);
    }
}

static gboolean
start (GstBaseSrc *gst_base)
{
    GstOmxBaseSrc *self;

    self = GST_OMX_BASE_SRC (gst_base);

    GST_LOG_OBJECT (self, "begin");

    g_omx_core_init (self->gomx);
    if (self->gomx->omx_error)
        return GST_STATE_CHANGE_FAILURE;

    GST_LOG_OBJECT (self, "end");

    return TRUE;
}

static gboolean
stop (GstBaseSrc *gst_base)
{
    GstOmxBaseSrc *self;

    self = GST_OMX_BASE_SRC (gst_base);

    GST_LOG_OBJECT (self, "begin");

    g_omx_core_stop (self->gomx);
    g_omx_core_unload (self->gomx);
    g_omx_core_deinit (self->gomx);

    if (self->gomx->omx_error)
        return GST_STATE_CHANGE_FAILURE;

    GST_LOG_OBJECT (self, "end");

    return TRUE;
}

static void
finalize (GObject *obj)
{
    GstOmxBaseSrc *self;

    self = GST_OMX_BASE_SRC (obj);

    g_omx_core_free (self->gomx);

    g_free (self->omx_role);
    g_free (self->omx_component);
    g_free (self->omx_library);

    G_OBJECT_CLASS (parent_class)->finalize (obj);
}

/* protected helper method which can be used by derived classes:
 */
GstFlowReturn
gst_omx_base_src_create_from_port (GstOmxBaseSrc *self,
        GOmxPort *out_port,
        GstBuffer **ret_buf)
{
    GOmxCore *gomx;
    GstFlowReturn ret = GST_FLOW_OK;

    gomx = self->gomx;

    GST_LOG_OBJECT (self, "begin");

    if (out_port->enabled)
    {
        if (G_UNLIKELY (gomx->omx_state == OMX_StateIdle))
        {
            GST_INFO_OBJECT (self, "omx: play");
            g_omx_core_start (gomx);
        }

        if (G_UNLIKELY (gomx->omx_state != OMX_StateExecuting))
        {
            GST_ERROR_OBJECT (self, "Whoa! very wrong");
            ret = GST_FLOW_ERROR;
            goto beach;
        }

        while (out_port->enabled)
        {
            gpointer obj = g_omx_port_recv (out_port);

            if (G_UNLIKELY (!obj))
            {
                ret = GST_FLOW_ERROR;
                break;
            }

            if (G_LIKELY (GST_IS_BUFFER (obj)))
            {
                GstBuffer *buf = GST_BUFFER (obj);
                if (G_LIKELY (GST_BUFFER_SIZE (buf) > 0))
                {
                    PRINT_BUFFER (self, buf);
                    *ret_buf = buf;
                    break;
                }
            }
            else if (GST_IS_EVENT (obj))
            {
                GST_INFO_OBJECT (self, "got eos");
                g_omx_core_set_done (gomx);
                break;
            }
        }
    }

    if (!out_port->enabled)
    {
        GST_WARNING_OBJECT (self, "done");
        ret = GST_FLOW_UNEXPECTED;
    }

beach:

    GST_LOG_OBJECT (self, "end");

    return ret;
}

static GstFlowReturn
create (GstBaseSrc *gst_base,
        guint64 offset,
        guint length,
        GstBuffer **ret_buf)
{
    GstOmxBaseSrc *self = GST_OMX_BASE_SRC (gst_base);

    GST_LOG_OBJECT (self, "state: %d", self->gomx->omx_state);

    if (self->gomx->omx_state == OMX_StateLoaded)
    {
        GST_INFO_OBJECT (self, "omx: prepare");

        gst_omx_base_src_setup_ports (self);
        g_omx_core_prepare (self->gomx);
    }

    return gst_omx_base_src_create_from_port (self, self->out_port, ret_buf);
}

static gboolean
handle_event (GstBaseSrc *gst_base,
              GstEvent *event)
{
    GstOmxBaseSrc *self;

    self = GST_OMX_BASE_SRC (gst_base);

    GST_LOG_OBJECT (self, "begin");

    GST_DEBUG_OBJECT (self, "event: %s", GST_EVENT_TYPE_NAME (event));

    switch (GST_EVENT_TYPE (event))
    {
        case GST_EVENT_EOS:
            /* Close the output port. */
            g_omx_core_set_done (self->gomx);
            break;

        case GST_EVENT_NEWSEGMENT:
            break;

        default:
            break;
    }

    GST_LOG_OBJECT (self, "end");

    return TRUE;
}

static void
set_property (GObject *obj,
              guint prop_id,
              const GValue *value,
              GParamSpec *pspec)
{
    GstOmxBaseSrc *self;

    self = GST_OMX_BASE_SRC (obj);

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
        case ARG_NUM_OUTPUT_BUFFERS:
            {
                OMX_PARAM_PORTDEFINITIONTYPE param;
                OMX_U32 nBufferCountActual = g_value_get_uint (value);

                G_OMX_PORT_GET_DEFINITION (self->out_port, &param);

                g_return_if_fail (nBufferCountActual >= param.nBufferCountMin);
                param.nBufferCountActual = nBufferCountActual;

                G_OMX_PORT_SET_DEFINITION (self->out_port, &param);
            }
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
    GstOmxBaseSrc *self;

    self = GST_OMX_BASE_SRC (obj);

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
        case ARG_NUM_OUTPUT_BUFFERS:
            {
                OMX_PARAM_PORTDEFINITIONTYPE param;
                G_OMX_PORT_GET_DEFINITION (self->out_port, &param);
                g_value_set_uint (value, param.nBufferCountActual);
            }
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
    GstBaseSrcClass *gst_base_src_class;
    GstOmxBaseSrcClass *omx_base_class;

    gobject_class = G_OBJECT_CLASS (g_class);
    gst_base_src_class = GST_BASE_SRC_CLASS (g_class);
    omx_base_class = GST_OMX_BASE_SRC_CLASS (g_class);

    gobject_class->finalize = finalize;

    gst_base_src_class->start = start;
    gst_base_src_class->stop = stop;
    gst_base_src_class->event = handle_event;
    gst_base_src_class->create = create;

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

        /* note: the default values for these are just a guess.. since we wouldn't know
         * until the OMX component is constructed.  But that is ok, these properties are
         * only for debugging
         */
        g_object_class_install_property (gobject_class, ARG_NUM_OUTPUT_BUFFERS,
                                         g_param_spec_uint ("output-buffers", "Output buffers",
                                                            "The number of OMX output buffers",
                                                            1, 10, 4, G_PARAM_READWRITE));
    }

    omx_base_class->out_port_index = 0;
}

/* TODO make a util fxn out of this.. since this code is copied all over
 * the place
 */
void check_settings (GOmxPort *port, GstPad *pad)
{
    /* note: specifically DO NOT check port->enabled!  This can be called
     * during buffer allocation in transition-to-enabled while
     * port->enabled is still FALSE
     */
    GOmxCore *gomx = port->core;
    GstCaps *caps = NULL;

    caps = gst_pad_get_negotiated_caps (pad);

    if (!caps)
    {
        /** @todo We shouldn't be doing this. */
        GST_WARNING_OBJECT (gomx->object, "faking settings changed notification");
        if (gomx->settings_changed_cb)
            gomx->settings_changed_cb (gomx);
    }
    else
    {
        GST_LOG_OBJECT (gomx->object, "caps already fixed: %" GST_PTR_FORMAT, caps);
        gst_caps_unref (caps);
    }
}


/**
 * overrides the default buffer allocation for output port to allow
 * pad_alloc'ing from the srcpad
 */
static GstBuffer *
buffer_alloc (GOmxPort *port, gint len)
{
    GstOmxBaseSrc  *self = port->core->object;
    GstBaseSrc *gst_base = GST_BASE_SRC (self);
    GstBuffer *buf;
    GstFlowReturn ret;

    check_settings (self->out_port, gst_base->srcpad);

    ret = gst_pad_alloc_buffer_and_set_caps (
            gst_base->srcpad, GST_BUFFER_OFFSET_NONE,
            len, GST_PAD_CAPS (gst_base->srcpad), &buf);

    if (ret == GST_FLOW_OK) return buf;

    return NULL;
}


static void
type_instance_init (GTypeInstance *instance,
                    gpointer g_class)
{
    GstOmxBaseSrc *self;
    GstOmxBaseSrcClass *klass;

    self = GST_OMX_BASE_SRC (instance);
    klass = GST_OMX_BASE_SRC_CLASS (g_class);

    GST_LOG_OBJECT (self, "begin");

    /* GOmx */
    self->gomx = g_omx_core_new (self, g_class);
    self->gomx->use_timestamps = FALSE;
    self->gomx->gen_timestamps = FALSE;
    self->gomx->last_buf_timestamp = GST_CLOCK_TIME_NONE;
    self->out_port = g_omx_core_get_port (self->gomx, "out", klass->out_port_index);
    self->out_port->buffer_alloc = buffer_alloc;

    GST_LOG_OBJECT (self, "end");
}
