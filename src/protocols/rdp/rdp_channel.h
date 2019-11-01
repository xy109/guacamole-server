#include "config.h"

#include <freerdp/freerdp.h>
#include <guacamole/client.h>
#include <freerdp/client/channels.h>
#include <freerdp/gdi/video.h>

void guac_rdp_channel_connected(rdpContext *context,ChannelConnectedEventArgs *e);
void guac_rdp_channel_disconnected(rdpContext* context, ChannelDisconnectedEventArgs* e);
PVIRTUALCHANNELENTRY guac_freerdp_channels_load_static_addin_entry(LPCSTR pszName, LPSTR pszSubsystem, LPSTR pszType, DWORD dwFlags);