/*
 * net.h
 *
 * Copyright 2011 Thomas L Dunnick
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
#ifndef __NET__
#define __NET__

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <winsock.h>
#include <openssl/ssl.h>

#define WSA_VERSION 0x202	/* missing in winsock?			*/
#define DFLTTIMEOUT 5000 	/* 5 second default receive timeout	*/

typedef struct netcon
{
  SOCKET sock;
  struct sockaddr_in sin;
  SSL *ssl;
} NETCON;

/*
 * call me first!
 */
int net_startup (void);
/*
 * call me last
 */
int net_shutdown (void);
/*
 * Set up an SSL context...
 * cert - certificate to present to peer (DER, PEM, or PKCS12)
 * key - private key for cert (can be in same file!)
 * passwd - password for cert/key
 * auth - CA chain of certificates (currently only PEM supported)
 * is_server - true if we are a server
 */
SSL_CTX *net_ctx (char *cert, char *key, char *passwd, char *auth, 
  int is_server);
/*
 * open a network connection...
 * host/port - our hostname/port if server, or host we want to connect to
 * listeners - 0 if client, or number of connections queued
 * ctx - SSL context if a secure connection
 */
NETCON *net_open (char *hostname, int port, int listeners, SSL_CTX *ctx);
/*
 * accept a connection from a listener
 */
NETCON *net_accept (NETCON *conn, SSL_CTX *ctx);
/*
 * Set a connection specific ssl context. Normally called from net_open()
 * or net_accept() rather than directly.
 */
NETCON *net_ssl (NETCON *conn, SSL_CTX *ctx, int is_server);
/*
 * return connecting host name
 */
char *net_remotehost (NETCON *conn, char *buf);
/*
 * return true if connection host is localhost
 */
int net_localhost (NETCON *conn);
/*
 * set receive timeout - see DFLTTIMEOUT above
 */
int net_timeout (NETCON *conn, int timeout);
/*
 * read from a connection
 */
int net_read (NETCON *conn, char *buf, int sz);
/*
 * write to a connection
 */
int net_write (NETCON *conn, char *buf, int sz);
/*
 * close a connection
 */
NETCON *net_close (NETCON *conn);

#endif /* __NET__ */
