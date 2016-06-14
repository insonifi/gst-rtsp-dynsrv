/* Copyright (C) 2006 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2015 Andrey Panteleyev <insonifi gmail com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

#define DEFAULT_RTSP_PORT "8554"
#define DEFAULT_SEP "`"
#define DEFAULT_MCAST_TTL 1
#define DEFAULT_FMCAST FALSE
#define MCAST "mcast"
#define QSIGN "?"

static char *port = (char *) DEFAULT_RTSP_PORT;
static char *sep = (char *) DEFAULT_SEP;
static guint8 mttl = (guint8) DEFAULT_MCAST_TTL;
static gboolean *force_mcast = (gboolean *) DEFAULT_FMCAST;
static GOptionEntry entries[] = {
  {"port", 'p', 0, G_OPTION_ARG_STRING, &port,
      "Port to listen on (default: " DEFAULT_RTSP_PORT ")", "PORT"},
  {"sep", 's', 0, G_OPTION_ARG_STRING, &sep,
      "Separator used to replace space character "
        "in launch strings (default: " DEFAULT_SEP ")", "SEP"},
  {"force-mcast", 'm', 0, G_OPTION_ARG_NONE, &force_mcast,
      "Force streams to use multicast", NULL},
  {"mcast-ttl", 't', 0, G_OPTION_ARG_INT, &mttl,
      "Multicast TTL", NULL},
  {NULL}
};

static gchar *
path_to_launch (const gchar * path)
{
  gchar *result = NULL;
  gchar new_delim = ' ';
  
  result = g_strdelimit (g_strdup (path + 1), sep, new_delim);
  
  return result;
}

static void
media_constructed (GstRTSPMediaFactory * factory, GstRTSPMedia * media)
{
  guint i, n_streams;

  n_streams = gst_rtsp_media_n_streams (media);

  for (i = 0; i < n_streams; i++) {
    GstRTSPAddressPool *pool;
    GstRTSPStream *stream;
    gchar *min, *max;

    stream = gst_rtsp_media_get_stream (media, i);

    /* make a new address pool */
    pool = gst_rtsp_address_pool_new ();

    min = g_strdup_printf ("224.3.0.%d", (2 * i) + 1);
    max = g_strdup_printf ("224.3.0.%d", (2 * i) + 2);
    gst_rtsp_address_pool_add_range (pool, min, max,
                                      5000 + (10 * i), 5010 + (10 * i), mttl);
    g_free (min);
    g_free (max);

    gst_rtsp_stream_set_address_pool (stream, pool);
    g_object_unref (pool);
  }
}

static void
handle_client (GstRTSPClient * client, GstRTSPContext * ctx,
    GstRTSPServer * server, gpointer user_data)
{
  GstRTSPClientClass *klass;
  GstRTSPMountPoints *mounts;
  GstRTSPMediaFactory *factory;
  GstRTSPUrl *uri;
  gchar *path;
  gchar *launch;

  uri = ctx->uri;

  if (!uri)
    return;

  klass = GST_RTSP_CLIENT_GET_CLASS (client);
  path = klass->make_path_from_uri (client, uri);

  launch = path_to_launch (path);
  mounts = gst_rtsp_server_get_mount_points (server);
  factory = gst_rtsp_mount_points_match (mounts, path, NULL);
  if (!factory)
  {
    factory = gst_rtsp_media_factory_new ();
    gst_rtsp_media_factory_set_launch (factory, launch);
    gst_rtsp_media_factory_set_shared (factory, TRUE);
    if (force_mcast || g_str_has_suffix (path, MCAST))
    {
      g_print ("multicast ");
      g_signal_connect (factory, "media-constructed", (GCallback)
                                                      media_constructed, NULL);
      /* only allow multicast */
      gst_rtsp_media_factory_set_protocols (factory,
                                            GST_RTSP_LOWER_TRANS_UDP_MCAST);
    }
    gst_rtsp_mount_points_add_factory (mounts, path, factory);
    g_print ("new factory: %s\n", launch);
  }
  else
  {
    g_object_unref (factory);
  }
  g_object_unref (mounts);
  g_free (path);
  g_free (launch);
}

static void
client_connected (GstRTSPServer * server,
    GstRTSPClient * client, gpointer user_data)
{
  g_signal_connect_object (client, "options-request", (GCallback)
      handle_client, server, G_CONNECT_AFTER);
}

static gboolean
timeout (GstRTSPServer * server)
{
  GstRTSPSessionPool *pool;

  pool = gst_rtsp_server_get_session_pool (server);
  gst_rtsp_session_pool_cleanup (pool);
  g_object_unref (pool);

  return TRUE;
}

int
main (int argc, char *argv[])
{
  GMainLoop *loop;
  GstRTSPServer *server;
  GOptionContext *optctx;
  GError *error = NULL;

  optctx = g_option_context_new (NULL);
  g_option_context_add_main_entries (optctx, entries, NULL);
  g_option_context_add_group (optctx, gst_init_get_option_group ());
  if (!g_option_context_parse (optctx, &argc, &argv, &error)) {
    g_printerr ("Error parsing options: %s\n", error->message);
    return -1;
  }
  g_option_context_free (optctx);

  loop = g_main_loop_new (NULL, FALSE);

  /* create a server instance */
  server = gst_rtsp_server_new ();
  g_object_set (server, "service", port, NULL);

  /* attach the server to the default maincontext */
  gst_rtsp_server_attach (server, NULL);

  g_signal_connect (server, "client-connected", (GCallback)
      client_connected, NULL);

  g_timeout_add_seconds (2, (GSourceFunc) timeout, server);

  g_object_unref (server);

  /* start serving */
  g_print ("rtsp://127.0.0.1:%s/\n", port);
  g_main_loop_run (loop);

  return 0;
}
