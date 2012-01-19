/*
 * net.c
 *
 * Copyright 2011-2012 Thomas L Dunnick
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

#include "log.h"
#include "net.h"
#include "openssl/err.h"
#include "crypt.h"

#ifdef UNITTEST
#undef UNITTEST
#include "unittest.h"
#include "crypt.c"
#define UNITTEST
#define debug _DEBUG_
#endif

#ifndef debug
#define debug(fmt...)
#endif

#define REASON ERR_error_string (ERR_get_error (), NULL)
#define SSLREASON ssl_error (conn->ssl)

/*
 * get ssl error message
 */
const char *ssl_error (SSL *ssl)
{
  switch (SSL_get_error (ssl, 0))
  {
    case SSL_ERROR_NONE : return ("No error");
    case SSL_ERROR_ZERO_RETURN : return ("Connection closed");
    case SSL_ERROR_WANT_READ : return ("Waiting Read");
    case SSL_ERROR_WANT_WRITE : return ("Waiting Write");
    case SSL_ERROR_WANT_CONNECT : return ("Not Connected");
    case SSL_ERROR_WANT_ACCEPT : return ("Not Accepted");
    case SSL_ERROR_WANT_X509_LOOKUP : return ("Waiting Auth Callback");
    case SSL_ERROR_SYSCALL : return ("IO Error");
    case SSL_ERROR_SSL : return ("Protocol Error");
    default : break;
  }
  return ("Unknown");
}

/*
 * initialize networking
 */
int net_startup (void)
{
  WSADATA wsadata;

  if ((gethostbyname ("localhost") != NULL) ||
      (WSAGetLastError () != WSANOTINITIALISED))
  {
    debug ("WINSOCK/SSL already initialized\n");
    return (0);
  }
  debug ("initializing WINSOCK...\n");
  if (WSAStartup (WSA_VERSION, &wsadata))
  {
    error ("Failed Windows Socket Startup()!\n");
    return (-1);
  }
  SSL_load_error_strings();
  SSL_library_init();
  OpenSSL_add_all_ciphers();
  return 0;
}

/*
 * shut down networking
 */
int net_shutdown (void)
{
  WSACleanup ();
  return (0);
}

/*
 * Get an ssl context.  Note that if an auth file is provided,
 * peer verification is turned on automatically.
 */
SSL_CTX *net_ctx (char *cert, char *key, char *passwd,
    char *auth, int is_server)
{
  SSL_CTX *ctx;
  EVP_PKEY *pkey;
  X509 *x509;
  int e;

  debug ("initializing SSL\n");
  if (is_server)
    ctx = (SSL_CTX *) SSL_CTX_new (SSLv23_server_method());
  else
    ctx = (SSL_CTX *) SSL_CTX_new (SSLv23_client_method());
  if (ctx == NULL)
  {
    error ("Can't initialize SSL context - %s\n", REASON);
    return (NULL);
  }
  if ((cert != NULL) && *cert)
  {
    debug ("loading certificate...\n");
    if ((x509 = crypt_get_X509 (cert, passwd)) == NULL)
    {
      error ("Can't get certificate %s\n", cert);
      goto fail;
    }
    SSL_CTX_set_default_passwd_cb_userdata (ctx, passwd);
    e = SSL_CTX_use_certificate (ctx, x509);
    X509_free (x509);
    if (!e)
    {
      error ("Can't use certificate %s - %s\n", cert, REASON);
      goto fail;
    }
  }
  if ((key != NULL) && *key)
  {
    debug ("loading private key...\n");
    if ((pkey = crypt_get_pkey (cert, passwd)) == NULL)
    {
      error ("Can't get key %s\n", cert);
      goto fail;
    }
    e = SSL_CTX_use_PrivateKey (ctx, pkey);
    EVP_PKEY_free (pkey);
    if (!e)
    {
      error ("Can't use key %s - %s\n", cert, REASON);
      goto fail;
    }
  }
  /*
   * Note that a if a certificate authority chain is provided, we
   * turn on forced verification and set the depth to 1 (e.g. signer
   * must be a root authority.  Currently only PEM CA's are supported.
   */
  if ((auth != NULL) && *auth)
  {
    debug ("loading authority...\n");
    if (!SSL_CTX_load_verify_locations (ctx, auth, 0))
    {
      error ("Can't use authority %s - %s\n", auth, REASON);
      goto fail;
    }
    SSL_CTX_set_verify (ctx, SSL_VERIFY_PEER, NULL);
#if (OPENSSL_VERSION_NUMBER < 0x00905100L)
    SSL_CTX_set_verify_depth (ctx, 1);
#endif
  }
  return (ctx);

fail:
  SSL_CTX_free (ctx);
  return (NULL);
}

/*
 * open a network connection - if we are a server, set it to listen,
 * otherwise go ahead and connect and complete SSL negotiations
 */
NETCON *net_open (char *hostname, int port, int listeners, SSL_CTX *ctx)
{
  struct hostent *hp;
  NETCON *conn;

  conn = (NETCON *) malloc (sizeof (NETCON));
  memset (conn, 0, sizeof (NETCON));

  if ((conn->sock = socket (AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
  {
    error ("Can't get socket\n");
    free (conn);
    return (NULL);
  }
  conn->sin.sin_family = AF_INET;
  if (strcmpi ("ANY", hostname) == 0)
    conn->sin.sin_addr.s_addr = htonl (INADDR_ANY);
  else
  {
    if ((hp = gethostbyname (hostname)) == NULL)
    {
      error ("Can't get hostent for %s\n", hostname);
      free (conn);
      return (NULL);
    }
    memmove ((char *)&conn->sin.sin_addr, hp->h_addr, hp->h_length);
  }
  conn->sin.sin_port = htons ((unsigned short) port);
  if (listeners)
  {
    if (bind (conn->sock, (struct sockaddr *) &conn->sin, 
      sizeof (conn->sin)) < 0)
    {
      error ("Can't bind socket to port %d\n", port);
      free (conn);
      return (NULL);
    }
    if (listen (conn->sock, listeners) < 0)
    {
      closesocket (conn->sock);
      free (conn);
      error ("Failed listen on socket port %d\n", port);
      return (NULL);
    }
  }
  else
  {
    if (connect (conn->sock, (struct sockaddr *) &conn->sin, 
      sizeof (conn->sin)) < 0)
    {
      free (conn);
      error ("Can't bind socket to port %d\n", port);
      return (NULL);
    }
    net_timeout (conn, DFLTTIMEOUT);
    conn = net_ssl (conn, ctx, 0);
  }
  return (conn);
}

/*
 * accept a connection on a listen socket, set default read timeout,
 * and perform SSL negotiations
 */
NETCON *net_accept (NETCON *conn, SSL_CTX *ctx)
{
  int e;
  NETCON *c;

  c = (NETCON *) malloc (sizeof (NETCON));
  memset (c, 0, sizeof (NETCON));

  c->sock = accept (conn->sock, (struct sockaddr *) &c->sin, &e);
  if (c->sock == INVALID_SOCKET)
  {
    if (h_errno)
      error ("socket accept error %d - %s\n", h_errno, strerror (h_errno));
    free (c);
    return (NULL);
  }
  net_timeout (c, DFLTTIMEOUT);
  c = net_ssl (c, ctx, 1);
  return (c);
}

/*
 * perform SSL negotiations on a network connection
 */
NETCON *net_ssl (NETCON *conn, SSL_CTX *ctx, int is_server)
{
  int e;

  if (ctx == NULL)	/* not ssl connection, do nothing	*/
    return (conn);
  debug ("negotiating SSL connection\n");
  if ((conn->ssl = (SSL *) SSL_new (ctx)) == NULL)
  {
    error ("Can't establish SSL context - %s\n", REASON);
    return (net_close (conn));
  }
  if (SSL_set_fd (conn->ssl, conn->sock) == 0)
  {
    error ("Can't set SSL socket - %s\n", SSLREASON);
    return (net_close (conn));
  }
  if (is_server)
  {
    /* if we wanted to over ride the CTX verify setting...
     * SSL_set_verify (ssl, SSL_VERIFY_NONE, NULL);
     */
    if ((e = SSL_accept (conn->ssl)) != 1)
    {
      if (e)
        error ("Can't complete SSL accept - %s\n", SSLREASON);
      else
	debug ("EOF on SSL_accept\n");
      return (net_close (conn));
    }
  }
  else
  {
    if ((e = SSL_connect (conn->ssl)) != 1)
    {
      error ("Can't complete SSL connection - %s\n", SSLREASON);
      return (net_close (conn));
    }
    /* if we wanted to force a verify check...
     * if (SSL_get_verify_result (conn->ssl) != X509_V_OK)
     * {
     *   error ("Server certificate did not verify\n");
     *   return (net_close (conn));
     * }
     *
     * We could also check that the CN matches the hostname...
     */
    
  }
  return (conn);
}

/*
 * return connecting host name
 */
char *net_remotehost (NETCON *conn, char *buf)
{
  struct hostent *ent;

  if (buf == NULL) 
    return ("");
  *buf = 0;
  if (conn != NULL)
  {
    ent = gethostbyaddr ((char *) &(conn->sin.sin_addr),
      sizeof (conn->sin.sin_addr), AF_INET);
    if (ent != NULL)
      strcpy (buf, ent->h_name);
  }
  return (buf);
}

/*
 * return true if connection host is localhost
 */
int net_localhost (NETCON *conn)
{
  char buf[MAX_PATH];

  return (strcmp (net_remotehost (conn, buf), "localhost") == 0);
}

/*
 * set a connection read timeout
 */
int net_timeout (NETCON *conn, int timeout)
{
  DWORD t;

  /* Set socket read time limit to select interval */
  t = timeout;
  if (setsockopt (conn->sock, SOL_SOCKET, SO_RCVTIMEO, 
    (char *) &t, sizeof(t)))
  {
    error ("Can't set timeout on socket!\n");
    return (-1);
  }
  return (0);
}

/*
 * read from a socket
 *
 * note that the other end may have closed the connection or we
 * may have timed out or when we get here, so we don't complain about
 * IO errors (SSL_ERROR_SYSCALL).
 */
int net_read (NETCON *conn, char *buf, int sz)
{
  int n, rsz;

  rsz = 0;
  while (n = sz - rsz)
  {
    if (conn->ssl != NULL)
    {
      if ((n = SSL_read (conn->ssl, buf + rsz, n)) <= 0)
      {
	int e = SSL_get_error (conn->ssl, n);
	if (e != SSL_ERROR_ZERO_RETURN)
	{
	  if (e != SSL_ERROR_SYSCALL) 
	    error ("read error %d - %s\n", e, SSLREASON);
	  else
	    debug ("read EOF\n");
	}
	break;
      }
    }
    else
    {
      if ((n = recv (conn->sock, buf + rsz, n, 0)) <= 0)
      {
        if (h_errno && (h_errno != WSAETIMEDOUT))
          error ("read error %d - %s\n", h_errno, strerror (h_errno));
        break;
      }
    }
    rsz += n;
  }
  return (rsz);
}

/*
 * write to a socket
 */
int net_write (NETCON *conn, char *buf, int sz)
{
  if (conn->ssl != NULL)
    sz = SSL_write (conn->ssl, buf, sz);
  else
    sz = send (conn->sock, buf, sz, 0);
  return (sz);
}

/*
 * close a socket
 */
#ifndef SD_SEND
#define SD_SEND 1
#endif
NETCON *net_close (NETCON *conn)
{
  if (conn->sock == INVALID_SOCKET)
    return (NULL);
  if (conn->ssl != NULL)
  {
    debug ("shutdown SSL\n");
    if (SSL_shutdown (conn->ssl) == 0)
      SSL_shutdown (conn->ssl);
    SSL_free (conn->ssl);
  }
  if (shutdown (conn->sock, SD_SEND) == 0)
  {
    char c;
    while (recv (conn->sock, &c, 1, 0) == 1);
  }
  closesocket (conn->sock);
  free (conn);
  return (NULL);
}

#ifdef UNITTEST

int main (int argc, char **argv)
{
  info ("%s has no current unittests\n", argv[0]);
  exit (0);
}

#endif
