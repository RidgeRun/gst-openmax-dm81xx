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

#ifndef GSTOMX_CORE_H
#define GSTOMX_CORE_H

#include "gstomx_util.h"

G_BEGIN_DECLS

/* Typedefs. */

typedef void (*GOmxCb) (GOmxCore *core);
typedef void (*GOmxCbargs2) (GOmxCore *core, gint data1, gint data2);

/* Structures. */

struct GOmxCore
{
    gpointer object; /**< GStreamer element. */

    OMX_HANDLETYPE omx_handle;
    OMX_ERRORTYPE omx_error;

    OMX_STATETYPE omx_state;
    GCond *omx_state_condition;
    GMutex *omx_state_mutex;

    GPtrArray *ports;

    GSem *done_sem;
    GSem *flush_sem;
    GSem *port_sem;

    GOmxCb settings_changed_cb;
    GOmxCbargs2 index_settings_changed_cb;

    GOmxImp *imp;

    gboolean done;

    gboolean use_timestamps; /** @todo remove; timestamps should always be used */

    gboolean gen_timestamps;
	GstClockTime   last_buf_timestamp;
};

/* Utility Macros */

#define _G_OMX_INIT_PARAM(param) G_STMT_START {  /* util for other macros */  \
        memset ((param), 0, sizeof (*(param)));                               \
        (param)->nSize = sizeof (*(param));                                   \
        (param)->nVersion.s.nVersionMajor = 1;                                \
        (param)->nVersion.s.nVersionMinor = 1;                                \
    } G_STMT_END

#define G_OMX_CORE_GET_PARAM(core, idx, param) G_STMT_START {                 \
        _G_OMX_INIT_PARAM (param);                                            \
        OMX_GetParameter (g_omx_core_get_handle (core), (idx), (param));      \
    } G_STMT_END

#define G_OMX_CORE_SET_PARAM(core, idx, param)                                \
        OMX_SetParameter (                                                    \
            g_omx_core_get_handle (core), (idx), (param))

#define G_OMX_CORE_GET_CONFIG(core, idx, param) G_STMT_START {                \
        _G_OMX_INIT_PARAM (param);                                            \
        OMX_GetConfig (g_omx_core_get_handle (core), (idx), (param));         \
    } G_STMT_END

#define G_OMX_CORE_SET_CONFIG(core, idx, param)                               \
        OMX_SetConfig (                                                       \
            g_omx_core_get_handle (core), (idx), (param))


/* Functions. */

GOmxCore *g_omx_core_new (gpointer object, gpointer klass);
void g_omx_core_free (GOmxCore *core);
void g_omx_core_init (GOmxCore *core);
void g_omx_core_deinit (GOmxCore *core);
void g_omx_core_prepare (GOmxCore *core);
void g_omx_core_start (GOmxCore *core);
void g_omx_core_pause (GOmxCore *core);
void g_omx_core_stop (GOmxCore *core);
void g_omx_core_unload (GOmxCore *core);
void g_omx_core_set_done (GOmxCore *core);
void g_omx_core_wait_for_done (GOmxCore *core);
void g_omx_core_flush_start (GOmxCore *core);
void g_omx_core_flush_stop (GOmxCore *core);
OMX_HANDLETYPE g_omx_core_get_handle (GOmxCore *core);
GOmxPort *g_omx_core_get_port (GOmxCore *core, const gchar *name, guint index);
void g_omx_core_change_state (GOmxCore *core, OMX_STATETYPE state);

/* Friend:  helpers used by GOmxPort */
void g_omx_core_got_buffer (GOmxCore *core,
        GOmxPort *port,
        OMX_BUFFERHEADERTYPE *omx_buffer);

G_END_DECLS


#endif /* GSTOMX_CORE_H */
