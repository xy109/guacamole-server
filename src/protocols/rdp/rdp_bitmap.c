/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "config.h"

#include "client.h"
#include "common/display.h"
#include "common/surface.h"
#include "rdp.h"
#include "rdp_bitmap.h"
#include "rdp_settings.h"

#include <cairo/cairo.h>
#include <freerdp/codec/bitmap.h>
#include <freerdp/codec/color.h>
#include <freerdp/freerdp.h>
#include <guacamole/client.h>
#include <guacamole/socket.h>

#ifdef ENABLE_WINPR
#include <winpr/wtypes.h>
#else
#include "compat/winpr-wtypes.h"
#endif
#ifdef FREERDP_2
#include <freerdp/gdi/gdi.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#ifdef FREERDP_2
BOOL guac_rdp_convert_bitmap(rdpContext *context, rdpBitmap *bitmap)
{
    if (bitmap->format != context->gdi->dstFormat)
    {
        BYTE *data;
        if (!(data = _aligned_malloc(bitmap->width * bitmap->height * 4, 16)))
            return FALSE;
        if (!freerdp_image_copy(data, context->gdi->dstFormat, 0, 0, 0,
                                bitmap->width, bitmap->height,
                                bitmap->data, bitmap->format,
                                0, 0, 0, &context->gdi->palette, FREERDP_FLIP_NONE))
        {
            _aligned_free(data);
            return FALSE;
        }

        _aligned_free(bitmap->data);
        bitmap->data = data;
        bitmap->format = context->gdi->dstFormat;
    }
    return TRUE;
}
#endif

void guac_rdp_cache_bitmap(rdpContext *context, rdpBitmap *bitmap)
{
    guac_client *client = ((rdp_freerdp_context *)context)->client;
    guac_rdp_client *rdp_client = (guac_rdp_client *)client->data;

    /* Allocate buffer */
    guac_common_display_layer *buffer = guac_common_display_alloc_buffer(
        rdp_client->display, bitmap->width, bitmap->height);

    /* Cache image data if present */
    if (bitmap->data != NULL)
    {
/* Create surface from image data */
#ifdef FREERDP_2
        guac_rdp_convert_bitmap(context, bitmap);
#endif
        cairo_surface_t *image = cairo_image_surface_create_for_data(
            bitmap->data, CAIRO_FORMAT_RGB24,
            bitmap->width, bitmap->height, 4 * bitmap->width);
        /* Send surface to buffer */
        guac_common_surface_draw(buffer->surface, 0, 0, image);

        /* Free surface */
        cairo_surface_destroy(image);
    }

    /* Store buffer reference in bitmap */
    ((guac_rdp_bitmap *)bitmap)->layer = buffer;
}

BOOL guac_rdp_bitmap_new(rdpContext *context, rdpBitmap *bitmap)
{
    if (bitmap->data)
    {
#ifdef FREERDP_2
        guac_rdp_convert_bitmap(context, bitmap);
#else
        /* Convert image data if present */
        if (bitmap->bpp != 32)
        {

            /* Convert image data to 32-bit RGB */
            unsigned char *image_buffer = freerdp_image_convert(bitmap->data, NULL,
                                                                bitmap->width, bitmap->height,
                                                                guac_rdp_get_depth(context->instance),
                                                                32, ((rdp_freerdp_context *)context)->clrconv);

            /* Free existing image, if any */
            if (image_buffer != bitmap->data)
            {
#ifdef FREERDP_BITMAP_REQUIRES_ALIGNED_MALLOC
                _aligned_free(bitmap->data);
#else
                free(bitmap->data);
#endif
            }
            /* Store converted image in bitmap */
            bitmap->data = image_buffer;
        }
#endif
    }

    /* No corresponding surface yet - caching is deferred. */
    ((guac_rdp_bitmap *)bitmap)->layer = NULL;

    /* Start at zero usage */
    ((guac_rdp_bitmap *)bitmap)->used = 0;
    return TRUE;
}

BOOL guac_rdp_bitmap_paint(rdpContext *context, rdpBitmap *bitmap)
{
    guac_client *client = ((rdp_freerdp_context *)context)->client;
    guac_rdp_client *rdp_client = (guac_rdp_client *)client->data;

    guac_common_display_layer *buffer = ((guac_rdp_bitmap *)bitmap)->layer;

    int width = bitmap->right - bitmap->left + 1;
    int height = bitmap->bottom - bitmap->top + 1;

    guac_client_log(client, GUAC_LOG_DEBUG, "call guac_rdp_bitmap_paint");

    /* If not cached, cache if necessary */
    if (buffer == NULL && ((guac_rdp_bitmap *)bitmap)->used >= 1)
        guac_rdp_cache_bitmap(context, bitmap);

    /* If cached, retrieve from cache */
    if (buffer != NULL)
    {
        guac_common_surface_copy(buffer->surface, 0, 0, width, height,
                                 rdp_client->display->default_surface,
                                 bitmap->left, bitmap->top);

        /* Otherwise, draw with stored image data */
    }
    else if (bitmap->data != NULL)
    {
        /* Create surface from image data */
#ifdef FREERDP_2
        guac_rdp_convert_bitmap(context, bitmap);
#endif
        cairo_surface_t *image = cairo_image_surface_create_for_data(
            bitmap->data, CAIRO_FORMAT_RGB24,
            width, height, 4 * bitmap->width);
        /* Draw image on default surface */
        guac_common_surface_draw(rdp_client->display->default_surface,
                                 bitmap->left, bitmap->top, image);

        /* Free surface */
        cairo_surface_destroy(image);
    }

    /* Increment usage counter */
    ((guac_rdp_bitmap *)bitmap)->used++;
    return TRUE;
}

void guac_rdp_bitmap_free(rdpContext *context, rdpBitmap *bitmap)
{

    guac_client *client = ((rdp_freerdp_context *)context)->client;
    guac_rdp_client *rdp_client = (guac_rdp_client *)client->data;
    guac_common_display_layer *buffer = ((guac_rdp_bitmap *)bitmap)->layer;

    /* If cached, free buffer */
    if (buffer != NULL)
        guac_common_display_free_buffer(rdp_client->display, buffer);
}

BOOL guac_rdp_bitmap_setsurface(rdpContext *context, rdpBitmap *bitmap, BOOL primary)
{

    guac_client *client = ((rdp_freerdp_context *)context)->client;
    guac_rdp_client *rdp_client = (guac_rdp_client *)client->data;

    if (primary)
        rdp_client->current_surface = rdp_client->display->default_surface;

    else
    {
        /* Make sure that the recieved bitmap is not NULL before processing */
        if (bitmap == NULL)
        {
            guac_client_log(client, GUAC_LOG_INFO, "NULL bitmap found in bitmap_setsurface instruction.");
            return TRUE;
        }

        /* If not available as a surface, make available. */
        if (((guac_rdp_bitmap *)bitmap)->layer == NULL)
            guac_rdp_cache_bitmap(context, bitmap);

        rdp_client->current_surface =
            ((guac_rdp_bitmap *)bitmap)->layer->surface;
    }
    return TRUE;
}

#ifdef LEGACY_RDPBITMAP
BOOL guac_rdp_bitmap_decompress(rdpContext *context, rdpBitmap *bitmap, UINT8 *data,
                                int width, int height, int bpp, int length, BOOL compressed)
#elif defined(FREERDP_2)
BOOL guac_rdp_bitmap_decompress(rdpContext *context, rdpBitmap *bitmap,
                                const BYTE *data, UINT32 width, UINT32 height,
                                UINT32 bpp, UINT32 length, BOOL compressed,
                                UINT32 codec_id)
#else
BOOL guac_rdp_bitmap_decompress(rdpContext *context, rdpBitmap *bitmap, const UINT8 *data,
                                int width, int height, int bpp, int length, BOOL compressed, int codec_id)
#endif
{
    int size = width * height * 4;
#ifdef FREERDP_BITMAP_REQUIRES_ALIGNED_MALLOC
    /* Free pre-existing data, if any (might be reused) */
    if (bitmap->data != NULL)
        _aligned_free(bitmap->data);

    /* Allocate new data */
    bitmap->data = (UINT8 *)_aligned_malloc(size, 16);
#else
    /* Free pre-existing data, if any (might be reused) */
    free(bitmap->data);

    /* Allocate new data */
    bitmap->data = (UINT8 *)malloc(size);
#endif

    if (compressed)
    {

#ifdef HAVE_RDPCONTEXT_CODECS
        rdpCodecs *codecs = context->codecs;
        /* Decode as interleaved if less than 32 bits per pixel */
        if (bpp < 32)
        {
#ifdef FREERDP_2
            freerdp_client_codecs_prepare(codecs, FREERDP_CODEC_INTERLEAVED, width, height);
#else
            freerdp_client_codecs_prepare(codecs, FREERDP_CODEC_INTERLEAVED);
#endif
#ifdef INTERLEAVED_DECOMPRESS_TAKES_PALETTE
            interleaved_decompress(codecs->interleaved, data, length, bpp,
                                   &(bitmap->data), PIXEL_FORMAT_XRGB32, -1, 0, 0, width, height,
                                   (BYTE *)((rdp_freerdp_context *)context)->palette);
            bitmap->bpp = 32;
#elif defined(FREERDP_2)
            interleaved_decompress(codecs->interleaved, data, length, width, height, bpp,
                                   bitmap->data, PIXEL_FORMAT_XRGB32, -1, 0, 0, width, height, &(context->gdi->palette));
#else
            interleaved_decompress(codecs->interleaved, data, length, bpp,
                                   &(bitmap->data), PIXEL_FORMAT_XRGB32, -1, 0, 0, width, height);
            bitmap->bpp = bpp;
#endif
        }

        /* Otherwise, decode as planar */
        else
        {
#ifdef FREERDP_2
            freerdp_client_codecs_prepare(codecs, FREERDP_CODEC_PLANAR, width, height);
#else
            freerdp_client_codecs_prepare(codecs, FREERDP_CODEC_PLANAR);
#endif
#ifdef PLANAR_DECOMPRESS_CAN_FLIP
            planar_decompress(codecs->planar, data, length,
                              &(bitmap->data), PIXEL_FORMAT_XRGB32, -1, 0, 0, width, height,
                              TRUE);
            bitmap->bpp = 32;
#elif defined(FREERDP_2)
            planar_decompress(codecs->planar, data, length, width, height,
                              bitmap->data, PIXEL_FORMAT_XRGB32, -1, 0, 0, width, height, FREERDP_FLIP_VERTICAL);
#else
            planar_decompress(codecs->planar, data, length,
                              &(bitmap->data), PIXEL_FORMAT_XRGB32, -1, 0, 0, width, height);
            bitmap->bpp = bpp;
#endif
        }
#else
        bitmap_decompress(data, bitmap->data, width, height, length, bpp, bpp);
        bitmap->bpp = bpp;
#endif
    }
    else
    {
#ifdef FREERDP_2
        const UINT32 SrcFormat = gdi_get_pixel_format(bpp);
        if (!freerdp_image_copy(bitmap->data, PIXEL_FORMAT_XRGB32, 0, 0, 0,
                                width, height, data, SrcFormat,
                                0, 0, 0, &(context->gdi->palette), FREERDP_FLIP_VERTICAL))
            return TRUE;
#else
        freerdp_image_flip(data, bitmap->data, width, height, bpp);
        bitmap->bpp = bpp;
#endif
    }
#ifdef FREERDP_2
    bitmap->format = PIXEL_FORMAT_XRGB32;
#endif
    bitmap->compressed = FALSE;
    bitmap->length = size;
    return TRUE;
}
