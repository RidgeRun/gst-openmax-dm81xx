/*
 * Copyright (C) 2013 Vodalys Ingénierie
 *
 * Author: Jean-Baptiste Théou <jean-baptiste.theou@vodalys.com>
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


/*
 * Base filter for 2 inputs and 1 output plugin.
 * */



#include "gstomx_base_filter21.h"
#include "gstomx.h"
#include "gstomx_interface.h"
#include "gstomx_buffertransport.h"

enum
{
    ARG_0,
    ARG_COMPONENT_ROLE,
    ARG_COMPONENT_NAME,
    ARG_LIBRARY_NAME,
    ARG_USE_TIMESTAMPS,
    ARG_NUM_INPUT_BUFFERS,
    ARG_NUM_OUTPUT_BUFFERS,
    ARG_X_SINK_0,
    ARG_Y_SINK_0,
    ARG_X_SINK_1,
    ARG_Y_SINK_1,
	ARG_GEN_TIMESTAMPS
};

static void init_interfaces (GType type);
GSTOMX_BOILERPLATE_FULL (GstOmxBaseFilter21, gst_omx_base_filter21, GstElement, GST_TYPE_ELEMENT, init_interfaces);


static GstFlowReturn push_buffer (GstOmxBaseFilter21 *self, GstBuffer *buf);
static GstFlowReturn pad_chain (GstPad *pad, GstBuffer *buf);
static gboolean pad_event (GstPad *pad, GstEvent *event);


static gint
gstomx_calculate_stride (int width, GstVideoFormat format)
{
    switch (format)
    {
        case GST_VIDEO_FORMAT_NV12:
            return width;
        case GST_VIDEO_FORMAT_YUY2:
            return width * 2;
        default:
            GST_ERROR ("unsupported color format");
    }
    return -1;
}

static gboolean
src_setcaps (GstPad *pad, GstCaps *caps)
{
    GstOmxBaseFilter21 *self;
    GstVideoFormat format;
    GstStructure *structure;

    self = GST_OMX_BASE_FILTER21 (GST_PAD_PARENT (pad));
    structure = gst_caps_get_structure (caps, 0);

    GST_INFO_OBJECT (self, "setcaps (src): %" GST_PTR_FORMAT, caps);
    g_return_val_if_fail (caps, FALSE);
    g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);

    if (!gst_video_format_parse_caps_strided (caps, &format, &self->out_width, &self->out_height, &self->out_stride))
    {
        GST_WARNING_OBJECT (self, "width and/or height is not set in caps");
        return FALSE;
    }

    if (!self->out_stride)
    {
        self->out_stride = gstomx_calculate_stride (self->out_width, format);
    }

    /* save the src caps later needed by omx transport buffer */
    if (self->out_port->caps)
        gst_caps_unref (self->out_port->caps);

    self->out_port->caps = gst_caps_copy (caps);

    return TRUE;
}




static gboolean
sink_setcaps (GstPad *pad,
              GstCaps *caps)
{
    GstStructure *structure;
    GstOmxBaseFilter21 *self;
    GOmxCore *gomx;
    GstVideoFormat format;
    int sink_number;
    self = GST_OMX_BASE_FILTER21 (GST_PAD_PARENT (pad));
	if(strcmp(GST_PAD_NAME(pad), "sink_00") == 0){
		sink_number=0;
	}
	else if(strcmp(GST_PAD_NAME(pad), "sink_01") == 0){
		sink_number=1;
	}
    gomx = (GOmxCore *) self->gomx;
	GST_INFO_OBJECT (self, "setcaps (sink): %d", sink_number);
    GST_INFO_OBJECT (self, "setcaps (sink): %" GST_PTR_FORMAT, caps);
	
    g_return_val_if_fail (caps, FALSE);
    g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);

    structure = gst_caps_get_structure (caps, 0);

    g_return_val_if_fail (structure, FALSE);
    if (!gst_video_format_parse_caps_strided (caps,
            &format, &self->in_width[sink_number], &self->in_height[sink_number], &self->in_stride[sink_number]))
    {
        GST_WARNING_OBJECT (self, "width and/or height is not set in caps");
        return FALSE;
    }

    if (!self->in_stride[sink_number]) 
    {
        self->in_stride[sink_number] = gstomx_calculate_stride (self->in_width[sink_number], format);
    }

    {
        const GValue *framerate = NULL;
        framerate = gst_structure_get_value (structure, "framerate");
        if (framerate)
        {
            self->framerate_num[sink_number] = gst_value_get_fraction_numerator (framerate);
            self->framerate_denom[sink_number] = gst_value_get_fraction_denominator (framerate);

			if (self->framerate_num[sink_number] && self->framerate_denom[sink_number]) {
				self->duration = gst_util_uint64_scale_int(GST_SECOND,
						gst_value_get_fraction_denominator (framerate),
						gst_value_get_fraction_numerator (framerate));
			}
			GST_DEBUG_OBJECT (self, "Nominal frame duration =%"GST_TIME_FORMAT,
					GST_TIME_ARGS (self->duration));
        }
    }

    return gst_pad_set_caps (pad, caps);
}



static void
setup_ports (GstOmxBaseFilter21 *self)
{
    OMX_PARAM_PORTDEFINITIONTYPE param;
	int i;
	gboolean omx_allocate, share_buffer;
	gboolean set_omx_allocate = FALSE, set_share_buffer = FALSE;
	
    if (g_getenv ("OMX_ALLOCATE_ON"))
    {
        GST_DEBUG_OBJECT (self, "OMX_ALLOCATE_ON");
        omx_allocate = TRUE;
        share_buffer = FALSE;
		set_omx_allocate = set_share_buffer = TRUE;
    }
    else if (g_getenv ("OMX_SHARE_HACK_ON"))
    {
        GST_DEBUG_OBJECT (self, "OMX_SHARE_HACK_ON");
        share_buffer = TRUE;
		set_share_buffer = TRUE;
    }
    else if (g_getenv ("OMX_SHARE_HACK_OFF"))
    {
        GST_DEBUG_OBJECT (self, "OMX_SHARE_HACK_OFF");
        share_buffer = FALSE;
		set_share_buffer = TRUE;
    }

    /* Input port configuration. */
    for (i = 0; i < NUM_INPUTS; i++) {
		G_OMX_PORT_GET_DEFINITION (self->in_port[i], &param);
		g_omx_port_setup (self->in_port[i], &param);
		//gst_pad_set_element_private (self->sinkpad[i], self->in_port[i]);
		if (set_omx_allocate) self->in_port[i]->omx_allocate = omx_allocate;
		if (set_share_buffer) self->in_port[i]->share_buffer = share_buffer;
	}
    /* Output port configuration. */
	G_OMX_PORT_GET_DEFINITION (self->out_port, &param);
	g_omx_port_setup (self->out_port, &param);
    gst_pad_set_element_private (self->srcpad, self->out_port);
	if (set_omx_allocate) self->out_port->omx_allocate = omx_allocate;
	if (set_share_buffer) self->out_port->share_buffer = share_buffer;
	
}

static void
setup_input_buffer (GstOmxBaseFilter21 *self, GstBuffer *buf, int sink_num)
{
	int j;
	gint i = 0;
	if (GST_IS_OMXBUFFERTRANSPORT (buf))
	{
		i = sink_num;
		OMX_PARAM_PORTDEFINITIONTYPE param;
		GOmxPort *port, *in_port;
		/* retrieve incoming buffer port information */
		port = GST_GET_OMXPORT (buf);
		/* Check if output port has set always_copy */
		if (port->always_copy != TRUE) {
				/* configure input buffer size to match with upstream buffer */
				G_OMX_PORT_GET_DEFINITION (self->in_port[i], &param);
				param.nBufferSize =  GST_BUFFER_SIZE (buf);
				param.nBufferCountActual = port->num_buffers;
				G_OMX_PORT_SET_DEFINITION (self->in_port[i], &param);

				/* allocate resource to save the incoming buffer port pBuffer pointer in
				* OmxBufferInfo structure.
				*/
			
				in_port =  self->in_port[i];
				in_port->share_buffer_info = malloc (sizeof(OmxBufferInfo));
				if(in_port->share_buffer_info == NULL){
					GST_ERROR_OBJECT (self, "omx: failed to malloc share_buffer_info");
				}
				in_port->share_buffer_info->pBuffer = malloc (sizeof(int) * port->num_buffers);
				if(in_port->share_buffer_info->pBuffer == NULL){
					GST_ERROR_OBJECT (self, "omx: failed to malloc share_buffer_info->pBuffer");
				}
				for (j=0; j < port->num_buffers; j++) {
					in_port->share_buffer_info->pBuffer[j] = port->buffers[j]->pBuffer;
				}
				/* disable omx_allocate alloc flag, so that we can fall back to shared method */
				self->in_port[i]->omx_allocate = FALSE;
				self->in_port[i]->always_copy = FALSE;
			return;
		}
	}
	/* ask openmax to allocate input buffer */
	self->in_port[i]->omx_allocate = TRUE;
	self->in_port[i]->always_copy = TRUE;
	GST_INFO_OBJECT (self, "omx: setup input buffer - end");
}

static GstStateChangeReturn
change_state (GstElement *element,
              GstStateChange transition)
{
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
    GstOmxBaseFilter21 *self;
    GOmxCore *core;
	int i;

    self = GST_OMX_BASE_FILTER21(element);
    core = self->gomx;

	printf("begin: changing state %s -> %s\n",
                     gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
                     gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));
    GST_INFO_OBJECT (self, "begin: changing state %s -> %s",
                     gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
                     gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

    switch (transition)
    {
        case GST_STATE_CHANGE_NULL_TO_READY:
            g_omx_core_init (core);
            if (core->omx_state != OMX_StateLoaded)
            {
                ret = GST_STATE_CHANGE_FAILURE;
                goto leave;
            }
            break;
            
        case GST_STATE_CHANGE_READY_TO_PAUSED:
            gst_collect_pads_start(self->collectpads);
            break;

        case GST_STATE_CHANGE_PAUSED_TO_READY:
            gst_collect_pads_stop(self->collectpads);
            break;

        default:
            break;
    }

    ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

    if (ret == GST_STATE_CHANGE_FAILURE)
        goto leave;

    switch (transition)
    {
        case GST_STATE_CHANGE_PAUSED_TO_READY:
			printf("Paused to ready\n");
            g_mutex_lock (self->ready_lock);
            if (self->ready)
            {
                /* unlock */
                for (i = 0; i < NUM_INPUTS; i++){
					g_omx_port_finish (self->in_port[i]);
               	}
               	g_omx_port_finish (self->out_port);

                g_omx_core_stop (core);
                g_omx_core_unload (core);
                self->ready = FALSE;
            }
            g_mutex_unlock (self->ready_lock);
            if (core->omx_state != OMX_StateLoaded &&
                core->omx_state != OMX_StateInvalid)
            {
                ret = GST_STATE_CHANGE_FAILURE;
                goto leave;
            }
            break;

        case GST_STATE_CHANGE_READY_TO_NULL:
            g_omx_core_deinit (core);
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
    GstOmxBaseFilter21 *self;

    self = GST_OMX_BASE_FILTER21 (obj);

    if (self->codec_data)
    {
        gst_buffer_unref (self->codec_data);
        self->codec_data = NULL;
    }

    g_omx_core_free (self->gomx);

    g_free (self->omx_role);
    g_free (self->omx_component);
    g_free (self->omx_library);

    g_mutex_free (self->ready_lock);

    G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
set_property (GObject *obj,
              guint prop_id,
              const GValue *value,
              GParamSpec *pspec)
{
    GstOmxBaseFilter21 *self;

    self = GST_OMX_BASE_FILTER21(obj);

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
        case ARG_USE_TIMESTAMPS:
            self->gomx->use_timestamps = g_value_get_boolean (value);
            break;
        case ARG_GEN_TIMESTAMPS:
            self->gomx->gen_timestamps = g_value_get_boolean (value);
            break;
        case ARG_X_SINK_0:
			self->x[0] = g_value_get_uint(value);
			break;
		case ARG_Y_SINK_0:
			self->y[0] = g_value_get_uint(value);
			break;
		case ARG_X_SINK_1:
			self->x[1] = g_value_get_uint(value);
			break;
		case ARG_Y_SINK_1:
			self->y[1] = g_value_get_uint(value);
			break;	
        case ARG_NUM_INPUT_BUFFERS:
            {
                OMX_PARAM_PORTDEFINITIONTYPE param;
                OMX_U32 nBufferCountActual = g_value_get_uint (value);
                int i;
                for (i = 0; i < NUM_INPUTS; i++) {
					G_OMX_PORT_GET_DEFINITION (self->in_port[i], &param);
					g_return_if_fail (nBufferCountActual >= param.nBufferCountMin);
					param.nBufferCountActual = nBufferCountActual;
					G_OMX_PORT_SET_DEFINITION (self->in_port[i], &param);
				}
            }
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
    GstOmxBaseFilter21 *self;

    self = GST_OMX_BASE_FILTER21 (obj);

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
        case ARG_USE_TIMESTAMPS:
            g_value_set_boolean (value, self->gomx->use_timestamps);
            break;
        case ARG_GEN_TIMESTAMPS:
            g_value_set_boolean (value, self->gomx->gen_timestamps);
            break;
        case ARG_X_SINK_0:
			g_value_set_uint(value,self->x[0]);
			break;
		case ARG_Y_SINK_0:
			g_value_set_uint(value,self->y[0]);
			break;
		case ARG_X_SINK_1:
			g_value_set_uint(value,self->x[1]);
			break;
		case ARG_Y_SINK_1:
			g_value_set_uint(value,self->y[1]);
			break;	
        case ARG_NUM_INPUT_BUFFERS:
        case ARG_NUM_OUTPUT_BUFFERS:
            {
                OMX_PARAM_PORTDEFINITIONTYPE param;
                GOmxPort *port = (prop_id == ARG_NUM_INPUT_BUFFERS) ?
                        self->in_port[0] : self->out_port;

                G_OMX_PORT_GET_DEFINITION (port, &param);

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
    GstElementClass *gstelement_class;
    GstOmxBaseFilter21Class *bclass;

    gobject_class = G_OBJECT_CLASS (g_class);
    gstelement_class = GST_ELEMENT_CLASS (g_class);
    bclass = GST_OMX_BASE_FILTER21_CLASS (g_class);

    gobject_class->finalize = finalize;
    gstelement_class->change_state = change_state;
    bclass->push_buffer = push_buffer;
    bclass->pad_chain = pad_chain;
    bclass->pad_event = pad_event;

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

        g_object_class_install_property (gobject_class, ARG_USE_TIMESTAMPS,
                                         g_param_spec_boolean ("use-timestamps", "Use timestamps",
                                                               "Whether or not to use timestamps",
                                                               TRUE, G_PARAM_READWRITE));

        g_object_class_install_property (gobject_class, ARG_GEN_TIMESTAMPS,
                                         g_param_spec_boolean ("gen-timestamps", "Generate timestamps",
                                                               "Whether or not to generate timestamps using interpolation/extrapolation",
                                                               TRUE, G_PARAM_READWRITE));

        /* note: the default values for these are just a guess.. since we wouldn't know
         * until the OMX component is constructed.  But that is ok, these properties are
         * only for debugging
         */
        g_object_class_install_property (gobject_class, ARG_X_SINK_0,
                                         g_param_spec_uint ("x-sink0", "X stating coordinate",
                                                            "X stating coordinate for sink_00",
                                                            0, 1920, 0, G_PARAM_READWRITE));
        
        g_object_class_install_property (gobject_class, ARG_Y_SINK_0,
                                         g_param_spec_uint ("y-sink0", "Y stating coordinate",
                                                            "Y stating coordinate for sink_00",
                                                            0, 1280, 0, G_PARAM_READWRITE));
                                                            
         g_object_class_install_property (gobject_class, ARG_X_SINK_1,
                                         g_param_spec_uint ("x-sink1", "X stating coordinate",
                                                            "X stating coordinate for sink_01",
                                                            0, 1920, 0, G_PARAM_READWRITE));
        
        g_object_class_install_property (gobject_class, ARG_Y_SINK_1,
                                         g_param_spec_uint ("y-sink1", "Y stating coordinate",
                                                            "Y stating coordinate for sink_01",
                                                            0, 1280, 0, G_PARAM_READWRITE));
         
        g_object_class_install_property (gobject_class, ARG_NUM_INPUT_BUFFERS,
                                         g_param_spec_uint ("input-buffers", "Input buffers",
                                                            "The number of OMX input buffers",
                                                            1, 10, 4, G_PARAM_READWRITE));
        g_object_class_install_property (gobject_class, ARG_NUM_OUTPUT_BUFFERS,
                                         g_param_spec_uint ("output-buffers", "Output buffers",
                                                            "The number of OMX output buffers",
                                                            1, 10, 4, G_PARAM_READWRITE));
    }
}

static GstFlowReturn
push_buffer (GstOmxBaseFilter21 *self,
             GstBuffer *buf)
{
    GstFlowReturn ret = GST_FLOW_ERROR;
	int i;

	if(GST_GET_OMXPORT(buf) != self->out_port){
		return ret;
	}
    GST_BUFFER_DURATION (buf) = self->duration;
	GST_BUFFER_TIMESTAMP(buf) = self->sink_camera_timestamp;
	GST_DEBUG_OBJECT(self, "timestamp=%" GST_TIME_FORMAT, GST_TIME_ARGS(self->sink_camera_timestamp));

	self->sink_camera_timestamp += self->duration;
	
    PRINT_BUFFER (self, buf);
    if (self->push_cb) {
        if (FALSE == self->push_cb (self, buf)) { gst_buffer_unref(buf); return GST_FLOW_OK; }
	}

	/** @todo check if tainted */
	ret = gst_pad_push (self->srcpad, buf);

    return ret;
}

static void
output_loop (gpointer data)
{
    GstPad *pad;
    GOmxCore *gomx;
    GOmxPort *out_port;
    GstOmxBaseFilter21 *self;
    GstFlowReturn ret = GST_FLOW_OK;
    GstOmxBaseFilter21Class *bclass;
    
    pad = data;
    self = GST_OMX_BASE_FILTER21 (gst_pad_get_parent (pad));
    gomx = self->gomx;

    bclass = GST_OMX_BASE_FILTER21_GET_CLASS (self);

    if (!self->ready)
    {
        g_error ("not ready");
        return;
    }

    out_port = (GOmxPort *)gst_pad_get_element_private(pad);

    if (G_LIKELY (out_port->enabled))
    {
        gpointer obj = g_omx_port_recv (out_port);
        if (G_UNLIKELY (!obj))
        {
            GST_WARNING_OBJECT (self, "null buffer: leaving");
            ret = GST_FLOW_WRONG_STATE;
            goto leave;
        }

        if (G_LIKELY (GST_IS_BUFFER (obj)))
        {
            if (G_UNLIKELY (GST_BUFFER_FLAG_IS_SET (obj, GST_BUFFER_FLAG_IN_CAPS)))
            {
                GstCaps *caps = NULL;
                GstStructure *structure;
                GValue value = { 0 };

                caps = gst_pad_get_negotiated_caps (pad);
                caps = gst_caps_make_writable (caps);
                structure = gst_caps_get_structure (caps, 0);

                g_value_init (&value, GST_TYPE_BUFFER);
                gst_value_set_buffer (&value, obj);
                gst_buffer_unref (obj);
                gst_structure_set_value (structure, "codec_data", &value);
                g_value_unset (&value);

                gst_pad_set_caps (pad, caps);
            }
            else
            {
                GstBuffer *buf = GST_BUFFER (obj);
                ret = bclass->push_buffer (self, buf);
                GST_DEBUG_OBJECT (self, "ret=%s", gst_flow_get_name (ret));
				// HACK!! Dont care if one of the output pads are not connected
                ret = GST_FLOW_OK;
            }
        }
        else if (GST_IS_EVENT (obj))
        {
            GST_INFO_OBJECT (self, "got eos");
            gst_pad_push_event (pad, obj);
            ret = GST_FLOW_UNEXPECTED;
            goto leave;
        }

    }

leave:

    self->last_pad_push_return = ret;

    if (gomx->omx_error != OMX_ErrorNone)
    {
        GST_INFO_OBJECT (self, "omx_error=%s", g_omx_error_to_str (gomx->omx_error));
        ret = GST_FLOW_ERROR;
    }

    if (ret != GST_FLOW_OK)
    {
        GST_INFO_OBJECT (self, "pause task, reason:  %s",
                         gst_flow_get_name (ret));
        gst_pad_pause_task (pad);
    }

    gst_object_unref (self);
    
}

static GstFlowReturn collected_pads(GstCollectPads *pads, GstOmxBaseFilter21 *self)
{
    GSList *item;
    GstCollectData *collectdata;
    GstFlowReturn ret = GST_FLOW_OK;
    GOmxCore *gomx = self->gomx;
    gint sink_number;
    GstBuffer *buffers[2];

    GST_DEBUG_OBJECT(self, "Collected pads !");

    // Collect buffers
    for( item = pads->data ; item != NULL ; item = item->next ) {
        collectdata = (GstCollectData *) item->data;
        
        //FIXME Use collect data
        if(strcmp(GST_PAD_NAME(collectdata->pad), "sink_00") == 0){
    		sink_number=0;
	    }
	    else{
		    sink_number=1;
	    }
	    
	    buffers[sink_number] = gst_collect_pads_pop(pads, collectdata);
    }

    // Detect EOS
    if( buffers[0] == NULL ) {
        GST_INFO_OBJECT(self, "EOS");
        gst_pad_push_event(self->srcpad, gst_event_new_eos());
        return GST_FLOW_UNEXPECTED;
    }

    // Setup input ports if not done yet
    if (G_LIKELY (gomx->omx_state != OMX_StateExecuting)) {
      for( sink_number=0 ; sink_number<2 ; sink_number++ ) {
        GST_INFO_OBJECT(self, "Setup port %d", sink_number);
        setup_input_buffer (self, buffers[sink_number], sink_number);
      }
    }
    
    // Call chain foreach buffer
    for( sink_number=0 ; sink_number<2 ; sink_number++ ) {
        ret = pad_chain(self->sinkpad[sink_number], buffers[sink_number]);
    }
    
    // Call output_loop after pad_chain
    output_loop(self->srcpad);
    
    return ret;
}

static GstFlowReturn
pad_chain (GstPad *pad,
           GstBuffer *buf)
{
    GOmxCore *gomx;
    GOmxPort *in_port;
    GstOmxBaseFilter21 *self;
    GstFlowReturn ret = GST_FLOW_OK;
	int i;
	static sink_init = 0;
	int sink_number;
	static gboolean init_done = FALSE;
	
    self = GST_OMX_BASE_FILTER21 (GST_OBJECT_PARENT (pad));
	if(strcmp(GST_PAD_NAME(pad), "sink_00") == 0){
		sink_number=0;
		if( self->sink_camera_timestamp == GST_CLOCK_TIME_NONE ) {
            self->sink_camera_timestamp = GST_BUFFER_TIMESTAMP(buf);
        	GST_INFO_OBJECT(self, "init timestamp=%" GST_TIME_FORMAT, GST_TIME_ARGS(self->sink_camera_timestamp));
        }
	}
	else if(strcmp(GST_PAD_NAME(pad), "sink_01") == 0){
		sink_number=1;
	}
    PRINT_BUFFER (self, buf);

    gomx = self->gomx;

    GST_LOG_OBJECT (self, "begin: size=%u, state=%d, sink_number=%d", GST_BUFFER_SIZE (buf), gomx->omx_state, sink_number);
	
	/*if (G_LIKELY (gomx->omx_state != OMX_StateExecuting))
    {
		GST_INFO_OBJECT (self, "Begin - Port %d", sink_number);
		//setup_input_buffer (self, buf, sink_number);
		//sink_init++;
		//g_mutex_lock (self->ready_lock);
		if(init_done == TRUE){
			GST_INFO_OBJECT (self, "Init_done");
			//g_mutex_unlock(self->ready_lock);
		}
		if(init_done == TRUE){
			sink_init = 0;
			init_done = FALSE;

		}
		else{
			while(sink_init != 2){
				usleep(1000);
			}
		}
	}*/
	
    if (G_UNLIKELY (gomx->omx_state == OMX_StateLoaded))
    {

        GST_INFO_OBJECT (self, "omx: prepare");
        
        /** @todo this should probably go after doing preparations. */
        if (self->omx_setup)
        {
            self->omx_setup (self);
        }
        
        /* enable input port */
        for(i=0;i<NUM_INPUTS;i++){
			GST_INFO_OBJECT (self,"Enable Port %d",self->in_port[i]->port_index);
			OMX_SendCommand (gomx->omx_handle, OMX_CommandPortEnable, self->in_port[i]->port_index, NULL);
			g_sem_down (self->in_port[i]->core->port_sem);
		}
		GST_INFO_OBJECT (self,"Enable Port %d",self->out_port->port_index);
		/* enable output port */
		OMX_SendCommand (gomx->omx_handle,OMX_CommandPortEnable, self->out_port->port_index, NULL);
		g_sem_down (self->out_port->core->port_sem);

		/* indicate that port is now configured */

        setup_ports (self);

        g_omx_core_prepare (self->gomx);

        if (gomx->omx_state == OMX_StateIdle)
        {
            self->ready = TRUE;
           	//gst_pad_start_task (self->srcpad, output_loop, self->srcpad);
        }
        
        if (gomx->omx_state != OMX_StateIdle)
            goto out_flushing;
        
        GST_INFO_OBJECT (self, "omx: end state Loaded");    
    }
    
    if (G_UNLIKELY (gomx->omx_state == OMX_StateIdle))
	{
		g_omx_core_start (gomx);
		GST_INFO_OBJECT (self, "Release Port - %d", sink_number);
		init_done = TRUE;
		//g_mutex_unlock (self->ready_lock);
		if (gomx->omx_state != OMX_StateExecuting){
			GST_INFO_OBJECT (self, "omx: executing FAILED !");
			goto out_flushing;
		
		}
	}
	
	if (G_LIKELY (self->in_port[sink_number]->enabled))
	{	      
		

		if (G_UNLIKELY (gomx->omx_state != OMX_StateExecuting))
		{
			GST_ERROR_OBJECT (self, "Whoa! very wrong");
		}

		while (TRUE)
		{
			gint sent;
			if (self->last_pad_push_return != GST_FLOW_OK ||
				!(gomx->omx_state == OMX_StateExecuting ||
				gomx->omx_state == OMX_StatePause))
			{
				GST_INFO_OBJECT (self, "last_pad_push_return=%d", self->last_pad_push_return);
				goto out_flushing;
			}
			sent = g_omx_port_send (self->in_port[sink_number], buf);
			if (G_UNLIKELY (sent < 0))
			{
				ret = GST_FLOW_WRONG_STATE;
				goto out_flushing;
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
				gst_buffer_unref (buf);

				break;
			}
			
		}
	}
	else
	{
		GST_WARNING_OBJECT (self, "done");
		ret = GST_FLOW_UNEXPECTED;
	}
	return ret;
leave:

    GST_LOG_OBJECT (self, "end");

    return ret;

    /* special conditions */
out_flushing:
    {
        const gchar *error_msg = NULL;

        if (gomx->omx_error)
        {
            error_msg = "Error from OpenMAX component";
        }
        else if (gomx->omx_state != OMX_StateExecuting &&
                 gomx->omx_state != OMX_StatePause)
        {
            error_msg = "OpenMAX component in wrong state";
        }

        if (error_msg)
        {
            GST_ELEMENT_ERROR (self, STREAM, FAILED, (NULL), (error_msg));
            ret = GST_FLOW_ERROR;
        }

        gst_buffer_unref (buf);

        goto leave;
    }
}

static gboolean
pad_event (GstPad *pad,
           GstEvent *event)
{
    GstOmxBaseFilter21 *self;
    GOmxCore *gomx;
    gboolean ret = TRUE;
	int i;
    self = GST_OMX_BASE_FILTER21 (GST_OBJECT_PARENT (pad));
    gomx = self->gomx;

    GST_INFO_OBJECT (self, "begin: event=%s", GST_EVENT_TYPE_NAME (event));

    switch (GST_EVENT_TYPE (event))
    {
        case GST_EVENT_EOS:
            /* printf ("Recieved EOS event, press <CTRL+C> to terminate pipeline.\n"); */
	    g_mutex_lock (self->ready_lock);
            self->number_eos--;
            printf("EOS : %d\n",self->number_eos);
			g_mutex_unlock (self->ready_lock);
            if(self->number_eos == 0){
              ret = gst_pad_push_event (self->srcpad, event);
            } 
            else{
              gst_event_unref (event);
            }
            break;

        case GST_EVENT_FLUSH_START:
			gst_event_ref(event);
			ret &= gst_pad_push_event (self->srcpad, event);
            self->last_pad_push_return = GST_FLOW_WRONG_STATE;
            g_omx_core_flush_start (gomx);
	        gst_pad_pause_task (self->srcpad);
            ret = TRUE;
            break;

        case GST_EVENT_FLUSH_STOP:
			gst_event_ref(event);
			ret &= gst_pad_push_event (self->srcpad, event);
            self->last_pad_push_return = GST_FLOW_OK;

            g_omx_core_flush_stop (gomx);

            /*if (self->ready)
               	gst_pad_start_task (self->srcpad, output_loop, self->srcpad);*/

            ret = TRUE;
            break;

        case GST_EVENT_NEWSEGMENT:
			self->last_buf_timestamp = GST_CLOCK_TIME_NONE;
			gst_event_ref(event);
			ret &= gst_pad_push_event (self->srcpad, event);
            break;

        default:
			gst_event_ref(event);
			ret &= gst_pad_push_event (self->srcpad, event);
            break;
    }

    GST_LOG_OBJECT (self, "end");

    return ret;
}

static gboolean
activate_push (GstPad *pad,
               gboolean active)
{
    gboolean result = TRUE;
    GstOmxBaseFilter21 *self;
	int i;

    self = GST_OMX_BASE_FILTER21 (gst_pad_get_parent (pad));

    if (active)
    {
        GST_DEBUG_OBJECT (self, "activate");
        self->last_pad_push_return = GST_FLOW_OK;

        /* we do not start the task yet if the pad is not connected */
        if (gst_pad_is_linked (pad))
        {
            if (self->ready)
            {
                /** @todo link callback function also needed */
				for (i = 0; i < NUM_INPUTS; i++)
					g_omx_port_resume (self->in_port[i]);
					
               	g_omx_port_resume (self->out_port);

                //result = gst_pad_start_task (pad, output_loop, pad);
            }
        }
    }
    else
    {
        GST_DEBUG_OBJECT (self, "deactivate");

        if (self->ready)
        {

            /* unlock loops */
			for (i = 0; i < NUM_INPUTS; i++)
				g_omx_port_pause (self->in_port[i]);
				
           	g_omx_port_pause (self->out_port);
        }

        /* make sure streaming finishes */
        result = gst_pad_stop_task (pad);
    }

    gst_object_unref (self);

    return result;
}

/**
 * overrides the default buffer allocation for output port to allow
 * pad_alloc'ing from the srcpad
 */
static GstBuffer *
buffer_alloc (GOmxPort *port, gint len)
{
    GstOmxBaseFilter21 *self = port->core->object;
    GstBuffer *buf;
    GstFlowReturn ret;
	int i;

	if(port != self->out_port){
			return NULL;
	}

#if 1
    /** @todo remove this check */
    if (G_LIKELY (self->in_port[0]->enabled))
    {
        GstCaps *caps = NULL;

        caps = gst_pad_get_negotiated_caps (self->srcpad);

        if (!caps)
        {
            /** @todo We shouldn't be doing this. */
            GOmxCore *gomx = self->gomx;
            GST_WARNING_OBJECT (self, "faking settings changed notification");
            if (gomx->settings_changed_cb)
                gomx->settings_changed_cb (gomx);
        }
        else
        {
            GST_LOG_OBJECT (self, "caps already fixed: %" GST_PTR_FORMAT, caps);
            gst_caps_unref (caps);
        }
    }
#endif

    ret = gst_pad_alloc_buffer_and_set_caps (
            self->srcpad, GST_BUFFER_OFFSET_NONE,
            len, GST_PAD_CAPS (self->srcpad), &buf);

    if (ret == GST_FLOW_OK) return buf;

    return NULL;
}

static void
type_instance_init (GTypeInstance *instance,
                    gpointer g_class)
{
    GstOmxBaseFilter21 *self;
    GstElementClass *element_class;
    GstOmxBaseFilter21Class *bclass;
	int i;
	char srcname[10];

    element_class = GST_ELEMENT_CLASS (g_class);
    bclass = GST_OMX_BASE_FILTER21_CLASS (g_class);

    self = GST_OMX_BASE_FILTER21 (instance);

    GST_LOG_OBJECT (self, "begin");


    self->gomx = g_omx_core_new (self, g_class);
    for (i = 0; i < NUM_INPUTS; i++) {
		sprintf(srcname, "in_%02x", i);
		self->input_port_index[i] = OMX_VSWMOSAIC_INPUT_PORT_START_INDEX + i;
		self->in_port[i] = g_omx_core_get_port (self->gomx, srcname, self->input_port_index[i]);
		self->in_port[i]->omx_allocate = TRUE;
		self->in_port[i]->share_buffer = FALSE;
		self->in_port[i]->port_index = self->input_port_index[i];
	}
	self->output_port_index = OMX_VSWMOSAIC_OUTPUT_PORT_START_INDEX;
	self->out_port = g_omx_core_get_port (self->gomx, "out", self->output_port_index);
	self->out_port->buffer_alloc = buffer_alloc;
	self->out_port->omx_allocate = TRUE;
	self->out_port->share_buffer = FALSE;
	self->out_port->port_index = self->output_port_index;
	self->number_eos = 2;
    self->ready_lock = g_mutex_new ();
    self->collectpads = gst_collect_pads_new();
    gst_collect_pads_set_function(self->collectpads, &collected_pads, self);
	for (i = 0; i < NUM_INPUTS; i++) {
		sprintf(srcname, "sink_%02x", i);
		self->sinkpad[i] =	gst_pad_new_from_template (gst_element_class_get_pad_template (element_class, srcname), srcname);

		gst_pad_set_chain_function (self->sinkpad[i], bclass->pad_chain);
		gst_pad_set_event_function (self->sinkpad[i], bclass->pad_event);
		gst_pad_set_setcaps_function (self->sinkpad[i], GST_DEBUG_FUNCPTR (sink_setcaps));
		gst_element_add_pad (GST_ELEMENT (self), self->sinkpad[i]);
		gst_collect_pads_add_pad(self->collectpads, self->sinkpad[i], sizeof(GstCollectData));
	}
	self->srcpad=
		gst_pad_new_from_template (gst_element_class_get_pad_template (element_class, "src"), "src");
	gst_pad_set_activatepush_function (self->srcpad, activate_push);
	gst_pad_set_setcaps_function (self->srcpad, GST_DEBUG_FUNCPTR (src_setcaps));
	gst_element_add_pad (GST_ELEMENT (self), self->srcpad);
    self->duration = GST_CLOCK_TIME_NONE;
    self->sink_camera_timestamp = GST_CLOCK_TIME_NONE;

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

