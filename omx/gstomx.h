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

#ifndef GSTOMX_H
#define GSTOMX_H

#include <gst/gst.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/types.h>

G_BEGIN_DECLS

GST_DEBUG_CATEGORY_EXTERN (gstomx_debug);
GST_DEBUG_CATEGORY_EXTERN (gstomx_util_debug);
GST_DEBUG_CATEGORY_EXTERN (gstomx_ppm);
#define GST_CAT_DEFAULT gstomx_debug

// #define ENABLE_OMX_DEBUG

#ifdef ENABLE_OMX_DEBUG
extern FILE *omx_debug_fp;
extern struct timeval omx_debug_start_time;

#define DEBUG_BUFFER_IN(name,ts) do { \
	struct timeval tv; \
	gettimeofday(&tv, NULL); \
	tv.tv_sec -= omx_debug_start_time.tv_sec; \
	fprintf(omx_debug_fp, "%d%06d Buffer [%lld] in %s (%d)\n", tv.tv_sec, tv.tv_usec, ts, name, syscall(SYS_gettid)); \
} while (0)

#define DEBUG_BUFFER_OUT(name,ts) do { \
	struct timeval tv; \
	gettimeofday(&tv, NULL); \
	tv.tv_sec -= omx_debug_start_time.tv_sec; \
	fprintf(omx_debug_fp, "%d%06d Buffer [%lld] out %s (%d)\n", tv.tv_sec, tv.tv_usec, ts, name, syscall(SYS_gettid)); \
} while (0)

#define DEBUG_BUFFER_FREE(name,ts) do { \
	struct timeval tv; \
	gettimeofday(&tv, NULL); \
	tv.tv_sec -= omx_debug_start_time.tv_sec; \
	fprintf(omx_debug_fp, "%d%06d Buffer [%lld] free %s (%d)\n", tv.tv_sec, tv.tv_usec, ts, name, syscall(SYS_gettid)); \
} while (0)

#define DEBUG_BUFFER_FEED(name,ts) do { \
	struct timeval tv; \
	gettimeofday(&tv, NULL); \
	tv.tv_sec -= omx_debug_start_time.tv_sec; \
	fprintf(omx_debug_fp, "%d%06d Buffer [%lld] feed %s (%d)\n", tv.tv_sec, tv.tv_usec, ts, name, syscall(SYS_gettid)); \
} while (0)
#else
#define DEBUG_BUFFER_IN(name,ts) do { } while (0)
#define DEBUG_BUFFER_OUT(name,ts) do { } while (0)
#define DEBUG_BUFFER_FREE(name,ts) do { } while (0)
#define DEBUG_BUFFER_FEED(name,ts) do { } while (0)
#endif


G_END_DECLS

#endif /* GSTOMX_H */
