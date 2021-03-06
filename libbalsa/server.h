/* -*-mode:c; c-style:k&r; c-basic-offset:4; -*- */
/* Balsa E-Mail Client
 *
 * Copyright (C) 1997-2016 Stuart Parmenter and others,
 *                         See the file AUTHORS for a list.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option) 
 * any later version.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  
 * GNU General Public License for more details.
 *  
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __LIBBALSA_SERVER_H__
#define __LIBBALSA_SERVER_H__

#ifndef BALSA_VERSION
# error "Include config.h before this file."
#endif

#include "imap/libimap.h"
#include "libbalsa.h"
#include "net-client.h"

#define LIBBALSA_TYPE_SERVER \
    (libbalsa_server_get_type())
#define LIBBALSA_SERVER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST (obj, LIBBALSA_TYPE_SERVER, LibBalsaServer))
#define LIBBALSA_SERVER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST (klass, LIBBALSA_TYPE_SERVER, \
                              LibBalsaServerClass))
#define LIBBALSA_IS_SERVER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE (obj, LIBBALSA_TYPE_SERVER))
#define LIBBALSA_IS_SERVER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE (klass, LIBBALSA_TYPE_SERVER))

GType libbalsa_server_get_type(void);

typedef struct _LibBalsaServerClass LibBalsaServerClass;


struct _LibBalsaServer {
    GObject object;
    const gchar *protocol; /**< type of the server: imap, pop3, or smtp. */

    gchar *host;
    gchar *user;
    gchar *passwd;
    NetClientCryptMode security;
    gboolean client_cert;
    gchar *cert_file;
    gchar *cert_passphrase;
    gboolean remember_passwd;
    gboolean remember_cert_passphrase;
    gboolean try_anonymous; /* user wants anonymous access */
};

struct _LibBalsaServerClass {
    GObjectClass parent_class;

    void (*set_username) (LibBalsaServer * server, const gchar * name);
    void (*set_host) (LibBalsaServer * server,
		      const gchar * host, NetClientCryptMode  security);
    void (*config_changed) (LibBalsaServer * server);
};

LibBalsaServer *libbalsa_server_new(void);

void libbalsa_server_set_username(LibBalsaServer * server,
				  const gchar * username);
void libbalsa_server_set_password(LibBalsaServer *server,
				  	  	  	  	  const gchar    *passwd,
								  gboolean        for_cert);
void libbalsa_server_set_host(LibBalsaServer     *server,
							  const gchar        *host,
							  NetClientCryptMode  security);

void libbalsa_server_config_changed(LibBalsaServer * server);
void libbalsa_server_load_config(LibBalsaServer * server);
void libbalsa_server_load_security_config(LibBalsaServer * server);
void libbalsa_server_save_config(LibBalsaServer * server);


/* NetClient related signal handlers */
gchar **libbalsa_server_get_auth(NetClient *client,
								 gboolean   need_passwd,
								 gpointer   user_data);
gboolean libbalsa_server_check_cert(NetClient           *client,
           	   	   	   	   	   	    GTlsCertificate     *peer_cert,
									GTlsCertificateFlags errors,
									gpointer             user_data);
gchar *libbalsa_server_get_cert_pass(NetClient *client,
									 gchar     *cert_subject,
									 gpointer   user_data);

void libbalsa_server_connect_signals(LibBalsaServer * server, GCallback cb,
                                     gpointer cb_data);

/* Check whether a server can be reached */

void libbalsa_server_test_can_reach(LibBalsaServer           * server,
                                    LibBalsaCanReachCallback * cb,
                                    gpointer                   cb_data);

/* Private: used only by LibBalsaMailboxRemote */
void libbalsa_server_test_can_reach_full(LibBalsaServer           * server,
                                         LibBalsaCanReachCallback * cb,
                                         gpointer                   cb_data,
                                         GObject                  * source_object);

#endif				/* __LIBBALSA_SERVER_H__ */
