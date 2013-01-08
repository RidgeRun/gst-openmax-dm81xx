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

#include "gstomx_videomixer.h"
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
    ARG_PORT_INDEX,
	ARG_FRAME_RATE,
	ARG_SETTINGS_CHANGED
};

enum
{   
    ARG_00,
	ARG_OUT_WIDTH,
	ARG_OUT_HEIGHT,
	ARG_OUT_X,
	ARG_OUT_Y,
	ARG_OUT_ZORDER,
	ARG_IN_X,
	ARG_IN_Y,
	ARG_CROP_WIDTH,
	ARG_CROP_HEIGHT
};

static void init_interfaces (GType type);
GSTOMX_BOILERPLATE_FULL (GstOmxVideoMixer, gst_omx_video_mixer, GstElement, GST_TYPE_ELEMENT, init_interfaces);

static GstStaticPadTemplate sink_template =
        GST_STATIC_PAD_TEMPLATE ("sink",
                GST_PAD_SINK,
                GST_PAD_REQUEST,
                GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV_STRIDED (
                        "{NV12}", "[ 0, max ]"))
        );

static GstStaticPadTemplate src_template =
        GST_STATIC_PAD_TEMPLATE ("src",
                GST_PAD_SRC,
                GST_PAD_ALWAYS,
                GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV ( "{YUY2}" ))
        );



static GstFlowReturn push_buffer (GstOmxVideoMixer *self, GstBuffer *buf);
static GstFlowReturn pad_chain (GstPad *pad, GstBuffer *buf);
static gboolean pad_event (GstPad *pad, GstEvent *event);

static void* vidmix_input_loop(void *arg);
static GstPad *request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name);
static void release_pad (GstElement * element, GstPad * pad);

static void gst_videomixer_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_videomixer_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);


GType gst_videomixer_pad_get_type (void);
G_DEFINE_TYPE (GstVideoMixerPad, gst_videomixer_pad, GST_TYPE_PAD);

static void
gst_videomixer_pad_class_init (GstVideoMixerPadClass * klass)
{

  GObjectClass *gobject_class = (GObjectClass *) klass;
  printf("pad class init!!\n");
  gobject_class->set_property = gst_videomixer_pad_set_property;
  gobject_class->get_property = gst_videomixer_pad_get_property;

  g_object_class_install_property (gobject_class, ARG_OUT_WIDTH,
                                         g_param_spec_uint ("outWidth", "Output width",
                                                            "Op width",
                                                            0, 100000, 0, G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class, ARG_OUT_HEIGHT,
                                         g_param_spec_uint ("outHeight", "Output height",
                                                            "op height",
                                                            0, 100000, 0, G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class, ARG_OUT_X,
                                         g_param_spec_uint ("outX", "Output X",
                                                            "op X",
                                                            0, 100000, 0, G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class, ARG_OUT_Y,
                                         g_param_spec_uint ("outY", "Output Y",
                                                            "op Y",
                                                            0, 100000, 0, G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class, ARG_OUT_ZORDER,
      g_param_spec_uint ("zorder", "Z-Order", "Z Order of the picture",
          0, 10000, 0,
          G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class, ARG_CROP_WIDTH,
                                         g_param_spec_uint ("cropWidth", "crop width",
                                                            "crop width",
                                                            0, 100000, 0, G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class, ARG_CROP_HEIGHT,
                                         g_param_spec_uint ("cropHeight", "crop height",
                                                            "crop height",
                                                            0, 100000, 0, G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class, ARG_IN_X,
                                         g_param_spec_uint ("inX", "Input X",
                                                            "ip X",
                                                            0, 100000, 0, G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class, ARG_IN_Y,
                                         g_param_spec_uint ("inY", "INput Y",
                                                            "ip Y",
                                                            0, 100000, 0, G_PARAM_WRITABLE));
  
}

static void
gst_videomixer_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstVideoMixerPad *pad = GST_VIDEO_MIXER_PAD (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_videomixer_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVideoMixerPad *pad = GST_VIDEO_MIXER_PAD (object);
  //ip_params *ch_info;
  //ch_info = (ip_params *)gst_pad_get_element_private(pad);
  switch (prop_id) {
    case ARG_OUT_WIDTH:
      pad->outWidth = g_value_get_uint (value);
      break;
    case ARG_OUT_HEIGHT:
      pad->outHeight = g_value_get_uint (value);
      break;
    case ARG_OUT_X:
      pad->outX  = g_value_get_uint (value);
	//  printf("property X!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!:%d\n",pad->outX);
      break;
    case ARG_OUT_Y:
      pad->outY  = g_value_get_uint (value);
      break;
    case ARG_OUT_ZORDER:
      pad->order = g_value_get_uint (value);
	  break;
	case ARG_CROP_WIDTH:
	  pad->cropWidth = g_value_get_uint (value);
	  break;
	case ARG_CROP_HEIGHT:
	  pad->cropHeight = g_value_get_uint (value);
	  break;
	case ARG_IN_X:
	  pad->inX = g_value_get_uint (value);
	  break;
	case ARG_IN_Y:
	  pad->inY = g_value_get_uint (value);
	  break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_videomixer_pad_init (GstVideoMixerPad * mixerpad)
{
/*
  gst_pad_set_setcaps_function (GST_PAD (mixerpad),
      gst_videomixer_pad_sink_setcaps);
  gst_pad_set_acceptcaps_function (GST_PAD (mixerpad),
      GST_DEBUG_FUNCPTR (gst_videomixer_pad_sink_acceptcaps));
  gst_pad_set_getcaps_function (GST_PAD (mixerpad),
      gst_videomixer_pad_sink_getcaps);

  mixerpad->zorder = DEFAULT_PAD_ZORDER;
  mixerpad->xpos = DEFAULT_PAD_XPOS;
  mixerpad->ypos = DEFAULT_PAD_YPOS;
  mixerpad->alpha = DEFAULT_PAD_ALPHA;*/
}


static void
setup_input_buffer (GstOmxVideoMixer *self)
{
    guint ii;
	GstBuffer *buf;
	for(ii = 0; ii < self->numpads; ii++) {
	   /* if (GST_IS_OMXBUFFERTRANSPORT (buf))*/
	    {
	        OMX_PARAM_PORTDEFINITIONTYPE param;
	        GOmxPort *port, *in_port;
	        gint i;
            in_port =  self->in_port[ii];
	        /* retrieve incoming buffer port information */
			buf = (GstBuffer *)async_queue_pop_full(self->sinkpad[ii]->queue,TRUE,FALSE);
	        port = GST_GET_OMXPORT (buf);
	        /* configure input buffer size to match with upstream buffer */
	        G_OMX_PORT_GET_DEFINITION (self->in_port[ii], &param);
	        param.nBufferSize =  GST_BUFFER_SIZE (buf);
	        param.nBufferCountActual = port->num_buffers;
	        G_OMX_PORT_SET_DEFINITION (self->in_port[ii], &param);

	        /* allocate resource to save the incoming buffer port pBuffer pointer in
	         * OmxBufferInfo structure.
	         */        
	        in_port->share_buffer_info = malloc (sizeof(OmxBufferInfo));
	        in_port->share_buffer_info->pBuffer = malloc (sizeof(int) * port->num_buffers);
	        for (i=0; i < port->num_buffers; i++) {
	            in_port->share_buffer_info->pBuffer[i] = port->buffers[i]->pBuffer;
	        }

	        /* disable omx_allocate alloc flag, so that we can fall back to shared method */
	        self->in_port[ii]->omx_allocate = FALSE;
	        self->in_port[ii]->always_copy = FALSE;
			async_queue_push (self->sinkpad[ii]->queue, buf);
	    }
	    /*else
	    {
	        printf("Ensure upstream component allocates OMX buffer!!\n");
	    }*/
	}
}
#if 0
static void
gstomx_vfpc_set_port_index (GObject *obj, int index)
{
    GstOmxVideoMixer *omx_base;
    GstOmxVideoMixer *self;

    omx_base = GST_OMX_VIDEO_MIXER (obj);
    self = GST_OMX_VIDEO_MIXER (obj);

    self->input_port_index = OMX_VFPC_INPUT_PORT_START_INDEX + index;
    self->output_port_index = OMX_VFPC_OUTPUT_PORT_START_INDEX + index;
 
    /* free the existing core and ports */
    g_omx_core_free (omx_base->gomx);
    g_omx_port_free (omx_base->in_port);
    g_omx_port_free (omx_base->out_port);

    /* create new core and ports */
    omx_base->gomx = g_omx_core_new (omx_base, self->g_class);
    omx_base->in_port = g_omx_core_get_port (omx_base->gomx, "in", self->input_port_index);
    omx_base->out_port = g_omx_core_get_port (omx_base->gomx, "out", self->output_port_index);

    omx_base->in_port->omx_allocate = TRUE;
    omx_base->in_port->share_buffer = FALSE;
    omx_base->in_port->always_copy  = FALSE;

    omx_base->out_port->omx_allocate = TRUE;
    omx_base->out_port->share_buffer = FALSE;
    omx_base->out_port->always_copy = FALSE;

    omx_base->in_port->port_index = self->input_port_index;
    omx_base->out_port->port_index = self->output_port_index;
}
#endif

static GstStateChangeReturn
change_state (GstElement *element,
              GstStateChange transition)
{
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
    GstOmxVideoMixer *self;
    GOmxCore *core;

    self = GST_OMX_VIDEO_MIXER (element);
    core = self->gomx;

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
            } else {
            }
            break;
		
        default:
            break;
    }
   //sleep(10);
    ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

    if (ret == GST_STATE_CHANGE_FAILURE)
        goto leave;

    switch (transition)
    	{
        case GST_STATE_CHANGE_PAUSED_TO_READY:
            if (self->ready)
            {
                int ii = 0;
				guint thread_ret;
				gpointer obj;
                /* unlock */
				printf("paused to ready!!\n");
				for(ii = 0; ii < self->numpads; ii++) {
                  g_omx_port_finish (self->in_port[ii]);
                  g_omx_port_finish (self->out_port[ii]);
				}
				printf("setting EOS to true\n");
				self->eos = TRUE;
				for(ii = 0; ii < self->numpads; ii++)
				  async_queue_disable (self->sinkpad[ii]->queue);
                printf("Waiting for ip thread to exit..semup!!\n");
				g_sem_up(self->bufferSem);
				pthread_join(self->input_loop, &thread_ret);

			   for(ii = 0; ii < self->numpads; ii++)
				 while(obj = async_queue_pop_full(self->sinkpad[ii]->queue, FALSE, TRUE)) { 
					printf("freeing un-processed buffer in queue - %d\n",ii);
					gst_buffer_unref(obj);
				 }
				
                g_omx_core_stop (core);
                g_omx_core_unload (core);
				for(ii = 0; ii < self->numpads; ii++)
				  g_free(self->orderList[ii]);

				g_free(self->orderList);
                self->ready = FALSE;
            }
           // g_mutex_unlock (self->ready_lock);
            if (core->omx_state != OMX_StateLoaded &&
                core->omx_state != OMX_StateInvalid)
            {
                ret = GST_STATE_CHANGE_FAILURE;
                goto leave;
            }
			printf("paused to ready...done!!\n");
            break;

        case GST_STATE_CHANGE_READY_TO_NULL:
			printf("calling g_omx_core_deinit\n");
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
    GstOmxVideoMixer *self;

    self = GST_OMX_VIDEO_MIXER (obj);

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
	g_mutex_free (self->loop_lock);

    G_OBJECT_CLASS (parent_class)->finalize (obj);
}

void update_scaler(GstOmxVideoMixer *self)
{
	 GOmxCore *gomx;
     OMX_ERRORTYPE err;
	 int ii,kk;
	 Olist tmp;

	OMX_CONFIG_VIDCHANNEL_RESOLUTION chResolution;
		gomx = (GOmxCore *) self->gomx;

		g_mutex_lock(self->loop_lock);
	    /* Set output channel resolution */
	    GST_LOG_OBJECT (self, "Setting channel resolution (output)");
		for(ii = 0;ii < self->numpads ; ii++) {
	    _G_OMX_INIT_PARAM (&chResolution);
	    chResolution.Frm0Width = self->sinkpad[ii]->outWidth;
	    chResolution.Frm0Height = self->sinkpad[ii]->outHeight;
	    chResolution.Frm0Pitch = self->out_stride;
	    chResolution.Frm1Width = 0;
	    chResolution.Frm1Height = 0;
	    chResolution.Frm1Pitch = 0;
	    chResolution.FrmStartX = self->sinkpad[ii]->outX*2;
	    chResolution.FrmStartY = self->sinkpad[ii]->outY;
	    chResolution.FrmCropWidth = 0;
	    chResolution.FrmCropHeight = 0;
	    chResolution.eDir = OMX_DirOutput;
		chResolution.nPortIndex = ii + OMX_VFPC_OUTPUT_PORT_START_INDEX;		
	    chResolution.nChId = ii;
		//printf("calling setconfig!!\n");
	    err = OMX_SetConfig (gomx->omx_handle, OMX_TI_IndexConfigVidChResolution, &chResolution);

	    if (err != OMX_ErrorNone) {
			printf("error setting param!!\n");
	        return;
	    }
		self->orderList[ii]->idx = ii;
		self->orderList[ii]->order = self->sinkpad[ii]->order;
        printf("updating crop : %d,%d ... %dx%d\n",self->sinkpad[ii]->inX,self->sinkpad[ii]->inY,
						self->sinkpad[ii]->cropWidth,self->sinkpad[ii]->cropHeight);
		_G_OMX_INIT_PARAM (&chResolution);
	    chResolution.Frm0Width = self->sinkpad[ii]->in_width;
	    chResolution.Frm0Height = self->sinkpad[ii]->in_height;
	    chResolution.Frm0Pitch = self->sinkpad[ii]->in_stride;
	    chResolution.Frm1Width = 0;
	    chResolution.Frm1Height = 0;
	    chResolution.Frm1Pitch = 0;
	    chResolution.FrmStartX = self->sinkpad[ii]->inX;//chResolution.Frm0Width/2;//self->left;
	    chResolution.FrmStartY = self->sinkpad[ii]->inY;//chResolution.Frm0Height/2;//self->top;
	    chResolution.FrmCropWidth = self->sinkpad[ii]->cropWidth;//0;
	    chResolution.FrmCropHeight = self->sinkpad[ii]->cropHeight;//0;
	    chResolution.eDir = OMX_DirInput;
		chResolution.nPortIndex = ii;
	    chResolution.nChId = ii;
	    err = OMX_SetConfig (gomx->omx_handle, OMX_TI_IndexConfigVidChResolution, &chResolution);

	    if (err != OMX_ErrorNone){
			printf("error setting param1!!\n");
	        return;
	    }
		}

		
	for(ii=0;ii<self->numpads;ii++)
		for(kk=ii+1;kk<self->numpads;kk++)
			if(self->orderList[kk]->order < self->orderList[ii]->order) {
               tmp = *(self->orderList[kk]);
			   *(self->orderList[kk]) = *(self->orderList[ii]);
			   	*(self->orderList[ii]) = tmp;
			}
    /*printf("ip zorder - starting from lowest: %d",self->orderList[0]->idx);
	for(ii=1;ii<self->numpads;ii++)
		printf(", %d",self->orderList[ii]->idx);
	printf("\n");*/
		g_mutex_unlock(self->loop_lock);
		//printf("Ret!!\n");
		
}
static void
set_property (GObject *obj,
              guint prop_id,
              const GValue *value,
              GParamSpec *pspec)
{
    GstOmxVideoMixer *self;

    self = GST_OMX_VIDEO_MIXER (obj);
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
        case ARG_NUM_INPUT_BUFFERS:
        case ARG_NUM_OUTPUT_BUFFERS:
            {
                OMX_PARAM_PORTDEFINITIONTYPE param;
                OMX_U32 nBufferCountActual = g_value_get_uint (value);
                GOmxPort *port = (prop_id == ARG_NUM_INPUT_BUFFERS) ?
                        self->in_port : self->out_port;

                G_OMX_PORT_GET_DEFINITION (port, &param);

                g_return_if_fail (nBufferCountActual >= param.nBufferCountMin);
                param.nBufferCountActual = nBufferCountActual;

                G_OMX_PORT_SET_DEFINITION (port, &param);
            }
            break;
		case ARG_PORT_INDEX:
            self->port_index = g_value_get_uint (value);
            if (!self->port_configured) 
                //gstomx_vfpc_set_port_index (obj, self->port_index);
            break;
		case ARG_FRAME_RATE:
			self->framerate_num = g_value_get_uint (value);
			break;
		case ARG_SETTINGS_CHANGED:
			self->settingsChanged = g_value_get_boolean (value);
			update_scaler(self);
			//printf("settings updated!!\n");
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
    GstOmxVideoMixer *self;

    self = GST_OMX_VIDEO_MIXER (obj);

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
        case ARG_NUM_INPUT_BUFFERS:
        case ARG_NUM_OUTPUT_BUFFERS:
            {
                OMX_PARAM_PORTDEFINITIONTYPE param;
                GOmxPort *port = (prop_id == ARG_NUM_INPUT_BUFFERS) ?
                        self->in_port[0] : self->out_port[0];

                G_OMX_PORT_GET_DEFINITION (port, &param);

                g_value_set_uint (value, param.nBufferCountActual);
            }
            break;
		case ARG_PORT_INDEX:
            g_value_set_uint (value, self->port_index);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
            break;
    }
}

static void
type_base_init (gpointer g_class)
{
    GstElementClass *element_class;

    element_class = GST_ELEMENT_CLASS (g_class);

    {
        GstElementDetails details;

        details.longname = "OpenMAX IL for OMX.TI.VPSSM3.VFPC.INDTXSCWB component";
        details.klass = "Filter";
        details.description = "Scale video using VPSS Scaler module ";
        details.author = "Prashant Nandakumar";

        gst_element_class_set_details (element_class, &details);
    }
    gst_element_class_add_pad_template (element_class,
        gst_static_pad_template_get (&sink_template));

    gst_element_class_add_pad_template (element_class,
        gst_static_pad_template_get (&src_template));

}

static void
type_class_init (gpointer g_class,
                 gpointer class_data)
{
    GObjectClass *gobject_class;
    GstElementClass *gstelement_class;
    GstOmxVideoMixerClass *bclass;

    gobject_class = G_OBJECT_CLASS (g_class);
    gstelement_class = GST_ELEMENT_CLASS (g_class);
    bclass = GST_OMX_VIDEO_MIXER_CLASS (g_class);
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

        /* note: the default values for these are just a guess.. since we wouldn't know
         * until the OMX component is constructed.  But that is ok, these properties are
         * only for debugging
         */
        g_object_class_install_property (gobject_class, ARG_NUM_INPUT_BUFFERS,
                                         g_param_spec_uint ("input-buffers", "Input buffers",
                                                            "The number of OMX input buffers",
                                                            1, 10, 4, G_PARAM_READWRITE));
        g_object_class_install_property (gobject_class, ARG_NUM_OUTPUT_BUFFERS,
                                         g_param_spec_uint ("output-buffers", "Output buffers",
                                                            "The number of OMX output buffers",
                                                            1, 10, 4, G_PARAM_READWRITE));

		g_object_class_install_property (gobject_class, ARG_PORT_INDEX,
                                         g_param_spec_uint ("port-index", "port index",
                                                            "input/output start port index",
                                                            0, 8, 0, G_PARAM_READWRITE));

		g_object_class_install_property (gobject_class, ARG_FRAME_RATE,
                                         g_param_spec_uint ("framerate", "Frame Rate",
                                                            "The rate at which the mixer ocmponent would operate",
                                                            1, 60, 15, G_PARAM_WRITABLE));

		g_object_class_install_property (gobject_class, ARG_SETTINGS_CHANGED,
                                         g_param_spec_boolean ("settingsChanged", "settingsChanged",
                                                               "Indicate if the port settings have changed",
                                                               TRUE, G_PARAM_WRITABLE));
    }

	/* Register the pad class */
  (void) (GST_TYPE_VIDEO_MIXER_PAD);

  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (request_new_pad);
  gstelement_class->release_pad =
      GST_DEBUG_FUNCPTR (release_pad);
}

static GstCaps*
create_src_caps (GstOmxVideoMixer *omx_base)
{
    GstCaps *caps;    
    GstOmxVideoMixer *self;
    int width, height;
    GstStructure *struc;

    self = GST_OMX_VIDEO_MIXER (omx_base);
    caps = gst_pad_peer_get_caps (omx_base->srcpad);
     //printf("create src caps!!\n");
    if (gst_caps_is_empty (caps))
    {
        width = self->sinkpad[0]->in_width;
        height = self->sinkpad[0]->in_height;
		//printf("set 1 height:%d, width:%d\n",height,width);
    }
    else
    {
        GstStructure *s;

        s = gst_caps_get_structure (caps, 0);

        if (!(gst_structure_get_int (s, "width", &width) &&
            gst_structure_get_int (s, "height", &height)))
        {
            width = self->sinkpad[0]->in_width;
            height = self->sinkpad[0]->in_height;  
			//printf("set 2 height:%d, width:%d\n",height,width);
        }
    }
    //printf("set 3 height:%d, width:%d\n",height,width);
    caps = gst_caps_new_empty ();
    struc = gst_structure_new (("video/x-raw-yuv"),
            "width",  G_TYPE_INT, width,
            "height", G_TYPE_INT, height,
            "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('Y', 'U', 'Y', '2'),
            NULL);


    if (self->framerate_denom)
    {
        gst_structure_set (struc,
        "framerate", GST_TYPE_FRACTION, self->framerate_num, self->framerate_denom, NULL);
    }

    gst_caps_append_structure (caps, struc);

	printf("created output caps:%s\n",gst_caps_to_string(caps));

    return caps;
}

guint arr[4][2] = { {0,0}, {1,0}, {0,1}, {1,1} };

static void
scaler_setup (GstOmxVideoMixer *omx_base)
{
    GOmxCore *gomx;
    OMX_ERRORTYPE err;
    OMX_PARAM_PORTDEFINITIONTYPE paramPort;
    OMX_PARAM_BUFFER_MEMORYTYPE memTypeCfg;
    OMX_PARAM_VFPC_NUMCHANNELPERHANDLE numChannels;
    OMX_CONFIG_VIDCHANNEL_RESOLUTION chResolution;
    OMX_CONFIG_ALG_ENABLE algEnable;
    GstOmxVideoMixer *self;
	int ii;

    gomx = (GOmxCore *) omx_base->gomx;
    self = GST_OMX_VIDEO_MIXER (omx_base);

    GST_LOG_OBJECT (self, "begin");
   // printf("scaler setup!!|n");
    /* set the output cap */
    gst_pad_set_caps (omx_base->srcpad, create_src_caps (omx_base));
    for(ii = 0; ii < self->numpads; ii++) {
		if(self->sinkpad[ii]->outWidth == -1)
			self->sinkpad[ii]->outWidth = self->out_width/2;

		if(self->sinkpad[ii]->outHeight == -1)
			self->sinkpad[ii]->outHeight = self->out_height/2;

    }
	for(ii = 0; ii < self->numpads; ii++) {
		if(self->sinkpad[ii]->outX == -1)
			self->sinkpad[ii]->outX = arr[ii][0]*(self->out_width)/2;

		if(self->sinkpad[ii]->outY == -1)
			self->sinkpad[ii]->outY = arr[ii][1]*self->out_height/2;

    }

    for(ii = 0; ii < self->numpads; ii++) {
        if(self->sinkpad[ii]->cropWidth == -1)
			self->sinkpad[ii]->cropWidth = self->sinkpad[ii]->in_width;

		if(self->sinkpad[ii]->cropHeight == -1)
			self->sinkpad[ii]->cropHeight = self->sinkpad[ii]->in_height;
    }
    /* Setting Memory type at input port to Raw Memory */
    GST_LOG_OBJECT (self, "Setting input port to Raw memory");
    for(ii = 0; ii < self->numpads; ii++) {
		
	    _G_OMX_INIT_PARAM (&memTypeCfg);
	    memTypeCfg.nPortIndex = self->input_port_index[ii];
	    memTypeCfg.eBufMemoryType = OMX_BUFFER_MEMORY_DEFAULT;    
	    err = OMX_SetParameter (gomx->omx_handle, OMX_TI_IndexParamBuffMemType, &memTypeCfg);

	    if (err != OMX_ErrorNone)
	        return;

	    /* Setting Memory type at output port to Raw Memory */
	    GST_LOG_OBJECT (self, "Setting output port to Raw memory");

	    _G_OMX_INIT_PARAM (&memTypeCfg);
	    memTypeCfg.nPortIndex = self->output_port_index[ii];
	    memTypeCfg.eBufMemoryType = OMX_BUFFER_MEMORY_DEFAULT;
	    err = OMX_SetParameter (gomx->omx_handle, OMX_TI_IndexParamBuffMemType, &memTypeCfg);

	    if (err != OMX_ErrorNone)
	        return;

	    /* Input port configuration. */
	    GST_LOG_OBJECT (self, "Setting port definition (input)");

	    G_OMX_PORT_GET_DEFINITION (omx_base->in_port[ii], &paramPort);
	    paramPort.format.video.nFrameWidth = self->sinkpad[ii]->in_width;
	    paramPort.format.video.nFrameHeight = self->sinkpad[ii]->in_height;
	    paramPort.format.video.nStride = self->sinkpad[ii]->in_stride;
	    paramPort.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
	    paramPort.format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
	    paramPort.nBufferSize =  self->sinkpad[ii]->in_stride * self->sinkpad[ii]->in_height * 1.5;
	    paramPort.nBufferAlignment = 0;
	    paramPort.bBuffersContiguous = 0;
	    G_OMX_PORT_SET_DEFINITION (omx_base->in_port[ii], &paramPort);
	    g_omx_port_setup (omx_base->in_port[ii], &paramPort);

	    /* Output port configuration. */
	    GST_LOG_OBJECT (self, "Setting port definition (output)");

	    G_OMX_PORT_GET_DEFINITION (omx_base->out_port[ii], &paramPort);
	    paramPort.format.video.nFrameWidth = self->out_width;
	    paramPort.format.video.nFrameHeight = self->out_height;
	    paramPort.format.video.nStride = self->out_stride;
	    paramPort.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
	    paramPort.format.video.eColorFormat = OMX_COLOR_FormatYCbYCr;
	    paramPort.nBufferSize =  self->out_stride * self->out_height;
	    paramPort.nBufferCountActual = 8;
	    paramPort.nBufferAlignment = 0;
	    paramPort.bBuffersContiguous = 0;
	    G_OMX_PORT_SET_DEFINITION (omx_base->out_port[ii], &paramPort);
	    g_omx_port_setup (omx_base->out_port[ii], &paramPort);
		
    }
    /* Set number of channles */
    GST_LOG_OBJECT (self, "Setting number of channels");

    _G_OMX_INIT_PARAM (&numChannels);
    numChannels.nNumChannelsPerHandle = self->numpads;    
    err = OMX_SetParameter (gomx->omx_handle, 
        (OMX_INDEXTYPE) OMX_TI_IndexParamVFPCNumChPerHandle, &numChannels);

    if (err != OMX_ErrorNone)
        return;

    for(ii = 0; ii < self->numpads; ii++) {
	    /* Set input channel resolution */
	    GST_LOG_OBJECT (self, "Setting channel resolution (input)");

	    _G_OMX_INIT_PARAM (&chResolution);
	    chResolution.Frm0Width = self->sinkpad[ii]->in_width;
	    chResolution.Frm0Height = self->sinkpad[ii]->in_height;
	    chResolution.Frm0Pitch = self->sinkpad[ii]->in_stride;
	    chResolution.Frm1Width = 0;
	    chResolution.Frm1Height = 0;
	    chResolution.Frm1Pitch = 0;
	    chResolution.FrmStartX = self->sinkpad[ii]->inX;//chResolution.Frm0Width/2;//self->left;
	    chResolution.FrmStartY = self->sinkpad[ii]->inY;//chResolution.Frm0Height/2;//self->top;
	    chResolution.FrmCropWidth = self->sinkpad[ii]->cropWidth;//0;
	    chResolution.FrmCropHeight = self->sinkpad[ii]->cropHeight;//0;
	    chResolution.eDir = OMX_DirInput;
		chResolution.nPortIndex = ii;
	    chResolution.nChId = ii;
	    err = OMX_SetConfig (gomx->omx_handle, OMX_TI_IndexConfigVidChResolution, &chResolution);

	    if (err != OMX_ErrorNone)
	        return;

	    /* Set output channel resolution */
	    GST_LOG_OBJECT (self, "Setting channel resolution (output)");
	    _G_OMX_INIT_PARAM (&chResolution);
		
		chResolution.Frm0Width = self->sinkpad[ii]->outWidth;//self->out_width/2;
		chResolution.Frm0Height = self->sinkpad[ii]->outHeight;//self->out_height/2;
	    chResolution.Frm0Pitch = self->out_stride;
	    chResolution.Frm1Width = 0;
	    chResolution.Frm1Height = 0;
	    chResolution.Frm1Pitch = 0;
	    chResolution.FrmStartX = self->sinkpad[ii]->outX*2;//arr[ii][0]*(chResolution.Frm0Width*2);
	    chResolution.FrmStartY = self->sinkpad[ii]->outY;//arr[ii][1]*chResolution.Frm0Height;
	    chResolution.FrmCropWidth = 0;
	    chResolution.FrmCropHeight = 0;
	    chResolution.eDir = OMX_DirOutput;
		chResolution.nPortIndex = ii + OMX_VFPC_OUTPUT_PORT_START_INDEX;		
	    chResolution.nChId = ii;
	    err = OMX_SetConfig (gomx->omx_handle, OMX_TI_IndexConfigVidChResolution, &chResolution);

	    if (err != OMX_ErrorNone)
	        return;

		//self->sinkpad[ii]->outWidth  = chResolution.Frm0Width; 
	    //self->sinkpad[ii]->outHeight = chResolution.Frm0Height; 
	    //self->sinkpad[ii]->outX = arr[ii][0]*self->sinkpad[ii]->outWidth;
	    //self->sinkpad[ii]->outY = chResolution.FrmStartY;

	    _G_OMX_INIT_PARAM (&algEnable);
	    algEnable.nPortIndex = ii;
	    algEnable.nChId = ii;
	    algEnable.bAlgBypass = OMX_FALSE;

	    err = OMX_SetConfig (gomx->omx_handle, (OMX_INDEXTYPE) OMX_TI_IndexConfigAlgEnable, &algEnable);

	    if (err != OMX_ErrorNone)
	        return;
    }
	    //printf("scaler setup...done!!|n");
}

static void
omx_setup (GstOmxVideoMixer *omx_base)
{
    GstOmxVideoMixer *self;
    GOmxCore *gomx;
    GOmxPort *port;
	int ii;

    self = GST_OMX_VIDEO_MIXER (omx_base);
    gomx = (GOmxCore *) omx_base->gomx;
   // printf("omx_setup!!\n");
    GST_INFO_OBJECT (omx_base, "begin");
    
    /* enable input port */
	for(ii = 0; ii < self->numpads; ii++) {
	    port = omx_base->in_port[ii];
	    OMX_SendCommand (g_omx_core_get_handle (port->core),
	            OMX_CommandPortEnable, port->port_index, NULL);
	    g_sem_down (port->core->port_sem);

	    /* enable output port */
	    port = omx_base->out_port[ii];
	    OMX_SendCommand (g_omx_core_get_handle (port->core),
	            OMX_CommandPortEnable, port->port_index, NULL);
	    g_sem_down (port->core->port_sem);
    }

    /* indicate that port is now configured */
    self->port_configured = TRUE;
    //printf("omx_setup...done!!\n");
    GST_INFO_OBJECT (omx_base, "end");
}


static GstFlowReturn
push_buffer (GstOmxVideoMixer *self,
             GstBuffer *buf)
{
    GstFlowReturn ret;

	GST_BUFFER_DURATION (buf) = self->duration;

    PRINT_BUFFER (self, buf);

    /** @todo check if tainted */
    GST_LOG_OBJECT (self, "begin");
    ret = gst_pad_push (self->srcpad, buf);
    GST_LOG_OBJECT (self, "end");

    return ret;
}

static OMX_BUFFERHEADERTYPE *
request_buffer (GOmxPort *port)
{
    //LOG (port, "request buffer");
    return async_queue_pop (port->queue);
}


gpointer
vidmix_port_recv (GstOmxVideoMixer *self)
{
    gpointer ret = NULL;
	GOmxPort *port;
    OMX_BUFFERHEADERTYPE *omx_buffer,*omx_buffer1;
	guint ii;
    OMX_BUFFERHEADERTYPE *omx_bufferHdr[NUM_PORTS - 1];
//printf("a\n");
    //while (!ret && port->enabled)
    for(ii = 0; ii < self->numpads; ii++)
    {
        
		port = self->out_port[ii];
		g_return_val_if_fail (port->type == GOMX_PORT_OUTPUT, NULL);

		/*if((self->chInfo[ii].eos == TRUE) && (self->numEosPending != 0)) {
			omx_bufferHdr[ii-1] = NULL;
			continue;
		}*/
	   // printf("Request buffer:%d!!\n",ii);
		omx_buffer1 = request_buffer (port);
		//printf("got buffer:%d!!\n",ii);

        if (G_UNLIKELY (!omx_buffer1))
        {
            return NULL;
        }
        //printf("Received:%p\n",omx_buffer1->pBuffer);
		if(ii == 0) {
			omx_buffer = omx_buffer1;
		} else {
		    //printf("vidmixop:%p\n",omx_buffer1);
		    omx_bufferHdr[ii-1] = omx_buffer1;		
		}
			

        if (G_UNLIKELY (omx_buffer1->nFlags & OMX_BUFFERFLAG_EOS))
        {
            //DEBUG (port, "got eos");
            return gst_event_new_eos ();
        }
    }
	
		
    if (G_LIKELY (omx_buffer->nFilledLen > 0))
        {
            GstBuffer *buf;
            
   buf = gst_omxbuffertransport_new (self->out_port[0], omx_buffer);
            //printf("buffer size:%d\n",GST_BUFFER_SIZE(buf));
            //GST_BUFFER_SIZE(buf) = GST_BUFFER_SIZE(buf)*2;
            GST_BUFFER_SIZE(buf) = self->outbufsize;
            if (port->core->use_timestamps)
            {
                /*GST_BUFFER_TIMESTAMP (buf) = gst_util_uint64_scale_int (
                        omx_buffer->nTimeStamp,
                        GST_SECOND, OMX_TICKS_PER_SECOND);*/
              /*if(self->timestamp == 0)
			  	self->timestamp = gst_clock_get_time(gst_element_get_clock(self)) - gst_element_get_base_time(self);*/
              GST_BUFFER_TIMESTAMP (buf) = self->timestamp;
			  GST_BUFFER_DURATION (buf) = self->duration;
			  self->timestamp += self->duration;
            }

            gst_omxbuffertransport_set_additional_headers (buf ,self->numpads -1,&omx_bufferHdr);
			gst_omxbuffertransport_set_bufsem (buf ,self->bufferSem);
            port->n_offset = omx_buffer->nOffset;

            ret = buf;
        }
	//printf("b\n");
return ret;
}
static void
output_loop (gpointer data)
{
    GstPad *pad;
    GOmxCore *gomx;
    GOmxPort *out_port;
    GstOmxVideoMixer *self;
    GstFlowReturn ret = GST_FLOW_OK;
    GstOmxVideoMixerClass *bclass;
    gpointer obj;
    pad = data;
    self = GST_OMX_VIDEO_MIXER (gst_pad_get_parent (pad));
    gomx = self->gomx;

    bclass = GST_OMX_VIDEO_MIXER_GET_CLASS (self);

    GST_LOG_OBJECT (self, "begin");

    if (!self->ready)
    {
        g_error ("not ready");
        return;
    }
   
	    out_port = self->out_port[0];

	    if (G_LIKELY (out_port->enabled))
	    {
			obj = vidmix_port_recv (self);
	        if (G_UNLIKELY (!obj))
	        {
	            GST_WARNING_OBJECT (self, "null buffer: leaving");
				printf("NULL buffer leaving!!\n");
	            ret = GST_FLOW_WRONG_STATE;
	            goto leave;
	        }
	
    if (G_LIKELY (GST_IS_BUFFER (obj)))
    {
       
        GstBuffer *buf = GST_BUFFER (obj);
		/*printf("push : %p %" 
                    GST_TIME_FORMAT ", duration: %" GST_TIME_FORMAT"\n",GST_BUFFER_DATA(buf),
                    GST_TIME_ARGS (GST_BUFFER_TIMESTAMP(buf)),
                    GST_TIME_ARGS (GST_BUFFER_DURATION(buf)));*/
        ret = bclass->push_buffer (self, buf);
        GST_DEBUG_OBJECT (self, "ret=%s", gst_flow_get_name (ret));
    
    }
    else if (GST_IS_EVENT (obj))
    {
        GST_DEBUG_OBJECT (self, "got eos");
		                printf("here....eos!!\n");
        gst_pad_push_event (self->srcpad, obj);
        ret = GST_FLOW_UNEXPECTED;
        goto leave;
    }
	   }

leave:

    self->last_pad_push_return = ret;

    if (gomx->omx_error != OMX_ErrorNone)
    {
        GST_DEBUG_OBJECT (self, "omx_error=%s", g_omx_error_to_str (gomx->omx_error));
        ret = GST_FLOW_ERROR;
    }

    if (ret != GST_FLOW_OK)
    {
        GST_INFO_OBJECT (self, "pause task, reason:  %s",
                         gst_flow_get_name (ret));
        gst_pad_pause_task (self->srcpad);
    }

    GST_LOG_OBJECT (self, "end");

    gst_object_unref (self);
}

void
vidmix_prepare (GOmxPort *port)
{
    OMX_PARAM_PORTDEFINITIONTYPE param;
#if 0
    GstBuffer *buf;
    guint size;

    DEBUG (port, "begin");

    G_OMX_PORT_GET_DEFINITION (port, &param);
    size = param.nBufferSize;

    buf = buffer_alloc (port, size);

    if (GST_BUFFER_SIZE (buf) != size)
    {
        DEBUG (port, "buffer sized changed, %d->%d",
                size, GST_BUFFER_SIZE (buf));
    }
#endif
    /* number of buffers could have changed */
    G_OMX_PORT_GET_DEFINITION (port, &param);
    port->num_buffers = param.nBufferCountActual;

   // gst_buffer_unref (buf);
}
static void
videomixer_port_prepare (GOmxPort *port)
{
    /* only prepare if the port is actually enabled: */
    if (port->enabled)
        vidmix_prepare (port);
}

typedef void (*GOmxPortFunc) (GOmxPort *port);

static inline GOmxPort *
get_port (GOmxCore *core, guint index)
{
    if (G_LIKELY (index < core->ports->len))
    {
        return g_ptr_array_index (core->ports, index);
    }

    return NULL;
}


static void inline
core_for_each_port (GOmxCore *core,
                    GOmxPortFunc func)
{
    guint index;

    for (index = 0; index < core->ports->len; index++)
    {
        GOmxPort *port;

        port = get_port (core, index);

        if (port)
            func (port);
    }
}

static inline void
change_state1 (GOmxCore *core,
              OMX_STATETYPE state)
{
    GST_DEBUG_OBJECT (core->object, "state=%d", state);
    OMX_SendCommand (core->omx_handle, OMX_CommandStateSet, state, NULL);
}

static inline void
wait_for_state (GOmxCore *core,
                OMX_STATETYPE state)
{
    GTimeVal tv;
    gboolean signaled;

    g_mutex_lock (core->omx_state_mutex);

    if (core->omx_error != OMX_ErrorNone)
        goto leave;

    g_get_current_time (&tv);
    g_time_val_add (&tv, 100000000);

    /* try once */
    if (core->omx_state != state)
    {
        signaled = g_cond_timed_wait (core->omx_state_condition, core->omx_state_mutex, &tv);

        if (!signaled)
        {
            GST_ERROR_OBJECT (core->object, "timed out");
        }
    }

    if (core->omx_error != OMX_ErrorNone)
        goto leave;

    if (core->omx_state != state)
    {
        GST_ERROR_OBJECT (core->object, "wrong state received: state=%d, expected=%d",
                          core->omx_state, state);
    }

leave:
    g_mutex_unlock (core->omx_state_mutex);
}

void
vidmix_port_allocate_buffers (GstOmxVideoMixer *self)
{
    OMX_PARAM_PORTDEFINITIONTYPE param;
    guint i,ii,iii;
    guint size;
	GOmxPort *port;
	guint16 *opbuf;

	for(ii = 0; ii < self->numpads; ii++)  {

	    port = self->in_port[ii];
	    if (port->buffers) {
			printf("WTF?? buffers already allocated!!\n");
	        return;
	    }

	    //DEBUG (port, "begin");

	    G_OMX_PORT_GET_DEFINITION (port, &param);
	    size = param.nBufferSize;

	    port->buffers = g_new0 (OMX_BUFFERHEADERTYPE *, port->num_buffers);
        //printf("Input num bufffers:%d\n",port->num_buffers);
	    for (i = 0; i < port->num_buffers; i++)
	    {
	        gpointer buffer_data = NULL;

	        buffer_data = port->share_buffer_info->pBuffer[i];
	        
	        //DEBUG (port, "%d: OMX_UseBuffer(), size=%d, share_buffer=%d", i, size, port->share_buffer);
	        OMX_UseBuffer (port->core->omx_handle,
	                       &port->buffers[i],
	                       port->port_index,
	                       NULL,
	                       size,
	                       buffer_data);

			//printf("portidx ip of buffer ip:%d, op:%d\n",port->buffers[i]->nInputPortIndex,port->buffers[i]->nOutputPortIndex);

	        g_return_if_fail (port->buffers[i]);
	    }
	}

	for(ii = 0; ii < self->numpads; ii++)  {

	    port = self->out_port[ii];
	    if (port->buffers) {
			printf("Invalid !! buffers already allocated...\n");
	        return;
	    }

	    //DEBUG (port, "begin");
        if(ii == 0) {
		    G_OMX_PORT_GET_DEFINITION (port, &param);
		    size = param.nBufferSize;

		    port->buffers = g_new0 (OMX_BUFFERHEADERTYPE *, port->num_buffers);
            self->outbufsize = size;
		    for (i = 0; i < port->num_buffers; i++)
		    {
		        gpointer buffer_data = NULL;

	            //DEBUG (port, "%d: OMX_AllocateBuffer(), size=%d", i, size);
	            //printf("allocating buffer for port idx:%d\n",port->port_index);
	            OMX_AllocateBuffer (port->core->omx_handle,
	                                &port->buffers[i],
	                                port->port_index,
	                                NULL,
	                                size);

					
                //printf("portidx op of buffer ip:%d, op:%d\n",port->buffers[i]->nInputPortIndex,port->buffers[i]->nOutputPortIndex);
                //port->buffers[i]->nOffset = self->out_width;
	            g_return_if_fail (port->buffers[i]);
				opbuf = (guint16*)port->buffers[i]->pBuffer;
                                for(iii = 0; iii < size/2; iii++)
                                        opbuf[iii] = 0x8000;
				//printf("allocated buffer:%p\n",port->buffers[i]->pBuffer);
		    } 
			self->bufferSem = g_sem_new_with_value(port->num_buffers);
        }else {
            G_OMX_PORT_GET_DEFINITION (port, &param);
		    size = param.nBufferSize;

		    port->buffers = g_new0 (OMX_BUFFERHEADERTYPE *, port->num_buffers);

		    for (i = 0; i < port->num_buffers; i++)
		    {
		        gpointer buffer_data = NULL;

	            //printf("Use buffer for port idx:%d\n",port->port_index);
	            buffer_data = self->out_port[0]->buffers[i]->pBuffer;
 	            //printf("Use buffer:%p\n",buffer_data);
				//printf("out_width:%d out_height:%d\n",self->out_width,self->out_height);
    	        OMX_UseBuffer (port->core->omx_handle,
	                       &port->buffers[i],
	                       port->port_index,
	                       NULL,
	                       size,
	                       (gchar*)buffer_data/*+(arr[ii -1][0]*self->out_width + arr[ii -1][1]*self->out_width*self->out_height)*/);
					

	            g_return_if_fail (port->buffers[i]);
		    } 

        	}
	}
   // DEBUG (port, "end");
}


static void
videomixer_port_allocate_buffers (GOmxPort *port)
{
    /* only allocate buffers if the port is actually enabled: */
    if (port->enabled)
        vidmix_port_allocate_buffers (port);
}


void
videomixer_prepare (GstOmxVideoMixer *self,GOmxCore *core)
{
    GST_DEBUG_OBJECT (core->object, "begin");

    /* Prepare port */
    core_for_each_port (core, videomixer_port_prepare);

    change_state1 (core, OMX_StateIdle);

    /* Allocate buffers. */
    //core_for_each_port (core, videomixer_port_allocate_buffers);
    videomixer_port_allocate_buffers(self);

    wait_for_state (core, OMX_StateIdle);
    GST_DEBUG_OBJECT (core->object, "end");
}

static void* vidmix_input_loop(void *arg) {
	GstOmxVideoMixer *self = (GstOmxVideoMixer *)arg;
	GOmxPort *port,*in_port;
	int ii,kk;
	GstBuffer * buf;
	GOmxCore *gomx;
	gint sent;
	struct timeval tv;
		guint64 afttime;
		guint64 beftime;

	port = self->in_port[0];
	gomx = port->core;

    if (G_UNLIKELY (gomx->omx_state == OMX_StateLoaded))
    {
        GST_INFO_OBJECT (self, "omx: prepare");
        
        /** @todo this should probably **/
        
            omx_setup (self);
        

        setup_input_buffer (self);

		scaler_setup(self);

        //printf("calline prepare!!\n");
        videomixer_prepare (self,self->gomx);
		//printf("calline prepare returned!!\n");

        if (gomx->omx_state == OMX_StateIdle)
        {
            self->ready = TRUE;
			//printf("Starting output loop1\n");
            gst_pad_start_task (self->srcpad, output_loop, self->srcpad);
        }

        if (gomx->omx_state != OMX_StateIdle) {
            printf("Transition to idle failed!!\n");
        }
    }

    //printf("inport enabled:%d\n",in_port->enabled);
    in_port = self->in_port[0];
    if (G_LIKELY (in_port->enabled))
    {        
        if (G_UNLIKELY (gomx->omx_state == OMX_StateIdle))
        {
            GST_INFO_OBJECT (self, "omx: play");
			//printf("calling omx start!!\n");
            g_omx_core_start (gomx);
			//printf("calling omx startret \n");
			
            if (gomx->omx_state != OMX_StateExecuting) {
                printf("Transition to executing failed!!\n");
            }
        }
    }
	else
    {
       printf("port not ennabled!!\n");
    }
    //printf("Init done!!\n");

    while(TRUE) {
		OMX_BUFFERHEADERTYPE *omx_buffer;
		//printf("sem cnt:%d\n",self->bufferSem->counter);
		
	    /*gettimeofday(&tv, NULL);
		beftime = (tv.tv_sec * 1000000 + tv.tv_usec);*/
		g_sem_down(self->bufferSem);
		/*gettimeofday(&tv, NULL);
		afttime = (tv.tv_sec * 1000000 + tv.tv_usec);
		printf("Wait:%lld\n",(afttime-beftime));*/
		g_mutex_lock(self->loop_lock);
        for(kk = 0; kk < self->numpads; kk++) {
			ii = self->orderList[kk]->idx;
            port = self->in_port[ii];
			gomx = port->core;
			buf = NULL;
			/*if(self->chInfo[ii].eos == FALSE)*/ {
			  buf = (GstBuffer *)async_queue_pop_full(self->sinkpad[ii]->queue,FALSE,FALSE);
			  if(buf == NULL) {

				if(self->eos == TRUE) {
					printf("goto leave!!\n");
					goto leave;
				}
			  	
                //printf("NULL buffer...ip exiting!!\n");
                //printf("reuse:%d, buffer:%p!!\n",ii,self->sinkpad[ii]->lastBuf);
                //buf = gst_buffer_ref(self->chInfo[ii].lastBuf);
                //omx_buffer = GST_GET_OMXBUFFER(self->chInfo[ii].lastBuf);
                g_mutex_lock(port->mutex);
				//printf("wait:%d,buffer:%p\n",ii,self->chInfo[ii].lastBuf);
				while(GST_BUFFER_FLAG_IS_SET(self->sinkpad[ii]->lastBuf,GST_BUFFER_FLAG_BUSY))
					g_cond_wait(port->cond,port->mutex);
				//printf("wait done:%d\n",ii);
				g_mutex_unlock(port->mutex);

                buf = (self->sinkpad[ii]->lastBuf);
			  }else {
                if(self->sinkpad[ii]->lastBuf) {
				  gst_buffer_unref(self->sinkpad[ii]->lastBuf);
                }
				self->sinkpad[ii]->lastBuf = buf;
			  }
			  	
			  GST_BUFFER_FLAG_SET(buf,GST_BUFFER_FLAG_BUSY);
	          sent = g_omx_port_send (port, buf);
			  g_mutex_lock(port->mutex);
				//printf("wait:%d,buffer:%p\n",ii,self->chInfo[ii].lastBuf);
				while(GST_BUFFER_FLAG_IS_SET(self->sinkpad[ii]->lastBuf,GST_BUFFER_FLAG_BUSY))
					g_cond_wait(port->cond,port->mutex);
				//printf("wait done:%d\n",ii);
				g_mutex_unlock(port->mutex);
			}
        }
		g_mutex_unlock(self->loop_lock);
    }
	
leave:
	//printf("leaving ip thread!!\n");
	for(ii = 0; ii < self->numpads; ii++)
		if(self->sinkpad[ii]->lastBuf)
			gst_buffer_unref(self->sinkpad[ii]->lastBuf);
	g_mutex_unlock(self->loop_lock);
	return NULL;
	
}

static GstFlowReturn
pad_chain (GstPad *pad,
           GstBuffer *buf)
{
    GOmxCore *gomx;
    GOmxPort *in_port;
    GstOmxVideoMixer *self;
    GstFlowReturn ret = GST_FLOW_OK;
	GstVideoMixerPad *mixpad ;
    
    self = GST_OMX_VIDEO_MIXER (GST_OBJECT_PARENT (pad));
	if(self->eos == TRUE) {
      gst_buffer_unref(buf);
	  return ret;
    }
    PRINT_BUFFER (self, buf);
   // printf("ip for channel %d\n",ch_info->idx);
    mixpad = GST_VIDEO_MIXER_PAD (pad);
    gomx = self->gomx;

    GST_LOG_OBJECT (self, "begin: size=%u, state=%d", GST_BUFFER_SIZE (buf), gomx->omx_state);
    g_mutex_lock (self->ready_lock);
	 if(self->ipCreated == FALSE) {
	 	        gint ii,kk;
				Olist tmp;

	            printf("Starting input thread...num sink pads:%d\n",self->numpads);
				self->orderList = (guint*)g_malloc(sizeof(guint*)*self->numpads);
				for(ii = 0; ii < self->numpads; ii++) {
            	  self->input_port_index[ii] = OMX_VFPC_INPUT_PORT_START_INDEX + ii;
                  self->output_port_index[ii] = OMX_VFPC_OUTPUT_PORT_START_INDEX + ii;
                  self->in_port[ii] = g_omx_core_get_port (self->gomx, "in", self->input_port_index[ii]);
                  self->out_port[ii] = g_omx_core_get_port (self->gomx, "out", self->output_port_index[ii]);
            	  //self->out_port[ii]->buffer_alloc = buffer_alloc;
                  self->in_port[ii]->omx_allocate = TRUE;
                  self->out_port[ii]->omx_allocate = TRUE;
                  self->in_port[ii]->share_buffer = FALSE;
                  self->out_port[ii]->share_buffer = FALSE;
              	  self->in_port[ii]->port_index = self->input_port_index[ii];
                  self->out_port[ii]->port_index = self->output_port_index[ii];

				  self->orderList[ii] = (guint*)g_malloc(sizeof(Olist));
		          self->orderList[ii]->idx = self->sinkpad[ii]->idx;
		          self->orderList[ii]->order = self->sinkpad[ii]->order;
            	}

                self->numEosPending = self->numpads;
#if 1
            	for(ii=0;ii<self->numpads;ii++)
            		for(kk=ii+1;kk<self->numpads;kk++)
            			if(self->orderList[kk]->order < self->orderList[ii]->order) {
                           tmp = *(self->orderList[kk]);
            			   *(self->orderList[kk]) = *(self->orderList[ii]);
            			   	*(self->orderList[ii]) = tmp;
            			}
            
            	printf("ip zorder - starting from lowest: %d",self->orderList[0]->idx);
            	for(ii=1;ii<self->numpads;ii++)
            		printf(", %d",self->orderList[ii]->idx);
            	printf("\n");
#endif					

			    pthread_create(&self->input_loop,NULL,vidmix_input_loop,(void*)self);
				self->ipCreated = TRUE;
	 	}
    g_mutex_unlock (self->ready_lock);
	//printf("pushing buffer:%p to queue:%p\n",buf,gst_pad_get_element_private(pad));

	//usleep(1000*1000);    
    async_queue_push (mixpad->queue, buf);
	//async_queue_push (self->queue[1], gst_buffer_ref(buf));
	
leave:

    GST_LOG_OBJECT (self, "end");
   // printf("leaving :%d!!\n",ch_info->idx);
    return ret;

    /* special conditions */
out_flushing:
    {
        const gchar *error_msg = NULL;
         printf("out flushing!!\n");
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
    GstOmxVideoMixer *self;
    GOmxCore *gomx;
    gboolean ret = TRUE;
	static int ii = 0;
	/*ip_params *ch_info;
	ch_info = (ip_params *)gst_pad_get_element_private(pad);*/

    self = GST_OMX_VIDEO_MIXER (GST_OBJECT_PARENT (pad));
    gomx = self->gomx;

    GST_INFO_OBJECT (self, "begin: event=%s", GST_EVENT_TYPE_NAME (event));

    switch (GST_EVENT_TYPE (event))
    {
        case GST_EVENT_EOS: 
			{
				GstVideoMixerPad *mixpad ;
				mixpad = GST_VIDEO_MIXER_PAD (pad);
           // printf ("Recieved EOS event, press <CTRL+C> to terminate pipeline.\n");
            /* if we are init'ed, and there is a running loop; then
             * if we get a buffer to inform it of EOS, let it handle the rest
             * in any other case, we send EOS */
             printf("eos received on pad:%d\n",mixpad->idx);
#if 0
            if (self->ready && self->last_pad_push_return == GST_FLOW_OK)
            {
                if (g_omx_port_send (self->in_port, event) >= 0)
                {
                    gst_event_unref (event);
                    break;
                }
            }
#endif
#if 0
            ch_info->eos = TRUE;
            g_mutex_lock (self->ready_lock);
			self->numEosPending--;
			printf("EOS pending - %d\n",self->numEosPending);
			g_mutex_unlock (self->ready_lock);
			if(self->numEosPending == 0) {
				printf("sending EOS from mixer!!\n");
				ret = gst_pad_push_event (self->srcpad, event);
			}
			else
				gst_event_unref (event);			
#else
            g_mutex_lock (self->ready_lock);
            self->numEosPending--;
			g_mutex_unlock (self->ready_lock);
			if(self->numEosPending == 0)
            /*if(self->eos == FALSE)*/ {
              self->eos = TRUE;
			  printf("eos pushed on pad:%d\n",mixpad->idx);
              ret = gst_pad_push_event (self->srcpad, event);
            } else
              gst_event_unref (event);

#endif

        	}
            break;

        case GST_EVENT_FLUSH_START:
            gst_pad_push_event (self->srcpad, event);
            self->last_pad_push_return = GST_FLOW_WRONG_STATE;

            g_omx_core_flush_start (gomx);

            gst_pad_pause_task (self->srcpad);

            ret = TRUE;
            break;

        case GST_EVENT_FLUSH_STOP:
            gst_pad_push_event (self->srcpad, event);
            self->last_pad_push_return = GST_FLOW_OK;

            g_omx_core_flush_stop (gomx);

            if (self->ready)
                gst_pad_start_task (self->srcpad, output_loop, self->srcpad);

            ret = TRUE;
            break;

        case GST_EVENT_NEWSEGMENT:
			if(ii == 0) {
            ret = gst_pad_push_event (self->srcpad, event);
			ii = 1;
				} else
				gst_event_unref(event);
            break;
		 case GST_EVENT_CROP:
            gst_event_unref(event);
            ret = TRUE;
			break;

        default:
            ret = gst_pad_push_event (self->srcpad, event);
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
    GstOmxVideoMixer *self;

    self = GST_OMX_VIDEO_MIXER (gst_pad_get_parent (pad));
    printf("Video mixer activate push!!\n");
    if (active)
    {
        GST_DEBUG_OBJECT (self, "activate");
        self->last_pad_push_return = GST_FLOW_OK;

        /* we do not start the task yet if the pad is not connected */
        if (gst_pad_is_linked (pad))
        {
            if (self->ready)
            {
              int ii;
                /** @todo link callback function also needed */
				for(ii = 0; ii < self->numpads; ii++) {
                g_omx_port_resume (self->in_port);
                g_omx_port_resume (self->out_port);
				}
                printf("Starting output loop\n");
                result = gst_pad_start_task (pad, output_loop, pad);
            }
        }
    }
    else
    {
        GST_DEBUG_OBJECT (self, "deactivate");

        if (self->ready)
        {
            int ii;
            /** @todo disable this until we properly reinitialize the buffers. */
#if 0
            /* flush all buffers */
            OMX_SendCommand (self->gomx->omx_handle, OMX_CommandFlush, OMX_ALL, NULL);
#endif

            /* unlock loops */
           for(ii = 0; ii < self->numpads; ii++) {
            g_omx_port_pause (self->in_port[ii]);
            g_omx_port_pause (self->out_port[ii]);
           }
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
    GstOmxVideoMixer *self = port->core->object;
    GstBuffer *buf;
    GstFlowReturn ret;
   //printf("here!!!!!!!!!!!!\n");
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
    //printf("allocating buffer!!\n");
    ret = gst_pad_alloc_buffer_and_set_caps (
            self->srcpad, GST_BUFFER_OFFSET_NONE,
            len, GST_PAD_CAPS (self->srcpad), &buf);

    if (ret == GST_FLOW_OK) return buf;

    return NULL;
}

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
sink_setcaps (GstPad *pad,
              GstCaps *caps)
{
    GstStructure *structure;
    GstOmxVideoMixer *self;
    GstOmxVideoMixer *omx_base;
    GOmxCore *gomx;
    GstVideoFormat format;
	GstVideoMixerPad *mixpad ;
	gchar *name;
    
    self = GST_OMX_VIDEO_MIXER (GST_PAD_PARENT (pad));
    omx_base = GST_OMX_VIDEO_MIXER (self);
    mixpad = GST_VIDEO_MIXER_PAD (pad);
    gomx = (GOmxCore *) omx_base->gomx;
    GST_INFO_OBJECT (self, "setcaps (sink): %" GST_PTR_FORMAT, caps);

   name = gst_caps_to_string(caps);
   printf("In sink set caps:%s\n",name);
   g_free(name);
    g_return_val_if_fail (caps, FALSE);
    g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);

    structure = gst_caps_get_structure (caps, 0);

    g_return_val_if_fail (structure, FALSE);

    if (!gst_video_format_parse_caps_strided (caps,
            &format, &mixpad->in_width, &mixpad->in_height, &mixpad->in_stride))
    {
        GST_WARNING_OBJECT (self, "width and/or height is not set in caps");
        return FALSE;
    }

    if (!mixpad->in_stride) 
    {
        mixpad->in_stride = gstomx_calculate_stride (mixpad->in_width, format);
    }
#if 0
    {
        const GValue *framerate = NULL;
        framerate = gst_structure_get_value (structure, "framerate");
        if (framerate)
        {
            self->framerate_num = gst_value_get_fraction_numerator (framerate);
            self->framerate_denom = gst_value_get_fraction_denominator (framerate);

           /* omx_base->duration = gst_util_uint64_scale_int(GST_SECOND,
                    gst_value_get_fraction_denominator (framerate),
                    gst_value_get_fraction_numerator (framerate));
            GST_DEBUG_OBJECT (self, "Nominal frame duration =%"GST_TIME_FORMAT,
                                GST_TIME_ARGS (omx_base->duration));*/
        }
    }
#endif
    self->duration = ((guint64)GST_SECOND/self->framerate_num);

    if (self->sink_setcaps)
        self->sink_setcaps (pad, caps);
   printf("ip width:%d, ip height: %d, ip stride:%d\n",mixpad->in_width,mixpad->in_height,
   	mixpad->in_stride);
    return gst_pad_set_caps (pad, caps);
}

static gboolean
src_setcaps (GstPad *pad, GstCaps *caps)
{
    GstOmxVideoMixer *self;
    GstOmxVideoMixer *omx_base;
    GstVideoFormat format;
    GstStructure *structure;
	gchar *name;

    self = GST_OMX_VIDEO_MIXER (GST_PAD_PARENT (pad));
    omx_base = GST_OMX_VIDEO_MIXER (self);
    structure = gst_caps_get_structure (caps, 0);

	name = gst_caps_to_string(caps);
   printf("In src set caps:%s\n",name);
   g_free(name);

    GST_INFO_OBJECT (omx_base, "setcaps (src): %" GST_PTR_FORMAT, caps);
    g_return_val_if_fail (caps, FALSE);
    g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);

    if (!gst_video_format_parse_caps_strided (caps,
            &format, &self->out_width, &self->out_height, &self->out_stride))
    {
        GST_WARNING_OBJECT (self, "width and/or height is not set in caps");
        return FALSE;
    }
    printf("set src_setcaps  height:%d, width:%d\n",self->out_height,self->out_width);
    if (!self->out_stride)
    {
        self->out_stride = gstomx_calculate_stride (self->out_width, format);
    }

    /* save the src caps later needed by omx transport buffer */
    if (omx_base->out_port[0]->caps)
        gst_caps_unref (omx_base->out_port[0]->caps);

    omx_base->out_port[0]->caps = gst_caps_copy (caps);

    return TRUE;
}

static GstPad *request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * req_name)
{
	GstOmxVideoMixer *mix = NULL;
	GstVideoMixerPad *mixpad = NULL;
	GstElementClass *klass = GST_ELEMENT_GET_CLASS (element);
	printf("request pad!!\n");
	g_return_val_if_fail (templ != NULL, NULL);
	
	if (G_UNLIKELY (templ->direction != GST_PAD_SINK)) {
	  g_warning ("gstomx_videomixer: request pad that is not a SINK pad");
	  return NULL;
	}
	
	g_return_val_if_fail (GST_IS_OMX_VIDEO_MIXER (element), NULL);
	
	mix = GST_OMX_VIDEO_MIXER (element);
	
	if (templ == gst_element_class_get_pad_template (klass, "sink")) {
	  gint serial = 0;
	  gchar *name = NULL;
	
	  //GST_VIDEO_MIXER_STATE_LOCK (mix);
	  if (req_name == NULL || strlen (req_name) < 6
		  || !g_str_has_prefix (req_name, "sink_")) {
		/* no name given when requesting the pad, use next available int */
		serial = mix->next_sinkpad++;
	  } else {
		/* parse serial number from requested padname */
		serial = atoi (&req_name[5]);
		if (serial >= mix->next_sinkpad)
		  mix->next_sinkpad = serial + 1;
	  }
	  /* create new pad with the name */
	  name = g_strdup_printf ("sink_%02d", serial);
	  printf("creating pad with name:%s\n",name);
	  mixpad = g_object_new (GST_TYPE_VIDEO_MIXER_PAD, "name", name, "direction",
		  templ->direction, "template", templ, NULL);
	  g_free (name);

	  mixpad->idx       = mix->numpads;
	  mixpad->order     = 0;
	  mixpad->outX      = -1;
	  mixpad->outY      = -1;
	  mixpad->queue     = async_queue_new ();
	  mixpad->eos       = FALSE;
	  mixpad->lastBuf   = NULL;
	  mixpad->outWidth  = -1;
	  mixpad->outHeight = -1;
	  mixpad->in_width  = 0;
	  mixpad->in_height = 0;
	  mixpad->in_stride = 0;
	  mixpad->inX       = 0;
	  mixpad->inY       = 0;
	  mixpad->cropWidth = -1;
	  mixpad->cropHeight = -1;
	  	
	  gst_pad_set_chain_function (mixpad,GST_DEBUG_FUNCPTR( pad_chain));
	  gst_pad_set_event_function (mixpad,GST_DEBUG_FUNCPTR( pad_event));
	  gst_pad_set_setcaps_function (mixpad,GST_DEBUG_FUNCPTR (sink_setcaps));
	  printf("Setting sink pad:%d\n",mix->numpads);
	  mix->sinkpad[mix->numpads] = mixpad;
	//  GST_VIDEO_MIXER_STATE_UNLOCK (mix);
	} else {
	  g_warning ("gstomx_videomixer: this is not our template!");
	  return NULL;
	}
	
	/* add the pad to the element */
	gst_element_add_pad (element, GST_PAD (mixpad));
	mix->numpads++;
	gst_child_proxy_child_added (GST_OBJECT (mix), GST_OBJECT (mixpad));
	printf("request pad done ret!!\n");
	return GST_PAD (mixpad);

}

static void
release_pad (GstElement * element, GstPad * pad)
{
  GstOmxVideoMixer *mix = NULL;
  GstVideoMixerPad *mixpad;

  mix = GST_OMX_VIDEO_MIXER (element);
  //GST_VIDEO_MIXER_STATE_LOCK (mix);
  /*if (G_UNLIKELY (g_slist_find (mix->sinkpads, pad) == NULL)) {
    g_warning ("Unknown pad %s", GST_PAD_NAME (pad));
    goto error;
  }*/

  mixpad = GST_VIDEO_MIXER_PAD (pad);

  //mix->sinkpads = g_slist_remove (mix->sinkpads, pad);
  //gst_videomixer_collect_free (mixpad->mixcol);
  //gst_collect_pads_remove_pad (mix->collect, pad);
  gst_child_proxy_child_removed (GST_OBJECT (mix), GST_OBJECT (mixpad));
  /* determine possibly new geometry and master */
  //gst_videomixer_set_master_geometry (mix);
  mix->numpads--;
  //GST_VIDEO_MIXER_STATE_UNLOCK (mix);

  gst_element_remove_pad (element, pad);
  return;
}


static void
type_instance_init (GTypeInstance *instance,
                    gpointer g_class)
{
    GstOmxVideoMixer *self;
    GstElementClass *element_class;
    GstOmxVideoMixerClass *bclass;
    GstPadTemplate * templ;
	GstVideoMixerPad *mixpad;
	
    element_class = GST_ELEMENT_CLASS (g_class);
    bclass = GST_OMX_VIDEO_MIXER_CLASS (g_class);

    self = GST_OMX_VIDEO_MIXER (instance);
    GST_LOG_OBJECT (self, "begin");

    /* GOmx */
    self->gomx = g_omx_core_new (self, g_class);

    self->ipCreated = FALSE;	
	self->eos = FALSE;
    self->ready_lock = g_mutex_new ();
	self->loop_lock = g_mutex_new ();

    self->srcpad =
        gst_pad_new_from_template (gst_element_class_get_pad_template (element_class, "src"), "src");

    gst_pad_set_activatepush_function (self->srcpad, activate_push);

    gst_pad_use_fixed_caps (self->srcpad);

    gst_element_add_pad (GST_ELEMENT (self), self->srcpad);

    gst_pad_set_setcaps_function (self->srcpad,
            GST_DEBUG_FUNCPTR (src_setcaps));
	
	self->framerate_num = 15;
    self->framerate_denom = 1;
	self->timestamp = 0;
	self->next_sinkpad = 0;
	self->numpads = 0;
	self->outbufsize = 0;
	/*printf("duration:%lld, in time : %" 
                    GST_TIME_FORMAT "\n",self->duration,GST_TIME_ARGS(self->duration));
    printf("In instance init/...done!!\n");*/
    GST_LOG_OBJECT (self, "end");
}

static GstObject *
gst_omx_videomixer_child_proxy_get_child_by_index (GstChildProxy * child_proxy,
    guint index)
{
  GstOmxVideoMixer *mix = GST_OMX_VIDEO_MIXER (child_proxy);
  GstObject *obj;

  //GST_VIDEO_MIXER_STATE_LOCK (mix);
  //printf("index requested:%d\n",index);
  obj = mix->sinkpad[index];
  if (obj)
    gst_object_ref (obj);
  //GST_VIDEO_MIXER_STATE_UNLOCK (mix);
  return obj;
}

static guint
gst_omx_videomixer_child_proxy_get_children_count (GstChildProxy * child_proxy)
{
  guint count = 0;
  GstOmxVideoMixer *mix = GST_OMX_VIDEO_MIXER (child_proxy);

  //GST_VIDEO_MIXER_STATE_LOCK (mix);
  count = mix->numpads;
  //GST_VIDEO_MIXER_STATE_UNLOCK (mix);
  GST_INFO_OBJECT (mix, "Children Count: %d", count);
  return count;
}

static void
gst_omx_videomixer_child_proxy_init (gpointer g_iface, gpointer iface_data)
{
  GstChildProxyInterface *iface = g_iface;

  GST_INFO ("intializing child proxy interface");
  iface->get_child_by_index = gst_omx_videomixer_child_proxy_get_child_by_index;
  iface->get_children_count = gst_omx_videomixer_child_proxy_get_children_count;
}


static void
init_interfaces (GType object_type)
{
 static const GInterfaceInfo child_proxy_info = {
    (GInterfaceInitFunc) gst_omx_videomixer_child_proxy_init,
    NULL,
    NULL
  };

  g_type_add_interface_static (object_type, GST_TYPE_CHILD_PROXY,
      &child_proxy_info);
  GST_INFO ("GstChildProxy interface registered");
}

