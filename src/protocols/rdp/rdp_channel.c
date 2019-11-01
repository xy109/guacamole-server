#include "rdp_channel.h"
#include "rdp.h"
#ifdef FREERDP_2
#include <freerdp/addin.h>
#include <freerdp/client/channels.h>
#include <freerdp/channels/channels.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/client/rdpei.h>
#include <freerdp/client/rail.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/client/rdpgfx.h>
#include <freerdp/client/encomsp.h>
#include <freerdp/client/disp.h>
#include <freerdp/client/tsmf.h>
#include <freerdp/client/geometry.h>
#include <freerdp/client/video.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/codec/color.h>
#include "rdp_cliprdr.h"
// static CHANNEL_OPEN_DATA* freerdp_channels_find_channel_open_data_by_name(
//     rdpChannels* channels, const char* name)
// {
// 	int index;
// 	CHANNEL_OPEN_DATA* pChannelOpenData;

// 	for (index = 0; index < channels->openDataCount; index++)
// 	{
// 		pChannelOpenData = &channels->openDataList[index];

// 		if (strncmp(name, pChannelOpenData->name, CHANNEL_NAME_LEN) == 0)
// 			return pChannelOpenData;
// 	}

// 	return NULL;
// }
/**
 * Called whenever a channel connects via the PubSub event system within
 * FreeRDP.
 *
 * @param context
 *     The rdpContext associated with the active RDP session.
 *
 * @param e
 *     Event-specific arguments, mainly the name of the channel, and a
 *     reference to the associated plugin loaded for that channel by FreeRDP.
 */
void guac_rdp_channel_connected(rdpContext *context,
								ChannelConnectedEventArgs *e)
{
	guac_client *client = ((rdp_freerdp_context *)context)->client;
	guac_rdp_client *rdp_client = (guac_rdp_client *)client->data;
	guac_rdp_settings *settings = rdp_client->settings;
	guac_client_log(client, GUAC_LOG_DEBUG, "guac_rdp_channel_connected:%s", e->name);
	if (strcmp(e->name, RDPEI_DVC_CHANNEL_NAME) == 0)
	{
		//xfc->rdpei = (RdpeiClientContext*)e->pInterface;
	}
	else if (strcmp(e->name, TSMF_DVC_CHANNEL_NAME) == 0)
	{
		//xf_tsmf_init(xfc, (TsmfClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0)
	{
		//xf_graphics_pipeline_init(xfc, (RdpgfxClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, RAIL_SVC_CHANNEL_NAME) == 0)
	{
		//xf_rail_init(xfc, (RailClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
		guac_rdp_cliprdr_init((rdp_freerdp_context *)context, (CliprdrClientContext *)e->pInterface);
	}
	else if (strcmp(e->name, ENCOMSP_SVC_CHANNEL_NAME) == 0)
	{
		//xf_encomsp_init(xfc, (EncomspClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, DISP_DVC_CHANNEL_NAME) == 0)
	{
		if (settings->resize_method == GUAC_RESIZE_DISPLAY_UPDATE)
		{
			/* Store reference to the display update plugin once it's connected */
			if (strcmp(e->name, DISP_DVC_CHANNEL_NAME) == 0)
			{

				DispClientContext *disp = (DispClientContext *)e->pInterface;
				/* Init module with current display size */
				guac_rdp_disp_set_size(rdp_client->disp, rdp_client->settings,
									   context->instance, guac_rdp_get_width(context->instance),
									   guac_rdp_get_height(context->instance));

				/* Store connected channel */
				guac_rdp_disp_connect(rdp_client->disp, disp);
				guac_client_log(client, GUAC_LOG_DEBUG,
								"Display update channel connected.");
			}
		}
	}
	else if (strcmp(e->name, GEOMETRY_DVC_CHANNEL_NAME) == 0)
	{
		//gdi_video_geometry_init(xfc->context.gdi, (GeometryClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, VIDEO_CONTROL_DVC_CHANNEL_NAME) == 0)
	{
		//xf_video_control_init(xfc, (VideoClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, VIDEO_DATA_DVC_CHANNEL_NAME) == 0)
	{
		//gdi_video_data_init(xfc->context.gdi, (VideoClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, "rdpdr") == 0)
	{
		//查找对象
		void *data = freerdp_channels_get_static_channel_interface(context->channels, e->name);
		guac_client_log(client, GUAC_LOG_DEBUG, "guac_rdp_channel_connected:%s", e->name, sizeof(data));
	}
}
void guac_rdp_channel_disconnected(rdpContext *context, ChannelDisconnectedEventArgs *e)
{
	//guac_client* client = ((rdp_freerdp_context*)context)->client;
	//guac_rdp_client* rdp_client = (guac_rdp_client*)client->data;
	//guac_rdp_settings* settings = rdp_client->settings;

	if (strcmp(e->name, RDPEI_DVC_CHANNEL_NAME) == 0)
	{
		//xfc->rdpei = NULL;
	}
	else if (strcmp(e->name, DISP_DVC_CHANNEL_NAME) == 0)
	{
		//xf_disp_uninit(xfc->xfDisp, (DispClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, TSMF_DVC_CHANNEL_NAME) == 0)
	{
		//xf_tsmf_uninit(xfc, (TsmfClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0)
	{
		//xf_graphics_pipeline_uninit(xfc, (RdpgfxClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, RAIL_SVC_CHANNEL_NAME) == 0)
	{
		//xf_rail_uninit(xfc, (RailClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
		//xf_cliprdr_uninit(xfc, (CliprdrClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, ENCOMSP_SVC_CHANNEL_NAME) == 0)
	{
		//xf_encomsp_uninit(xfc, (EncomspClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, GEOMETRY_DVC_CHANNEL_NAME) == 0)
	{
		//gdi_video_geometry_uninit(xfc->context.gdi, (GeometryClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, VIDEO_CONTROL_DVC_CHANNEL_NAME) == 0)
	{
		//if (settings->SoftwareGdi)
		//	gdi_video_control_uninit(xfc->context.gdi, (VideoClientContext*)e->pInterface);
		//else
		//	xf_video_control_uninit(xfc, (VideoClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, VIDEO_DATA_DVC_CHANNEL_NAME) == 0)
	{
		//gdi_video_data_uninit(xfc->context.gdi, (VideoClientContext*)e->pInterface);
	}
}

PVIRTUALCHANNELENTRY guac_freerdp_channels_load_static_addin_entry(LPCSTR pszName, LPSTR pszSubsystem, LPSTR pszType, DWORD dwFlags)
{
	//打印驱动
	if (strcmp(pszName, "printer") == 0)
	{
		//return freerdp_load_dynamic_channel_addin_entry(pszName,pszSubsystem,pszType,dwFlags);
		if (pszSubsystem == 0 || strlen(pszSubsystem) < 1)
		{
			return freerdp_load_dynamic_channel_addin_entry("guacprinter",pszSubsystem, pszType, dwFlags);
		}
	}
	return freerdp_channels_load_static_addin_entry(pszName, pszSubsystem, pszType, dwFlags);
}
#endif // FREERDP_2