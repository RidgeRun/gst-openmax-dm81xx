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

#include "gstomx_util.h"
#include <dlfcn.h>
#include <string.h>

#include "gstomx.h"

GST_DEBUG_CATEGORY (gstomx_util_debug);


/* protect implementations hash_table */
static GMutex *imp_mutex;
static GHashTable *implementations;
static gboolean initialized;


/*
 * Main
 */

static GOmxImp *
imp_new (const gchar *name)
{
    GOmxImp *imp;

    imp = g_new0 (GOmxImp, 1);

    #ifdef USE_STATIC
        imp->mutex = g_mutex_new ();
    #else
    /* Load the OpenMAX IL symbols */
    {
        void *handle;

        imp->dl_handle = handle = dlopen (name, RTLD_LAZY);
        GST_DEBUG ("dlopen(%s) -> %p", name, handle);
        if (!handle)
        {
            g_warning ("%s\n", dlerror ());
            g_free (imp);
            return NULL;
        }

        imp->mutex = g_mutex_new ();
        imp->sym_table.init = dlsym (handle, "OMX_Init");
        imp->sym_table.deinit = dlsym (handle, "OMX_Deinit");
        imp->sym_table.get_handle = dlsym (handle, "OMX_GetHandle");
        imp->sym_table.free_handle = dlsym (handle, "OMX_FreeHandle");
    }
    #endif

    return imp;
}

static void
imp_free (GOmxImp *imp)
{
    if (imp->dl_handle)
    {
        dlclose (imp->dl_handle);
    }
    g_mutex_free (imp->mutex);
    g_free (imp);
}

/*
 * Helpers used by GOmxCore:
 */

GOmxImp *
g_omx_request_imp (const gchar *name)
{
    GOmxImp *imp = NULL;

    g_mutex_lock (imp_mutex);
    imp = g_hash_table_lookup (implementations, name);
    if (!imp)
    {
        imp = imp_new (name);
        if (imp)
            g_hash_table_insert (implementations, g_strdup (name), imp);
    }
    g_mutex_unlock (imp_mutex);

    if (!imp)
        return NULL;

    g_mutex_lock (imp->mutex);
    if (imp->client_count == 0)
    {
        OMX_ERRORTYPE omx_error;

        #ifdef USE_STATIC
        omx_error = OMX_Init ();
        #else
        omx_error = imp->sym_table.init ();
        #endif
        if (omx_error)
        {
            g_mutex_unlock (imp->mutex);
            return NULL;
        }
    }
    imp->client_count++;
    g_mutex_unlock (imp->mutex);

    return imp;
}

void
g_omx_release_imp (GOmxImp *imp)
{
    g_mutex_lock (imp->mutex);
    imp->client_count--;
    if (imp->client_count == 0)
    {
        #ifdef USE_STATIC
        OMX_Deinit();
        #else
        imp->sym_table.deinit ();
        #endif
    }
    g_mutex_unlock (imp->mutex);
}

/*
 * Helpers used by plugin:
 */

void
g_omx_init (void)
{
    if (!initialized)
    {
        /* safe as plugin_init is safe */
        imp_mutex = g_mutex_new ();
        implementations = g_hash_table_new_full (g_str_hash,
                                                 g_str_equal,
                                                 g_free,
                                                 (GDestroyNotify) imp_free);
        initialized = TRUE;
    }
}

void
g_omx_deinit (void)
{
    if (initialized)
    {
        g_hash_table_destroy (implementations);
        g_mutex_free (imp_mutex);
        initialized = FALSE;
    }
}



/*
 * Some misc utilities..
 */

const char *
g_omx_error_to_str (OMX_ERRORTYPE omx_error)
{
    switch (omx_error)
    {
        case OMX_ErrorNone:
            return "None";

        case OMX_ErrorInsufficientResources:
            return "There were insufficient resources to perform the requested operation";

        case OMX_ErrorUndefined:
            return "The cause of the error could not be determined";

        case OMX_ErrorInvalidComponentName:
            return "The component name string was not valid";

        case OMX_ErrorComponentNotFound:
            return "No component with the specified name string was found";

        case OMX_ErrorInvalidComponent:
            return "The component specified did not have an entry point";

        case OMX_ErrorBadParameter:
            return "One or more parameters were not valid";

        case OMX_ErrorNotImplemented:
            return "The requested function is not implemented";

        case OMX_ErrorUnderflow:
            return "The buffer was emptied before the next buffer was ready";

        case OMX_ErrorOverflow:
            return "The buffer was not available when it was needed";

        case OMX_ErrorHardware:
            return "The hardware failed to respond as expected";

        case OMX_ErrorInvalidState:
            return "The component is in invalid state";

        case OMX_ErrorStreamCorrupt:
            return "Stream is found to be corrupt";

        case OMX_ErrorPortsNotCompatible:
            return "Ports being connected are not compatible";

        case OMX_ErrorResourcesLost:
            return "Resources allocated to an idle component have been lost";

        case OMX_ErrorNoMore:
            return "No more indices can be enumerated";

        case OMX_ErrorVersionMismatch:
            return "The component detected a version mismatch";

        case OMX_ErrorNotReady:
            return "The component is not ready to return data at this time";

        case OMX_ErrorTimeout:
            return "There was a timeout that occurred";

        case OMX_ErrorSameState:
            return "This error occurs when trying to transition into the state you are already in";

        case OMX_ErrorResourcesPreempted:
            return "Resources allocated to an executing or paused component have been preempted";

        case OMX_ErrorPortUnresponsiveDuringAllocation:
            return "Waited an unusually long time for the supplier to allocate buffers";

        case OMX_ErrorPortUnresponsiveDuringDeallocation:
            return "Waited an unusually long time for the supplier to de-allocate buffers";

        case OMX_ErrorPortUnresponsiveDuringStop:
            return "Waited an unusually long time for the non-supplier to return a buffer during stop";

        case OMX_ErrorIncorrectStateTransition:
            return "Attempting a state transition that is not allowed";

        case OMX_ErrorIncorrectStateOperation:
            return "Attempting a command that is not allowed during the present state";

        case OMX_ErrorUnsupportedSetting:
            return "The values encapsulated in the parameter or config structure are not supported";

        case OMX_ErrorUnsupportedIndex:
            return "The parameter or config indicated by the given index is not supported";

        case OMX_ErrorBadPortIndex:
            return "The port index supplied is incorrect";

        case OMX_ErrorPortUnpopulated:
            return "The port has lost one or more of its buffers and it thus unpopulated";

        case OMX_ErrorComponentSuspended:
            return "Component suspended due to temporary loss of resources";

        case OMX_ErrorDynamicResourcesUnavailable:
            return "Component suspended due to an inability to acquire dynamic resources";

        case OMX_ErrorMbErrorsInFrame:
            return "Frame generated macroblock error";

        case OMX_ErrorFormatNotDetected:
            return "Cannot parse or determine the format of an input stream";

        case OMX_ErrorContentPipeOpenFailed:
            return "The content open operation failed";

        case OMX_ErrorContentPipeCreationFailed:
            return "The content creation operation failed";

        case OMX_ErrorSeperateTablesUsed:
            return "Separate table information is being used";

        case OMX_ErrorTunnelingUnsupported:
            return "Tunneling is unsupported by the component";

        default:
            return "Unknown error";
    }
}

OMX_COLOR_FORMATTYPE g_omx_fourcc_to_colorformat (guint32 fourcc)
{
    switch (fourcc)
    {
        case GST_MAKE_FOURCC ('I', '4', '2', '0'):
            return OMX_COLOR_FormatYUV420PackedPlanar;
        case GST_MAKE_FOURCC ('Y', 'U', 'Y', '2'):
            return OMX_COLOR_FormatYCbYCr;
        case GST_MAKE_FOURCC ('U', 'Y', 'V', 'Y'):
            return OMX_COLOR_FormatCbYCrY;
        case GST_MAKE_FOURCC ('N', 'V', '1', '2'):
            return OMX_COLOR_FormatYUV420PackedSemiPlanar;
        default:
            /* TODO, add other needed color formats.. */
            return OMX_COLOR_FormatUnused;
    }
}

guint32 g_omx_colorformat_to_fourcc (OMX_COLOR_FORMATTYPE eColorFormat)
{
    switch (eColorFormat)
    {
        case OMX_COLOR_FormatYUV420PackedPlanar:
            return GST_MAKE_FOURCC ('I', '4', '2', '0');
        case OMX_COLOR_FormatYCbYCr:
            return GST_MAKE_FOURCC ('Y', 'U', 'Y', '2');
        case OMX_COLOR_FormatCbYCrY:
            return GST_MAKE_FOURCC ('U', 'Y', 'V', 'Y');
        case OMX_COLOR_FormatYUV420PackedSemiPlanar:
            return GST_MAKE_FOURCC ('N', 'V', '1', '2');
        default:
            /* TODO, add other needed color formats.. */
            return 0;
    }
}

OMX_COLOR_FORMATTYPE g_omx_gstvformat_to_colorformat (GstVideoFormat videoformat)
{
    switch (videoformat)
    {
        case GST_VIDEO_FORMAT_I420:
            return OMX_COLOR_FormatYUV420PackedPlanar;
        case GST_VIDEO_FORMAT_YUY2:
            return OMX_COLOR_FormatYCbYCr;
        case GST_VIDEO_FORMAT_UYVY:
            return OMX_COLOR_FormatCbYCrY;
        case GST_VIDEO_FORMAT_NV12:
            return OMX_COLOR_FormatYUV420PackedSemiPlanar;
        case GST_VIDEO_FORMAT_RGB:
            return OMX_COLOR_Format24bitRGB888;
        case GST_VIDEO_FORMAT_ARGB:
            return OMX_COLOR_Format32bitARGB8888;
        /* Remove this comment after RGB_16 being added to GstVideoFormat list
        case GST_VIDEO_FORMAT_RGB_16:
            return OMX_COLOR_Format16bitRGB565;
        */
        default:
            return OMX_COLOR_FormatUnused;
    }
 }
