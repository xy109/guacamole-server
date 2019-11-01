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
#include "common/clipboard.h"
#include "common/iconv.h"
#include "rdp.h"
#include "rdp_cliprdr.h"

#include <freerdp/channels/channels.h>
#include <freerdp/freerdp.h>
#ifdef FREERDP_2
#include <freerdp/channels/cliprdr.h>
#include <freerdp/client/cliprdr.h>
#else
#include <freerdp/utils/event.h>
#endif
#include <guacamole/client.h>

#ifdef ENABLE_WINPR
#include <winpr/wtypes.h>
#else
#include "compat/winpr-wtypes.h"
#endif

#ifdef HAVE_FREERDP_CLIENT_CLIPRDR_H
#include <freerdp/client/cliprdr.h>
#else
#include "compat/client-cliprdr.h"
#endif

#include <stdlib.h>
#include <string.h>

#ifdef FREERDP_2
#define TEXT_FORMATS_COUNT 2
static CLIPRDR_FORMAT guac_text_formats[] = {{CF_TEXT, '\0'}, {CF_UNICODETEXT, '\0'}};
#endif

void guac_rdp_process_cliprdr_event(guac_client *client, wMessage *event)
{

#ifdef LEGACY_EVENT
    switch (event->event_type)
    {
#else
    switch (GetMessageType(event->id))
    {
#endif

    case CliprdrChannel_MonitorReady:
        guac_rdp_process_cb_monitor_ready(client, event);
        break;

    case CliprdrChannel_FormatList:
        guac_rdp_process_cb_format_list(client,
                                        (RDP_CB_FORMAT_LIST_EVENT *)event);
        break;

    case CliprdrChannel_DataRequest:
        guac_rdp_process_cb_data_request(client,
                                         (RDP_CB_DATA_REQUEST_EVENT *)event);
        break;

    case CliprdrChannel_DataResponse:
        guac_rdp_process_cb_data_response(client,
                                          (RDP_CB_DATA_RESPONSE_EVENT *)event);
        break;

    default:
#ifdef LEGACY_EVENT
        guac_client_log(client, GUAC_LOG_INFO,
                        "Unknown cliprdr event type: 0x%x",
                        event->event_type);
#else
        guac_client_log(client, GUAC_LOG_INFO,
                        "Unknown cliprdr event type: 0x%x",
                        GetMessageType(event->id));
#endif
    }
}
#ifndef FREERDP_2
void guac_rdp_process_cb_monitor_ready(guac_client *client, wMessage *event)
{

    rdpChannels *channels =
        ((guac_rdp_client *)client->data)->rdp_inst->context->channels;

    RDP_CB_FORMAT_LIST_EVENT *format_list =
        (RDP_CB_FORMAT_LIST_EVENT *)freerdp_event_new(
            CliprdrChannel_Class,
            CliprdrChannel_FormatList,
            NULL, NULL);

    /* Received notification of clipboard support. */

    /* Respond with supported format list */
    format_list->formats = (UINT32 *)malloc(sizeof(UINT32) * 2);
    format_list->formats[0] = CB_FORMAT_TEXT;
    format_list->formats[1] = CB_FORMAT_UNICODETEXT;
    format_list->num_formats = 2;

    freerdp_channels_send_event(channels, (wMessage *)format_list);
}
/**
 * Sends a clipboard data request for the given format.
 *
 * @param client
 *     The guac_client associated with the current RDP session.
 *
 * @param format
 *     The clipboard format to request. This format must be one of the
 *     documented values used by the CLIPRDR channel for clipboard format IDs.
 */
static void __guac_rdp_cb_request_format(guac_client *client, int format)
{
    guac_rdp_client *rdp_client = (guac_rdp_client *)client->data;
    rdpChannels *channels = rdp_client->rdp_inst->context->channels;

    /* Create new data request */
    RDP_CB_DATA_REQUEST_EVENT *data_request =
        (RDP_CB_DATA_REQUEST_EVENT *)freerdp_event_new(
            CliprdrChannel_Class,
            CliprdrChannel_DataRequest,
            NULL, NULL);

    /* Set to requested format */
    rdp_client->requested_clipboard_format = format;
    data_request->format = format;

    /* Send request */
    freerdp_channels_send_event(channels, (wMessage *)data_request);
}
void guac_rdp_process_cb_format_list(guac_client *client,
                                     RDP_CB_FORMAT_LIST_EVENT *event)
{

    int formats = 0;

    /* Received notification of available data */

    int i;
    for (i = 0; i < event->num_formats; i++)
    {
        /* If plain text available, request it */
        if (event->formats[i] == CB_FORMAT_TEXT)
            formats |= GUAC_RDP_CLIPBOARD_FORMAT_CP1252;
        else if (event->formats[i] == CB_FORMAT_UNICODETEXT)
            formats |= GUAC_RDP_CLIPBOARD_FORMAT_UTF16;
    }

    /* Prefer Unicode to plain text */
    if (formats & GUAC_RDP_CLIPBOARD_FORMAT_UTF16)
    {
        __guac_rdp_cb_request_format(client, CB_FORMAT_UNICODETEXT);
        return;
    }

    /* Use plain text if Unicode unavailable */
    if (formats & GUAC_RDP_CLIPBOARD_FORMAT_CP1252)
    {
        __guac_rdp_cb_request_format(client, CB_FORMAT_TEXT);
        return;
    }

    /* Ignore if no supported format available */
    guac_client_log(client, GUAC_LOG_INFO, "Ignoring unsupported clipboard data");
}
void guac_rdp_process_cb_data_request(guac_client *client,
                                      RDP_CB_DATA_REQUEST_EVENT *event)
{
    guac_rdp_client *rdp_client = (guac_rdp_client *)client->data;
    rdpChannels *channels = rdp_client->rdp_inst->context->channels;

    guac_iconv_write *writer;
    const char *input = rdp_client->clipboard->buffer;
    char *output = malloc(GUAC_RDP_CLIPBOARD_MAX_LENGTH);

    RDP_CB_DATA_RESPONSE_EVENT *data_response;
    /* Determine output encoding */
    switch (event->format)
    {

    case CB_FORMAT_TEXT:
        writer = GUAC_WRITE_CP1252;
        break;

    case CB_FORMAT_UNICODETEXT:
        writer = GUAC_WRITE_UTF16;
        break;

    default:
        guac_client_log(client, GUAC_LOG_ERROR,
                        "Server requested unsupported clipboard data type");
        free(output);
        return;
    }
    /* Create new data response */
    data_response = (RDP_CB_DATA_RESPONSE_EVENT *)freerdp_event_new(
        CliprdrChannel_Class,
        CliprdrChannel_DataResponse,
        NULL, NULL);

    /* Set data and size */
    data_response->data = (BYTE *)output;
    guac_iconv(GUAC_READ_UTF8, &input, rdp_client->clipboard->length,
               writer, &output, GUAC_RDP_CLIPBOARD_MAX_LENGTH);
    data_response->size = ((BYTE *)output) - data_response->data;

    /* Send response */
    freerdp_channels_send_event(channels, (wMessage *)data_response);
}
void guac_rdp_process_cb_data_response(guac_client *client,
                                       RDP_CB_DATA_RESPONSE_EVENT *event)
{
    guac_rdp_client *rdp_client = (guac_rdp_client *)client->data;
    char received_data[GUAC_RDP_CLIPBOARD_MAX_LENGTH];

    /* Ignore received text if outbound clipboard transfer is disabled */
    if (rdp_client->settings->disable_copy)
        return;

    guac_iconv_read *reader;
    const char *input = (char *)event->data;
    char *output = received_data;

    /* Find correct source encoding */
    switch (rdp_client->requested_clipboard_format)
    {

    /* Non-Unicode */
    case CB_FORMAT_TEXT:
        reader = GUAC_READ_CP1252;
        break;

    /* Unicode (UTF-16) */
    case CB_FORMAT_UNICODETEXT:
        reader = GUAC_READ_UTF16;
        break;

    default:
        guac_client_log(client, GUAC_LOG_ERROR, "Requested clipboard data in "
                                                "unsupported format %i",
                        rdp_client->requested_clipboard_format);
        return;
    }

    /* Convert send clipboard data */
    if (guac_iconv(reader, &input, event->size,
                   GUAC_WRITE_UTF8, &output, sizeof(received_data)))
    {

        int length = strnlen(received_data, sizeof(received_data));
        guac_common_clipboard_reset(rdp_client->clipboard, "text/plain");
        guac_common_clipboard_append(rdp_client->clipboard, received_data, length);
        guac_common_clipboard_send(rdp_client->clipboard, client);
    }
}
#endif
#ifdef FREERDP_2
static UINT guac_rdp_clipboard_send_client_capabilities(CliprdrClientContext *clipboard)
{
    CLIPRDR_CAPABILITIES capabilities;
    CLIPRDR_GENERAL_CAPABILITY_SET generalCapabilitySet;

    if (!clipboard || !clipboard->ClientCapabilities)
        return ERROR_INTERNAL_ERROR;

    capabilities.cCapabilitiesSets = 1;
    capabilities.capabilitySets = (CLIPRDR_CAPABILITY_SET *)&(generalCapabilitySet);
    generalCapabilitySet.capabilitySetType = CB_CAPSTYPE_GENERAL;
    generalCapabilitySet.capabilitySetLength = 12;
    generalCapabilitySet.version = CB_CAPS_VERSION_2;
    generalCapabilitySet.generalFlags = CB_USE_LONG_FORMAT_NAMES | CB_STREAM_FILECLIP_ENABLED | CB_FILECLIP_NO_FILE_PATHS;
    return clipboard->ClientCapabilities(clipboard, &capabilities);
}
static UINT guac_rdp_clipboard_send_client_format_list(CliprdrClientContext *clipboard)
{
    CLIPRDR_FORMAT_LIST formatList;
    formatList.msgFlags = CB_RESPONSE_OK;
    formatList.msgType = CB_FORMAT_LIST;
    formatList.dataLen = (4 + 1) * TEXT_FORMATS_COUNT;
    formatList.formats = guac_text_formats;
    formatList.numFormats = TEXT_FORMATS_COUNT;
    return clipboard->ClientFormatList(clipboard, &formatList);
}
static UINT __guac_rdp_clipboard_request_format(guac_rdp_client *client, CliprdrClientContext *clipboard, UINT32 formatId)
{
    UINT rc;
    CLIPRDR_FORMAT_DATA_REQUEST formatDataRequest;

    if (!clipboard || !clipboard->ClientFormatDataRequest)
        return ERROR_INTERNAL_ERROR;

    formatDataRequest.requestedFormatId = formatId;
    client->requested_clipboard_format = formatId;
    rc = clipboard->ClientFormatDataRequest(clipboard, &formatDataRequest);
    return rc;
}
static UINT guac_rdp_clipboard_monitor_ready(CliprdrClientContext *context,CLIPRDR_MONITOR_READY *monitorReady)
{
    UINT ret;
    WINPR_UNUSED(monitorReady);

    if ((ret = guac_rdp_clipboard_send_client_capabilities(context)) != CHANNEL_RC_OK)
        return ret;

    if ((ret = guac_rdp_clipboard_send_client_format_list(context)) != CHANNEL_RC_OK)
        return ret;

    return CHANNEL_RC_OK;
}
static UINT guac_rdp_clipboard_format_list(CliprdrClientContext *clipboard, CLIPRDR_FORMAT_LIST *formatList)
{
    int formats = 0;
    /* Received notification of available data */
    int i;
    for (i = 0; i < formatList->numFormats; i++)
    {
        /* If plain text available, request it */
        if (formatList->formats[i].formatId == CF_TEXT)
            formats |= GUAC_RDP_CLIPBOARD_FORMAT_CP1252;
        else if (formatList->formats[i].formatId == CF_UNICODETEXT)
            formats |= GUAC_RDP_CLIPBOARD_FORMAT_UTF16;
    }
    rdp_freerdp_context *context = clipboard->custom;
    guac_rdp_client *rdp_client = (guac_rdp_client *)(context->client->data);
    /* Prefer Unicode to plain text */
    if (formats & GUAC_RDP_CLIPBOARD_FORMAT_UTF16)
    {
        __guac_rdp_clipboard_request_format(rdp_client, clipboard, CF_UNICODETEXT);
        return 0;
    }

    /* Use plain text if Unicode unavailable */
    if (formats & GUAC_RDP_CLIPBOARD_FORMAT_CP1252)
    {
        __guac_rdp_clipboard_request_format(rdp_client, clipboard, CF_TEXT);
        return 0;
    }
    /* Ignore if no supported format available */
    guac_client_log(context->client, GUAC_LOG_INFO, "Ignoring unsupported clipboard data");
    return 0;
}
static UINT guac_rdp_clipboard_data_request(CliprdrClientContext *clipboard, CLIPRDR_FORMAT_DATA_REQUEST *formatDataRequest)
{
    rdp_freerdp_context *context =  (rdp_freerdp_context *)(clipboard->custom);
    guac_rdp_client *client = (guac_rdp_client*)context->client->data;
    guac_iconv_write *writer;
    const char *input = client->clipboard->buffer;
    char *output = malloc(GUAC_RDP_CLIPBOARD_MAX_LENGTH);
    /* Determine output encoding */
    switch (formatDataRequest->requestedFormatId)
    {
    case CF_TEXT:
        writer = GUAC_WRITE_CP1252;
        break;
    case CF_UNICODETEXT:
        writer = GUAC_WRITE_UTF16;
        break;
    default:
        guac_client_log(context->client, GUAC_LOG_ERROR,"Server requested unsupported clipboard data type");
        free(output);
        return 0;
    }
    CLIPRDR_FORMAT_DATA_RESPONSE response;
    response.msgFlags = CB_RESPONSE_OK;
    response.requestedFormatData = (BYTE *)output;
    guac_iconv(GUAC_READ_UTF8, &input, client->clipboard->length,
               writer, &output, GUAC_RDP_CLIPBOARD_MAX_LENGTH);
    response.dataLen = (BYTE *)output - response.requestedFormatData;
    UINT rc = clipboard->ClientFormatDataResponse(clipboard, &response);
    free(output);
    return rc;
}
static UINT guac_rdp_clipboard_data_response(CliprdrClientContext *clipboard,CLIPRDR_FORMAT_DATA_RESPONSE *formatDataResponse)
{
    char received_data[GUAC_RDP_CLIPBOARD_MAX_LENGTH];
    rdp_freerdp_context *context =  (rdp_freerdp_context *)(clipboard->custom);
    guac_rdp_client *client = (guac_rdp_client*)context->client->data;
    /* Ignore received text if outbound clipboard transfer is disabled */
    if (client->settings->disable_copy)
        return 0;

    guac_iconv_read *reader;
    const char *input = (char *)formatDataResponse->requestedFormatData;
    char *output = received_data;

    /* Find correct source encoding */
    switch (client->requested_clipboard_format)
    {
    /* Non-Unicode */
    case CF_TEXT:
        reader = GUAC_READ_CP1252;
        break;

    /* Unicode (UTF-16) */
    case CF_UNICODETEXT:
        reader = GUAC_READ_UTF16;
        break;
    default:
        guac_client_log(context->client, GUAC_LOG_ERROR, "Requested clipboard data in "
                                                "unsupported format %i",
                        client->requested_clipboard_format);
        return 0;
    }

    /* Convert send clipboard data */
    if (guac_iconv(reader, &input, formatDataResponse->dataLen,
                   GUAC_WRITE_UTF8, &output, sizeof(received_data)))
    {

        int length = strnlen(received_data, sizeof(received_data));
        guac_common_clipboard_reset(client->clipboard, "text/plain");
        guac_common_clipboard_append(client->clipboard, received_data, length);
        guac_common_clipboard_send(client->clipboard, context->client);
    }else{
        free(output);
    }
    return CHANNEL_RC_OK;
}
void guac_rdp_cliprdr_init(rdp_freerdp_context *context, CliprdrClientContext *cliprdr)
{
    context->cliprdrContext = cliprdr;
    cliprdr->custom = context;
    cliprdr->MonitorReady = guac_rdp_clipboard_monitor_ready;
    //cliprdr->ServerCapabilities = ;
    cliprdr->ServerFormatList = guac_rdp_clipboard_format_list;
    //cliprdr->ServerFormatListResponse = ;
    //cliprdr->ServerLockClipboardData=;
    //cliprdr->ServerUnlockClipboardData=;
    cliprdr->ServerFormatDataRequest = guac_rdp_clipboard_data_request;
    cliprdr->ServerFormatDataResponse = guac_rdp_clipboard_data_response;
}
#endif