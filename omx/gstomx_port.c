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

#include <string.h>

#include "gstomx_util.h"
#include "gstomx_port.h"
#include "gstomx.h"
#include "gstomx_base_filter.h"
#include "gstomx_base_videodec.h"
#include "gstomx_buffertransport.h"

#ifdef USE_OMXTICORE
#  include <OMX_TI_Common.h>
#  include <OMX_TI_Index.h>
#endif

GST_DEBUG_CATEGORY_EXTERN (gstomx_util_debug);

#ifndef OMX_BUFFERFLAG_CODECCONFIG
#  define OMX_BUFFERFLAG_CODECCONFIG 0x00000080 /* special nFlags field to use to indicated codec-data */
#endif

static OMX_BUFFERHEADERTYPE * request_buffer (GOmxPort *port);
static void release_buffer (GOmxPort *port, OMX_BUFFERHEADERTYPE *omx_buffer);
static void setup_shared_buffer (GOmxPort *port, OMX_BUFFERHEADERTYPE *omx_buffer);

#define DEBUG(port, fmt, args...) \
    GST_DEBUG ("<%s:%s> "fmt, GST_OBJECT_NAME ((port)->core->object), (port)->name, ##args)
#define LOG(port, fmt, args...) \
    GST_LOG ("<%s:%s> "fmt, GST_OBJECT_NAME ((port)->core->object), (port)->name, ##args)
#define WARNING(port, fmt, args...) \
    GST_WARNING ("<%s:%s> "fmt, GST_OBJECT_NAME ((port)->core->object), (port)->name, ##args)

/*
 * Port
 */

GOmxPort *
g_omx_port_new (GOmxCore *core, const gchar *name, guint index)
{
    GOmxPort *port = g_new0 (GOmxPort, 1);

    port->core = core;
    port->name = g_strdup_printf ("%s:%d", name, index);
    port->port_index = index;
    port->num_buffers = 0;
    port->buffers = NULL;

    port->enabled = TRUE;
    port->queue = async_queue_new ();
    port->mutex = g_mutex_new ();
	port->cond  = g_cond_new();

    port->ignore_count = 0;
    port->n_offset = 0;
    port->vp6_hack = FALSE;

	port->portptr = gst_omxportptr_new(port);

    return port;
}


void
g_omx_port_free (GOmxPort *port)
{

    DEBUG (port, "begin");

	gst_omxportptr_unref(port->portptr);

    g_mutex_free (port->mutex);
	g_cond_free(port->cond);
    async_queue_free (port->queue);

    g_free (port->name);

    g_free (port->buffers);
    g_free (port);

    GST_DEBUG ("end");
}

void
g_omx_port_setup (GOmxPort *port,
                  OMX_PARAM_PORTDEFINITIONTYPE *omx_port)
{
    GOmxPortType type = -1;

    switch (omx_port->eDir)
    {
        case OMX_DirInput:
            type = GOMX_PORT_INPUT;
            break;
        case OMX_DirOutput:
            type = GOMX_PORT_OUTPUT;
            break;
        default:
            break;
    }

    port->type = type;
    /** @todo should it be nBufferCountMin? */
    port->num_buffers = omx_port->nBufferCountActual;
    port->port_index = omx_port->nPortIndex;

    DEBUG (port, "type=%d, num_buffers=%d, port_index=%d",
        port->type, port->num_buffers, port->port_index);

    /* I don't think it is valid for buffers to be allocated at this point..
     * if there is a case where it is, then call g_omx_port_free_buffers()
     * here instead:
     */
    g_return_if_fail (!port->buffers);
}

static GstBuffer *
buffer_alloc (GOmxPort *port, gint len)
{
    GstBuffer *buf = NULL;

    if (port->buffer_alloc)
        buf = port->buffer_alloc (port, len);

    if (!buf)
        buf = gst_buffer_new_and_alloc (len);

    return buf;
}


/**
 * Ensure that srcpad caps are set before beginning transition-to-idle or
 * transition-to-loaded.  This is a bit ugly, because it requires pad-alloc'ing
 * a buffer from the downstream element for no particular purpose other than
 * triggering upstream caps negotiation from the sink..
 */
void
g_omx_port_prepare (GOmxPort *port)
{
    OMX_PARAM_PORTDEFINITIONTYPE param;
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

    /* number of buffers could have changed */
    G_OMX_PORT_GET_DEFINITION (port, &param);
    port->num_buffers = param.nBufferCountActual;

    gst_buffer_unref (buf);

/* REVISIT: In WBU code these macros are implemented in OMX_TI_Core.h and EZSDK is missing it hence
   commenting out code for now
  */
#if 0
#ifdef USE_OMXTICORE
    if (port->share_buffer)
    {
        OMX_TI_PARAM_BUFFERPREANNOUNCE param;
        OMX_TI_CONFIG_BUFFERREFCOUNTNOTIFYTYPE config;

        G_OMX_PORT_GET_PARAM (port, OMX_TI_IndexParamBufferPreAnnouncement, &param);
        param.bEnabled = FALSE;
        G_OMX_PORT_SET_PARAM (port, OMX_TI_IndexParamBufferPreAnnouncement, &param);

        G_OMX_PORT_GET_CONFIG (port, OMX_TI_IndexConfigBufferRefCountNotification, &config);
        config.bNotifyOnDecrease = TRUE;
        config.bNotifyOnIncrease = FALSE;
        config.nCountForNotification = 1;
        G_OMX_PORT_SET_CONFIG (port, OMX_TI_IndexConfigBufferRefCountNotification, &config);
    }
#endif
#endif
    DEBUG (port, "end");
}

void
g_omx_port_allocate_buffers (GOmxPort *port)
{
    OMX_PARAM_PORTDEFINITIONTYPE param;
    guint i;
    guint size;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    if (port->buffers)
        return;

    DEBUG (port, "begin");

    G_OMX_PORT_GET_DEFINITION (port, &param);
    size = param.nBufferSize;

    port->buffers = g_new0 (OMX_BUFFERHEADERTYPE *, port->num_buffers);

    for (i = 0; i < port->num_buffers; i++)
    {

        if (port->omx_allocate)
        {
           
            DEBUG (port, "%d: OMX_AllocateBuffer(), size=%d", i, size);
            eError =  OMX_AllocateBuffer (port->core->omx_handle,
                                &port->buffers[i],
                                port->port_index,
                                NULL,
                                size);
            if (eError != OMX_ErrorNone) {
               DEBUG (port, "%d: OMX_AllocateBuffer(), returned=%x", eError);
           }

            g_return_if_fail (port->buffers[i]);
        }
        else
        {
            gpointer buffer_data = NULL;

            if (!port->always_copy)
            {
				buffer_data = port->share_buffer_info->pBuffer[i];
            }
            else if (! port->share_buffer)
            {
                buffer_data = g_malloc (size);
            }

            DEBUG (port, "%d: OMX_UseBuffer(), size=%d, share_buffer=%d", i, size, port->share_buffer);
            OMX_UseBuffer (port->core->omx_handle,
                           &port->buffers[i],
                           port->port_index,
                           NULL,
                           size,
                           buffer_data);

            g_return_if_fail (port->buffers[i]);

            if (port->share_buffer)
            {
                /* we will need this later: */
                port->buffers[i]->nAllocLen = size;
            }
        }
    }
    
    DEBUG (port, "end");
}

void
g_omx_port_free_buffers (GOmxPort *port)
{
    guint i;

    if (!port->buffers)
        return;

    DEBUG (port, "begin");

	gst_omxportptr_mutex_lock(port->portptr);

    for (i = 0; i < port->num_buffers; i++)
    {
        OMX_BUFFERHEADERTYPE *omx_buffer;

        /* pop the buffer, to be sure that it has been returned from the
         * OMX component, to avoid freeing a buffer that the component
         * is still accessing:
         */
		#if 0
        omx_buffer = async_queue_pop_full (port->queue, TRUE, TRUE);
		#else
		omx_buffer = port->buffers[i];
		#endif

        if (omx_buffer)
        {
#if 0
            /** @todo how shall we free that buffer? */
            if (!port->omx_allocate)
            {
                g_free (omx_buffer->pBuffer);
                omx_buffer->pBuffer = NULL;
            }
#endif

            DEBUG (port, "OMX_FreeBuffer(%p)", omx_buffer);
            OMX_FreeBuffer (port->core->omx_handle, port->port_index, omx_buffer);
            port->buffers[i] = NULL;
        }
    }

    g_free (port->buffers);
    port->buffers = NULL;
	port->portptr->port = NULL;
	gst_omxportptr_mutex_unlock(port->portptr);

    DEBUG (port, "end");
}

void
g_omx_port_start_buffers (GOmxPort *port)
{
    guint i;

    if (!port->enabled)
        return;

    g_return_if_fail (port->buffers);

    DEBUG (port, "begin");

    for (i = 0; i < port->num_buffers; i++)
    {
        OMX_BUFFERHEADERTYPE *omx_buffer;

        omx_buffer = port->buffers[i];

        /* If it's an input port we will need to fill the buffer, so put it in
         * the queue, otherwise send to omx for processing (fill it up). */
        if (port->type == GOMX_PORT_INPUT)
        {
            /*if (port->always_copy)*/
                g_omx_core_got_buffer (port->core, port, omx_buffer);
        }
        else
        {
            setup_shared_buffer (port, omx_buffer);
            release_buffer (port, omx_buffer);
        }
    }

    DEBUG (port, "end");
}

void
g_omx_port_push_buffer (GOmxPort *port,
                        OMX_BUFFERHEADERTYPE *omx_buffer)
{
    if (!port->always_copy && omx_buffer->pAppPrivate)
    {
		/* Avoid a race condition of pAppPrivate getting set to null 
		   after the buffer is submitted back again */
		OMX_PTR appPrivate = omx_buffer->pAppPrivate;
        //omx_buffer->pAppPrivate = NULL;
    	g_mutex_lock(port->mutex);
        GST_BUFFER_FLAG_UNSET(appPrivate,GST_BUFFER_FLAG_BUSY);
		gst_buffer_unref (appPrivate);
		g_cond_signal(port->cond);
		g_mutex_unlock(port->mutex);
    } else
       async_queue_push (port->queue, omx_buffer);
}

static gint
omxbuffer_index (GOmxPort *port, OMX_U8 *pBuffer)
{
    int i;
    
    for (i=0; i < port->num_buffers; i++) 
        if (port->buffers[i]->pBuffer == pBuffer)
            return i;

    return -1; 
}


static OMX_BUFFERHEADERTYPE *
request_buffer (GOmxPort *port)
{
    LOG (port, "request buffer");
    return async_queue_pop (port->queue);
}

static void
release_buffer (GOmxPort *port, OMX_BUFFERHEADERTYPE *omx_buffer)
{
    
    OMX_ERRORTYPE eError = OMX_ErrorNone;


    switch (port->type)
    {
        case GOMX_PORT_INPUT:
            DEBUG (port, "ETB: omx_buffer=%p, pAppPrivate=%p, pBuffer=%p",
                    omx_buffer, omx_buffer ? omx_buffer->pAppPrivate : 0, omx_buffer ? omx_buffer->pBuffer : 0);
            if(omx_buffer->nFilledLen != 0) {
               eError = OMX_EmptyThisBuffer (port->core->omx_handle, omx_buffer);
               if (eError != OMX_ErrorNone) {
                  DEBUG (port, "Empty this buffer returned eError =%x",eError);
               }
	    }
            else{
               DEBUG (port, "filled length is zero put back into queue ");
               g_omx_port_push_buffer(port, omx_buffer);
	    }
            break;
        case GOMX_PORT_OUTPUT:
            DEBUG (port, "FTB: omx_buffer=%p, pAppPrivate=%p, pBuffer=%p",
                    omx_buffer, omx_buffer ? omx_buffer->pAppPrivate : 0, omx_buffer ? omx_buffer->pBuffer : 0);
            eError = OMX_FillThisBuffer (port->core->omx_handle, omx_buffer);
            if (eError != OMX_ErrorNone) {
           
           }
            break;
         default:
            break;
    }

  
   
}

/* NOTE ABOUT BUFFER SHARING:
 *
 * Buffer sharing is a sort of "extension" to OMX to allow zero copy buffer
 * passing between GST and OMX.
 *
 * There are only two cases:
 *
 * 1) shared_buffer is enabled, in which case we control nOffset, and use
 *    pAppPrivate to store the reference to the original GstBuffer that
 *    pBuffer ptr is copied from.  Note that in case of input buffers,
 *    the DSP/coprocessor should treat the buffer as read-only so cache-
 *    line alignment is not an issue.  For output buffers which are not
 *    pad_alloc()d, some care may need to be taken to ensure proper buffer
 *    alignment.
 * 2) shared_buffer is not enabled, in which case we respect the nOffset
 *    set by the component and pAppPrivate is NULL
 *
 */

static void
setup_shared_buffer (GOmxPort *port, OMX_BUFFERHEADERTYPE *omx_buffer)
{
    if (port->share_buffer)
    {
        GstBuffer *new_buf = buffer_alloc (port, omx_buffer->nAllocLen);

        omx_buffer->pAppPrivate = new_buf;
        omx_buffer->pBuffer     = GST_BUFFER_DATA (new_buf);
        omx_buffer->nAllocLen   = GST_BUFFER_SIZE (new_buf);
        omx_buffer->nOffset     = 0;
        omx_buffer->nFlags      = 0;

        /* special hack.. this should be removed: */
        omx_buffer->nFlags     |= OMX_BUFFERHEADERFLAG_MODIFIED;
    }
    else
    {
        g_assert (omx_buffer->pBuffer && !omx_buffer->pAppPrivate);
    }
}

typedef void (*SendPrep) (GOmxPort *port, OMX_BUFFERHEADERTYPE *omx_buffer, gpointer obj);

static void
send_prep_codec_data (GOmxPort *port, OMX_BUFFERHEADERTYPE *omx_buffer, GstBuffer *buf)
{
    omx_buffer->nFlags |= OMX_BUFFERFLAG_CODECCONFIG;
    omx_buffer->nFilledLen = GST_BUFFER_SIZE (buf);

    if (port->share_buffer)
    {
        omx_buffer->nOffset = 0;
        omx_buffer->pBuffer = malloc (omx_buffer->nFilledLen);
    }

    if (port->always_copy) 
    {
        memcpy (omx_buffer->pBuffer + omx_buffer->nOffset,
            GST_BUFFER_DATA (buf), omx_buffer->nFilledLen);
    }
}

static void
send_prep_buffer_data (GOmxPort *port, OMX_BUFFERHEADERTYPE *omx_buffer, GstBuffer *buf)
{
    if (port->share_buffer)
    {
        omx_buffer->nOffset     = port->n_offset;
        omx_buffer->pBuffer     = GST_BUFFER_DATA (buf);
        omx_buffer->nFilledLen  = GST_BUFFER_SIZE (buf);
        /* Temp hack to not update nAllocLen for each ETB/FTB till we
         * find a cleaner solution to get padded width and height */
        /* omx_buffer->nAllocLen   = GST_BUFFER_SIZE (buf); */
        omx_buffer->pAppPrivate = gst_buffer_ref (buf);

        /* special hack.. this should be removed: */
        omx_buffer->nFlags     |= OMX_BUFFERHEADERFLAG_MODIFIED;
    }
    else
    {
        if (port->always_copy)
        {
            omx_buffer->nFilledLen = MIN (GST_BUFFER_SIZE (buf),
                omx_buffer->nAllocLen - omx_buffer->nOffset);
			//printf("filled len:%d, buffer size:%d\n",omx_buffer->nFilledLen,GST_BUFFER_SIZE (buf));
			//printf("alloclen:%d, offset:%d\n",omx_buffer->nAllocLen,omx_buffer->nOffset);
        }

        if (G_UNLIKELY (port->vp6_hack))
        {
            DEBUG (port, "VP6 hack begin");

            DEBUG (port, "Adding the frame-length");
            memcpy (omx_buffer->pBuffer, &(omx_buffer->nFilledLen), 4);

            DEBUG (port, "memcpy the vp6 data");
            memcpy (omx_buffer->pBuffer + 4, GST_BUFFER_DATA (buf),
                    omx_buffer->nFilledLen);

            DEBUG (port, "add four bytes to nFilledLen");
            omx_buffer->nFilledLen += 4;

            DEBUG (port, "VP6 hack end");
        }
        else
        {
            if (port->always_copy) 
            {
                memcpy (omx_buffer->pBuffer + omx_buffer->nOffset,
                    GST_BUFFER_DATA (buf), omx_buffer->nFilledLen);
            }
        }
    }

    if (port->core->use_timestamps)
	{
		if (GST_CLOCK_TIME_NONE != GST_BUFFER_TIMESTAMP (buf)) {
			omx_buffer->nTimeStamp = gst_util_uint64_scale_int (
					GST_BUFFER_TIMESTAMP (buf),
					OMX_TICKS_PER_SECOND, GST_SECOND);
		} else {
			omx_buffer->nTimeStamp = (OMX_TICKS)-1;
		}
	}

    DEBUG (port, "omx_buffer: size=%lu, len=%lu, flags=%lu, offset=%lu, timestamp=%lld",
            omx_buffer->nAllocLen, omx_buffer->nFilledLen, omx_buffer->nFlags,
            omx_buffer->nOffset, omx_buffer->nTimeStamp);
}

/*wmv Send prepare start*/
static void
send_prep_wmv_codec_data (GOmxPort *port, OMX_BUFFERHEADERTYPE *omx_buffer, GstBuffer *buf)
{
   
    omx_buffer->nFlags |= OMX_BUFFERFLAG_CODECCONFIG;

    omx_buffer->nFilledLen = 0;//GST_BUFFER_SIZE (buf);


    if (port->share_buffer)
    {
        omx_buffer->nOffset = 0;
        omx_buffer->pBuffer = malloc (omx_buffer->nFilledLen);
    }

    if (port->always_copy) 
    {
       
         memcpy (omx_buffer->pBuffer + omx_buffer->nOffset,
            GST_BUFFER_DATA (buf), omx_buffer->nFilledLen);
    }

   
}

static void
send_prep_wmv_buffer_data (GOmxPort *port, OMX_BUFFERHEADERTYPE *omx_buffer, GstBuffer *buf)
{
   

    GstOmxBaseFilter *self = port->core->object;

    if (port->share_buffer)
    {
        omx_buffer->nOffset     = port->n_offset;
        omx_buffer->pBuffer     = GST_BUFFER_DATA (buf);
        omx_buffer->nFilledLen  = GST_BUFFER_SIZE (buf);
        /* Temp hack to not update nAllocLen for each ETB/FTB till we
         * find a cleaner solution to get padded width and height */
        /* omx_buffer->nAllocLen   = GST_BUFFER_SIZE (buf); */
        omx_buffer->pAppPrivate = gst_buffer_ref (buf);

        /* special hack.. this should be removed: */
        omx_buffer->nFlags     |= OMX_BUFFERHEADERFLAG_MODIFIED;
    }
    else
    {
        if (port->always_copy && self->codec_data) 
        {
            omx_buffer->nFilledLen = MIN (GST_BUFFER_SIZE (buf) + GST_BUFFER_SIZE(self->codec_data),
            omx_buffer->nAllocLen - omx_buffer->nOffset) ;
            memcpy (omx_buffer->pBuffer + omx_buffer->nOffset,
            GST_BUFFER_DATA (self->codec_data), GST_BUFFER_SIZE(self->codec_data));
            memcpy (omx_buffer->pBuffer + omx_buffer->nOffset + GST_BUFFER_SIZE(self->codec_data),
            GST_BUFFER_DATA (buf), omx_buffer->nFilledLen - GST_BUFFER_SIZE(self->codec_data));
            if(GST_BUFFER_SIZE(self->codec_data) == 36){
            	self->codec_data = NULL;/*To be done, making null disables memcpy of configdata to every buffer*/
            }
        }
        else
	{
            if (port->always_copy) 
            {
                omx_buffer->nFilledLen = MIN (GST_BUFFER_SIZE (buf),
                omx_buffer->nAllocLen - omx_buffer->nOffset);
                memcpy (omx_buffer->pBuffer + omx_buffer->nOffset,
                GST_BUFFER_DATA (buf), omx_buffer->nFilledLen);
            }
         }
    }

    if (port->core->use_timestamps)
    {
        omx_buffer->nTimeStamp = gst_util_uint64_scale_int (
                GST_BUFFER_TIMESTAMP (buf),
                OMX_TICKS_PER_SECOND, GST_SECOND);
		       
    }

    DEBUG (port, "omx_buffer: size=%lu, len=%lu, flags=%lu, offset=%lu, timestamp=%lld",
            omx_buffer->nAllocLen, omx_buffer->nFilledLen, omx_buffer->nFlags,
            omx_buffer->nOffset, omx_buffer->nTimeStamp);
   
}
/*wmv sendprepare end*/

static void
send_prep_mpeg4_buffer_data (GOmxPort *port, OMX_BUFFERHEADERTYPE *omx_buffer, GstBuffer *buf)
{
   

    GstOmxBaseFilter *self = port->core->object;

    if (port->share_buffer)
    {
        omx_buffer->nOffset     = port->n_offset;
        omx_buffer->pBuffer     = GST_BUFFER_DATA (buf);
        omx_buffer->nFilledLen  = GST_BUFFER_SIZE (buf);
        /* Temp hack to not update nAllocLen for each ETB/FTB till we
         * find a cleaner solution to get padded width and height */
        /* omx_buffer->nAllocLen   = GST_BUFFER_SIZE (buf); */
        omx_buffer->pAppPrivate = gst_buffer_ref (buf);

        /* special hack.. this should be removed: */
        omx_buffer->nFlags     |= OMX_BUFFERHEADERFLAG_MODIFIED;
    }
    else
    {
        if (port->always_copy && self->codec_data) 
        {
            omx_buffer->nFilledLen = MIN (GST_BUFFER_SIZE (buf) + GST_BUFFER_SIZE(self->codec_data),
            omx_buffer->nAllocLen - omx_buffer->nOffset) ;
            memcpy (omx_buffer->pBuffer + omx_buffer->nOffset,
            GST_BUFFER_DATA (self->codec_data), GST_BUFFER_SIZE(self->codec_data));
            memcpy (omx_buffer->pBuffer + omx_buffer->nOffset + GST_BUFFER_SIZE(self->codec_data),
            GST_BUFFER_DATA (buf), omx_buffer->nFilledLen - GST_BUFFER_SIZE(self->codec_data));
        }
        else
	{
            if (port->always_copy) 
            {
                omx_buffer->nFilledLen = MIN (GST_BUFFER_SIZE (buf),
                omx_buffer->nAllocLen - omx_buffer->nOffset);
                memcpy (omx_buffer->pBuffer + omx_buffer->nOffset,
                GST_BUFFER_DATA (buf), omx_buffer->nFilledLen);
            }
         }
    }

    if (port->core->use_timestamps)
    {
        omx_buffer->nTimeStamp = gst_util_uint64_scale_int (
                GST_BUFFER_TIMESTAMP (buf),
                OMX_TICKS_PER_SECOND, GST_SECOND);
		       
    }

    DEBUG (port, "omx_buffer: size=%lu, len=%lu, flags=%lu, offset=%lu, timestamp=%lld",
            omx_buffer->nAllocLen, omx_buffer->nFilledLen, omx_buffer->nFlags,
            omx_buffer->nOffset, omx_buffer->nTimeStamp);
   
}
/*wmv sendprepare end*/



static void
send_prep_eos_event (GOmxPort *port, OMX_BUFFERHEADERTYPE *omx_buffer, GstEvent *evt)
{
    omx_buffer->nFlags |= OMX_BUFFERFLAG_EOS;
    omx_buffer->nFilledLen = 0;
    if (port->share_buffer) {
        /* OMX should not try to read from the buffer, since it is empty..
         * but yet it complains if pBuffer is NULL.  This will get us past
         * that check, and ensure that OMX segfaults in a debuggible way
         * if they do something stupid like read from the empty buffer:
         */
        omx_buffer->pBuffer    = (OMX_U8 *)1;
        /* TODO: Temporary hack as OMX currently complains about
         * non-zero nAllocLen. Need to be removed once aligned with OMX.
         */
        /* omx_buffer->nAllocLen  = 0; */
    }
}

/* we are configured not copy the input buffer then update the pBuffer
 * to point to the recieved buffer and save the gstreamer buffer reference
 * in pAppPrivate so that it can be later freed up.
 */
static OMX_BUFFERHEADERTYPE *
get_input_buffer_header (GOmxPort *port, GstBuffer *src)
{
    OMX_BUFFERHEADERTYPE *omx_buffer,*tmp;
    int index;

    index = omxbuffer_index(port, GST_BUFFER_DATA (src));
     
    omx_buffer = port->buffers[index];

    omx_buffer->pBuffer = GST_BUFFER_DATA(src);
	tmp = GST_GET_OMXBUFFER(src);
	if(tmp)
        omx_buffer->nOffset = tmp->nOffset;
	else
		omx_buffer->nOffset = 0;
    omx_buffer->nFilledLen = GST_BUFFER_SIZE (src);
    omx_buffer->pAppPrivate = gst_buffer_ref (src);

    return omx_buffer;
}

/**
 * Sends a buffer to the OMX component 2-fields separately
 */
gint g_omx_port_send_interlaced_fields(GOmxPort *port, GstBuffer *buf, gint second_field_offset)
{
	OMX_BUFFERHEADERTYPE *out1, *out2, *in, *first, *second;
	gint ret;
	OMX_U8 *pBuffer;
	int index;

	if (G_UNLIKELY((!GST_IS_OMXBUFFERTRANSPORT (buf)) || port->always_copy)) {
		GST_ERROR_OBJECT(port->core->object,"Unexpected !!\n");
		return -1; /* something went wrong */
	}

	in = GST_GET_OMXBUFFER(buf);

	pBuffer = GST_BUFFER_DATA(buf);
    index = omxbuffer_index(port, pBuffer);
    out1 = port->buffers[index];
    index = omxbuffer_index(port, pBuffer + second_field_offset);
    out2 = port->buffers[index];

    out1->pBuffer = pBuffer;
    out2->pBuffer = out1->pBuffer + second_field_offset;
    out1->nOffset = out2->nOffset = in->nOffset;
    out1->nFlags = OMX_TI_BUFFERFLAG_VIDEO_FRAME_TYPE_INTERLACE;
	out2->nFlags = out1->nFlags | OMX_TI_BUFFERFLAG_VIDEO_FRAME_TYPE_INTERLACE_BOTTOM;
    ret = out1->nFilledLen = in->nFilledLen;
    out2->nFilledLen = (((in->nFilledLen + in->nOffset) *3) >> 2) - in->nOffset;
    out1->pAppPrivate = gst_buffer_ref(buf);
    out2->pAppPrivate = gst_buffer_ref(buf);

	if (in->nFlags & OMX_TI_BUFFERFLAG_VIDEO_FRAME_TYPE_INTERLACE_TOP_FIRST) {
		first = out1; second = out2; }
	else { first = out2; second = out1; }

	if (port->core->use_timestamps)	{
		if (GST_CLOCK_TIME_NONE != GST_BUFFER_TIMESTAMP (buf)) {
			first->nTimeStamp = gst_util_uint64_scale_int (
					GST_BUFFER_TIMESTAMP (buf),
					OMX_TICKS_PER_SECOND, GST_SECOND);
		} else {
			first->nTimeStamp = (OMX_TICKS)-1;
		}
	}
	// Timestamp for the second field comes from adding duration to the
	// First field timestamp
	second->nTimeStamp = (OMX_TICKS)-1;
	release_buffer (port, first);
	release_buffer (port, second);
	return ret;
}

/**
 * Send a buffer/event to the OMX component.  This handles conversion of
 * GST buffer, codec-data, and EOS events to the equivalent OMX buffer.
 *
 * This method does not take ownership of the ref to @obj
 *
 * Returns number of bytes sent, or negative if error
 */
gint
g_omx_port_send (GOmxPort *port, gpointer obj)
{

	SendPrep send_prep = NULL;

	g_return_val_if_fail (port->type == GOMX_PORT_INPUT, -1);


	GstOmxBaseVideoDec *self = GST_OMX_BASE_VIDEODEC (port->core->object);;

	if (GST_IS_BUFFER (obj))
	{   
		if(self->compression_format == OMX_VIDEO_CodingWMV)
		{

			if (G_UNLIKELY (GST_BUFFER_FLAG_IS_SET (obj, GST_BUFFER_FLAG_IN_CAPS))) 
				send_prep = (SendPrep)send_prep_wmv_codec_data;            
			else
				send_prep = (SendPrep)send_prep_wmv_buffer_data;
		}
		else
			if(self->compression_format == OMX_VIDEO_CodingMPEG4)
			{

				if (G_UNLIKELY (GST_BUFFER_FLAG_IS_SET (obj, GST_BUFFER_FLAG_IN_CAPS))) 
					send_prep = (SendPrep)send_prep_wmv_codec_data;            
				else
					send_prep = (SendPrep)send_prep_mpeg4_buffer_data;
			}
			else
			{
				if (G_UNLIKELY (GST_BUFFER_FLAG_IS_SET (obj, GST_BUFFER_FLAG_IN_CAPS))) 
					send_prep = (SendPrep)send_prep_codec_data;            
				else
					send_prep = (SendPrep)send_prep_buffer_data;
			}
	}
	else if (GST_IS_EVENT (obj))
	{
		if (G_LIKELY (GST_EVENT_TYPE (obj) == GST_EVENT_EOS))
			send_prep = (SendPrep)send_prep_eos_event;
	}

	if (G_LIKELY (send_prep))
	{ 
		gint ret;
		OMX_BUFFERHEADERTYPE *omx_buffer = NULL;

		if (port->always_copy) 
		{   

			omx_buffer = request_buffer (port);
			if (!omx_buffer)
			{
				DEBUG (port, "null buffer");
				return -1;
			}

			/* don't assume OMX component clears flags!
			 */
			omx_buffer->nFlags = 0;

			/* if buffer sharing is enabled, pAppPrivate might hold the ref to
			 * a buffer that is no longer required and should be unref'd.  We
			 * do this check here, rather than in send_prep_buffer_data() so
			 * we don't keep the reference live in case, for example, this time
			 * the buffer is used for an EOS event.
			 */
			if (omx_buffer->pAppPrivate)
			{
				GstBuffer *old_buf = omx_buffer->pAppPrivate;
				gst_buffer_unref (old_buf);
				omx_buffer->pAppPrivate = NULL;
				omx_buffer->pBuffer = NULL;     /* just to ease debugging */
			}
		}
		else
		{
			if (GST_IS_OMXBUFFERTRANSPORT (obj)) 
				omx_buffer = get_input_buffer_header (port, obj);
			else if(GST_IS_EVENT (obj) && (GST_EVENT_TYPE (obj) == GST_EVENT_EOS)) {
				omx_buffer = port->buffers[0];
			}
			else {
				GST_ERROR_OBJECT(port->core->object,"something went wrong!!\n");
				return -1; /* something went wrong */
			}
		}

		send_prep (port, omx_buffer, obj);

		ret = omx_buffer->nFilledLen;
		release_buffer (port, omx_buffer);
		return ret;
	}

	WARNING (port, "unknown obj type");
	return -1;
}

/**
 * Receive a buffer/event from OMX component.  This handles the conversion
 * of OMX buffer to GST buffer, codec-data, or EOS event.
 *
 * Returns <code>NULL</code> if buffer could not be received.
 */
gpointer
g_omx_port_recv (GOmxPort *port)
{
    gpointer ret = NULL;

    g_return_val_if_fail (port->type == GOMX_PORT_OUTPUT, NULL);

    while (!ret && port->enabled)
    {
        OMX_BUFFERHEADERTYPE *omx_buffer = request_buffer (port);

        if (G_UNLIKELY (!omx_buffer))
        {
            return NULL;
        }

        DEBUG (port, "omx_buffer=%p size=%lu, len=%lu, flags=%lu, offset=%lu, timestamp=%lld",
                omx_buffer, omx_buffer->nAllocLen, omx_buffer->nFilledLen, omx_buffer->nFlags,
                omx_buffer->nOffset, omx_buffer->nTimeStamp);

        /* XXX this ignore_count workaround might play badly w/ refcnting
         * in OMX component..
         */

        if (port->ignore_count)
        {
            DEBUG (port, "ignore_count=%d", port->ignore_count);
            release_buffer (port, omx_buffer);
            port->ignore_count--;
            continue;
        }

        if (G_UNLIKELY (omx_buffer->nFlags & OMX_BUFFERFLAG_EOS))
        {
            DEBUG (port, "got eos");
            ret = gst_event_new_eos ();
        }
        else if (G_LIKELY (omx_buffer->nFilledLen > 0))
        {
            GstBuffer *buf = omx_buffer->pAppPrivate;

            /* I'm not really sure if it was intentional to block zero-copy of
             * the codec-data buffer.. this is how the original code worked,
             * so I kept the behavior
             */
            if (!buf || (omx_buffer->nFlags & OMX_BUFFERFLAG_CODECCONFIG))
            {
                if (buf)
                    gst_buffer_unref (buf);

                if (port->always_copy) {
                    buf = buffer_alloc (port, omx_buffer->nFilledLen);
                    memcpy (GST_BUFFER_DATA (buf), omx_buffer->pBuffer, omx_buffer->nFilledLen);
                }
                else {
                    buf = gst_omxbuffertransport_new (port, omx_buffer);
                }
            }
            else if (buf)
            {
                /* don't rely on OMX having told us the correct buffer size
                 * when we allocated the buffer.
                 */
                GST_BUFFER_SIZE (buf) = omx_buffer->nFilledLen;
            }

            if (port->core->use_timestamps)
			{
				if (omx_buffer->nTimeStamp != (OMX_TICKS)-1) {
					GST_BUFFER_TIMESTAMP (buf) = gst_util_uint64_scale_int (
							omx_buffer->nTimeStamp,
							GST_SECOND, OMX_TICKS_PER_SECOND);
				} else {
					GST_BUFFER_TIMESTAMP (buf) = GST_CLOCK_TIME_NONE;
				}
			}

            if (G_UNLIKELY (omx_buffer->nFlags & OMX_BUFFERFLAG_CODECCONFIG))
            {
                GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_IN_CAPS);
            }

            port->n_offset = omx_buffer->nOffset;

            ret = buf;
        }
        else
        {
            GstBuffer *buf = omx_buffer->pAppPrivate;

            if (buf)
            {
                gst_buffer_unref (buf);
                omx_buffer->pAppPrivate = NULL;
            }

            DEBUG (port, "empty buffer %p", omx_buffer); /* keep looping */
        }

/* REVISIT: I don't know why but EZSDK OMX component sets this read-only 
   flag for resolution > QVGA
 */
#if 0
#ifdef USE_OMXTICORE
        if (omx_buffer->nFlags & OMX_TI_BUFFERFLAG_READONLY)
        {
            GstBuffer *buf = omx_buffer->pAppPrivate;

            if (buf)
            {
                /* if using buffer sharing, create an extra ref to the buffer
                 * to account for the fact that the OMX component is still
                 * holding a reference.  (This prevents the buffer from being
                 * free'd while the component is still using it as, for ex, a
                 * reference frame.)
                 */
                gst_buffer_ref (buf);
            }

            DEBUG (port, "dup'd buffer %p", omx_buffer);

            g_mutex_lock (port->core->omx_state_mutex);
            omx_buffer->nFlags &= ~OMX_TI_BUFFERFLAG_READONLY;
            g_mutex_unlock (port->core->omx_state_mutex);
        }
        else if (omx_buffer->nFlags & GST_BUFFERFLAG_UNREF_CHECK)
        {
            /* buffer has already been handled under READONLY case.. so
             * don't return it to gst.  Just unref it, and release the omx
             * buffer which was previously not released.
             */
            gst_buffer_unref(ret);
            ret = NULL;

            DEBUG (port, "unref'd buffer %p", omx_buffer);

            setup_shared_buffer (port, omx_buffer);
            release_buffer (port, omx_buffer);
        }
        else
#endif
#endif
        {
            setup_shared_buffer (port, omx_buffer);
            if ((NULL == ret) || port->always_copy) 
                release_buffer (port, omx_buffer);
        }
    }


    return ret;
}

void
g_omx_port_resume (GOmxPort *port)
{
    DEBUG (port, "resume");
    async_queue_enable (port->queue);
}

void
g_omx_port_pause (GOmxPort *port)
{
    DEBUG (port, "pause");
    async_queue_disable (port->queue);
}

void
g_omx_port_flush (GOmxPort *port)
{
    DEBUG (port, "begin");

    if (port->type == GOMX_PORT_OUTPUT)
    {
        /* This will get rid of any buffers that we have received, but not
         * yet processed in the output_loop.
         */
        OMX_BUFFERHEADERTYPE *omx_buffer;
        while ((omx_buffer = async_queue_pop_full (port->queue, FALSE, TRUE)))
        {
            omx_buffer->nFilledLen = 0;

#if 0
            if (omx_buffer->nFlags & OMX_TI_BUFFERFLAG_READONLY)
            {
                /* For output buffer that is marked with READONLY, we
                   cannot release until EventHandler OMX_TI_EventBufferRefCount
                   come. So, reset the nFlags to be released later. */
                DEBUG (port, "During flush encounter ReadOnly buffer %p", omx_buffer);
                g_mutex_lock (port->core->omx_state_mutex);
                omx_buffer->nFlags &= ~OMX_TI_BUFFERFLAG_READONLY;
                g_mutex_unlock (port->core->omx_state_mutex);
            }
            else
#endif
            {
                release_buffer (port, omx_buffer);
            }
        }
    }

    DEBUG (port, "SendCommand(Flush, %d)", port->port_index);
    OMX_SendCommand (port->core->omx_handle, OMX_CommandFlush, port->port_index, NULL);
    g_sem_down (port->core->flush_sem);
    port->ignore_count = port->num_buffers;
    DEBUG (port, "end");
}

void
g_omx_port_enable (GOmxPort *port)
{
    if (port->enabled)
    {
        DEBUG (port, "already enabled");
        return;
    }

    DEBUG (port, "begin");

    g_omx_port_prepare (port);

    DEBUG (port, "SendCommand(PortEnable, %d)", port->port_index);
    OMX_SendCommand (g_omx_core_get_handle (port->core),
            OMX_CommandPortEnable, port->port_index, NULL);

    g_omx_port_allocate_buffers (port);

    g_sem_down (port->core->port_sem);

    port->enabled = TRUE;

    if (port->core->omx_state == OMX_StateExecuting)
        g_omx_port_start_buffers (port);

    DEBUG (port, "end");
}

void
g_omx_port_disable (GOmxPort *port)
{
    if (!port->enabled)
    {
        DEBUG (port, "already disabled");
        return;
    }

    DEBUG (port, "begin");

    port->enabled = FALSE;

    DEBUG (port, "SendCommand(PortDisable, %d)", port->port_index);
    OMX_SendCommand (g_omx_core_get_handle (port->core),
            OMX_CommandPortDisable, port->port_index, NULL);

    g_omx_port_free_buffers (port);

    g_sem_down (port->core->port_sem);

    DEBUG (port, "end");
}

void
g_omx_port_finish (GOmxPort *port)
{
    DEBUG (port, "finish");
    port->enabled = FALSE;
    async_queue_disable (port->queue);
}


/*
 * Some domain specific port related utility functions:
 */

/* keep this list in sync GSTOMX_ALL_FORMATS */
static gint32 all_fourcc[] = {
        GST_MAKE_FOURCC ('N','V','1','2'),
        GST_MAKE_FOURCC ('I','4','2','0'),
        GST_MAKE_FOURCC ('Y','U','Y','2'),
        GST_MAKE_FOURCC ('U','Y','V','Y'),
};

#ifndef DIM  /* XXX is there a better alternative available? */
#  define DIM(x) (sizeof(x)/sizeof((x)[0]))
#endif

/**
 * A utility function to query the port for supported color formats, and
 * add the appropriate list of formats to @caps.  The @port can either
 * be an input port for a video encoder, or an output port for a decoder
 */
GstCaps *
g_omx_port_set_video_formats (GOmxPort *port, GstCaps *caps)
{
    OMX_VIDEO_PARAM_PORTFORMATTYPE param;
    int i,j;

    G_OMX_PORT_GET_PARAM (port, OMX_IndexParamVideoPortFormat, &param);

    caps = gst_caps_make_writable (caps);

    for (i=0; i<gst_caps_get_size (caps); i++)
    {
        GstStructure *struc = gst_caps_get_structure (caps, i);
        GValue formats = {0};

        g_value_init (&formats, GST_TYPE_LIST);

        for (j=0; j<DIM(all_fourcc); j++)
        {
            OMX_ERRORTYPE err;
            GValue fourccval = {0};

            g_value_init (&fourccval, GST_TYPE_FOURCC);

            /* check and see if OMX supports the format:
             */
            param.eColorFormat = g_omx_fourcc_to_colorformat (all_fourcc[j]);
            err = G_OMX_PORT_SET_PARAM (port, OMX_IndexParamVideoPortFormat, &param);

            if( err == OMX_ErrorIncorrectStateOperation )
            {
                DEBUG (port, "already executing?");

                /* if we are already executing, such as might be the case if
                 * we get a OMX_EventPortSettingsChanged event, just take the
                 * current format and bail:
                 */
                G_OMX_PORT_GET_PARAM (port, OMX_IndexParamVideoPortFormat, &param);
                gst_value_set_fourcc (&fourccval,
                        g_omx_colorformat_to_fourcc (param.eColorFormat));
                gst_value_list_append_value (&formats, &fourccval);
                break;
            }
            else if( err == OMX_ErrorNone )
            {
                gst_value_set_fourcc (&fourccval, all_fourcc[j]);
                gst_value_list_append_value (&formats, &fourccval);
            }
        }

        gst_structure_set_value (struc, "format", &formats);
    }

    return caps;
}

    /*For avoid repeated code needs to do only one function in order to configure
    video and images caps strure, and also maybe adding RGB color format*/

static gint32 jpeg_fourcc[] = {
        GST_MAKE_FOURCC ('U','Y','V','Y'),
        GST_MAKE_FOURCC ('N','V','1','2')
};

/**
 * A utility function to query the port for supported color formats, and
 * add the appropriate list of formats to @caps.  The @port can either
 * be an input port for a image encoder, or an output port for a decoder
 */
GstCaps *
g_omx_port_set_image_formats (GOmxPort *port, GstCaps *caps)
{
    //OMX_IMAGE_PARAM_PORTFORMATTYPE param;
    int i,j;

    //G_OMX_PORT_GET_PARAM (port, OMX_IndexParamImagePortFormat, &param);

    caps = gst_caps_make_writable (caps);

    for (i=0; i<gst_caps_get_size (caps); i++)
    {
        GstStructure *struc = gst_caps_get_structure (caps, i);
        GValue formats = {0};

        g_value_init (&formats, GST_TYPE_LIST);

        for (j=0; j<DIM(jpeg_fourcc); j++)
        {
            //OMX_ERRORTYPE err;
            GValue fourccval = {0};

            g_value_init (&fourccval, GST_TYPE_FOURCC);

        /* Got error from omx jpeg component , avoiding these lines by the moment till they support it*/
#if 0
            /* check and see if OMX supports the format:
             */
            param.eColorFormat = g_omx_fourcc_to_colorformat (all_fourcc[j]);
            err = G_OMX_PORT_SET_PARAM (port, OMX_IndexParamImagePortFormat, &param);

            if( err == OMX_ErrorIncorrectStateOperation )
            {
                DEBUG (port, "already executing?");

                /* if we are already executing, such as might be the case if
                 * we get a OMX_EventPortSettingsChanged event, just take the
                 * current format and bail:
                 */
                G_OMX_PORT_GET_PARAM (port, OMX_IndexParamImagePortFormat, &param);
                gst_value_set_fourcc (&fourccval,
                        g_omx_colorformat_to_fourcc (param.eColorFormat));
                gst_value_list_append_value (&formats, &fourccval);
                break;
            }
            else if( err == OMX_ErrorNone )
            {
                gst_value_set_fourcc (&fourccval, all_fourcc[j]);
                gst_value_list_append_value (&formats, &fourccval);
            }
#else
            gst_value_set_fourcc (&fourccval, jpeg_fourcc[j]);
            gst_value_list_append_value (&formats, &fourccval);
#endif
        }

        gst_structure_set_value (struc, "format", &formats);
    }

    return caps;
}

