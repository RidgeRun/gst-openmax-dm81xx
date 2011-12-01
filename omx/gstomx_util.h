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

#ifndef GSTOMX_UTIL_H
#define GSTOMX_UTIL_H

#include <glib.h>
#include <gst/video/video.h>
#include <OMX_Core.h>
#include <OMX_Component.h>
#include <OMX_TI_Video.h> /* for OMX_TI_VIDEO_CODINGTYPE enumeration including VP6 and VP7 formats*/

#include <async_queue.h>
#include <sem.h>

G_BEGIN_DECLS

#define GST_BUFFERFLAG_UNREF_CHECK 0x10000000

/* Typedefs. */

typedef struct GOmxCore GOmxCore;
typedef struct GOmxPort GOmxPort;
typedef struct GOmxImp GOmxImp;
typedef struct GOmxSymbolTable GOmxSymbolTable;


#include "gstomx_core.h"
#include "gstomx_port.h"


/* Structures. */

struct GOmxSymbolTable
{
    OMX_ERRORTYPE (*init) (void);
    OMX_ERRORTYPE (*deinit) (void);
    OMX_ERRORTYPE (*get_handle) (OMX_HANDLETYPE *handle,
                                 OMX_STRING name,
                                 OMX_PTR data,
                                 OMX_CALLBACKTYPE *callbacks);
    OMX_ERRORTYPE (*free_handle) (OMX_HANDLETYPE handle);
};

struct GOmxImp
{
    guint client_count;
    void *dl_handle;
    GOmxSymbolTable sym_table;
    GMutex *mutex;
};

/* Functions. */

void g_omx_init (void);
void g_omx_deinit (void);

GOmxImp * g_omx_request_imp (const gchar *name);
void g_omx_release_imp (GOmxImp *imp);

const char * g_omx_error_to_str (OMX_ERRORTYPE omx_error);
OMX_COLOR_FORMATTYPE g_omx_fourcc_to_colorformat (guint32 fourcc);
guint32 g_omx_colorformat_to_fourcc (OMX_COLOR_FORMATTYPE eColorFormat);
OMX_COLOR_FORMATTYPE g_omx_gstvformat_to_colorformat (GstVideoFormat videoformat);



/**
 * Basically like GST_BOILERPLATE / GST_BOILERPLATE_FULL, but follows the
 * init fxn naming conventions used by gst-openmax.  It expects the following
 * functions to be defined in the same src file following this macro
 * <ul>
 *   <li> type_base_init(gpointer g_class)
 *   <li> type_class_init(gpointer g_class, gpointer class_data)
 *   <li> type_instance_init(GTypeInstance *instance, gpointer g_class)
 * </ul>
 */
#define GSTOMX_BOILERPLATE_FULL(type, type_as_function, parent_type, parent_type_macro, additional_initializations) \
static void type_base_init (gpointer g_class);                                \
static void type_class_init (gpointer g_class, gpointer class_data);          \
static void type_instance_init (GTypeInstance *instance, gpointer g_class);   \
static parent_type ## Class *parent_class;                                    \
static void type_class_init_trampoline ## type (gpointer g_class, gpointer class_data)\
{                                                                             \
    parent_class = g_type_class_ref (parent_type_macro);                      \
    type_class_init (g_class, class_data);                                    \
}                                                                             \
GType type_as_function ## _get_type (void)                                    \
{                                                                             \
    /* The typedef for GType may be gulong or gsize, depending on the         \
     * system and whether the compiler is c++ or not. The g_once_init_*       \
     * functions always take a gsize * though ... */                          \
    static volatile gsize gonce_data = 0;                                     \
    if (g_once_init_enter (&gonce_data)) {                                    \
        GType _type;                                                          \
        GTypeInfo *type_info;                                                 \
        type_info = g_new0 (GTypeInfo, 1);                                    \
        type_info->class_size = sizeof (type ## Class);                       \
        type_info->base_init = type_base_init;                                \
        type_info->class_init = type_class_init_trampoline ## type;           \
        type_info->instance_size = sizeof (type);                             \
        type_info->instance_init = type_instance_init;                        \
        _type = g_type_register_static (parent_type_macro, #type, type_info, 0);\
        g_free (type_info);                                                   \
        additional_initializations (_type);                                   \
        g_once_init_leave (&gonce_data, (gsize) _type);                       \
    }                                                                         \
    return (GType) gonce_data;                                                \
}

#define GSTOMX_BOILERPLATE(type,type_as_function,parent_type,parent_type_macro)    \
  GSTOMX_BOILERPLATE_FULL (type, type_as_function, parent_type, parent_type_macro, \
      __GST_DO_NOTHING)


/* Debug Macros:
 */
#if 1
#define PRINT_BUFFER(obj, buffer)    G_STMT_START {             \
    if (buffer) {                                               \
      GST_DEBUG_OBJECT (obj, #buffer "=%p (time=%"GST_TIME_FORMAT", duration=%"GST_TIME_FORMAT", flags=%08x, size=%d)", \
              (buffer), GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)), GST_TIME_ARGS (GST_BUFFER_DURATION(buffer)), \
              GST_BUFFER_FLAGS (buffer), GST_BUFFER_SIZE (buffer)); \
    } else {                                                    \
      GST_DEBUG_OBJECT (obj, #buffer "=null");                  \
    }                                                           \
  } G_STMT_END
#else
#define PRINT_BUFFER(obj, buffer)    G_STMT_START {             \
    if (buffer) {                                               \
      printf ( "buffer=%p (time=%"GST_TIME_FORMAT", duration=%"GST_TIME_FORMAT", flags=%08x, size=%d)\n", \
              (buffer), GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)), GST_TIME_ARGS (GST_BUFFER_DURATION(buffer)), \
              GST_BUFFER_FLAGS (buffer), GST_BUFFER_SIZE (buffer)); \
    } else {                                                    \
      GST_DEBUG_OBJECT (obj, #buffer "=null");                  \
    }                                                           \
  } G_STMT_END
#endif

G_END_DECLS

#endif /* GSTOMX_UTIL_H */
