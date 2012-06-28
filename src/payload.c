/*
 * payload.c
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

#ifdef UNITTEST
#undef UNITTEST
#include "unittest.h"
#include "util.c"
#include "dbuf.c"
#include "log.c"
#include "xml.c"
#include "mime.c"
#include "b64.c"
#include "crypt.c"
#include "xcrypt.c"
#define debug _DEBUG_
#define UNITTEST
#else
#endif

#include <stdio.h>
#include "util.h"
#include "log.h"
#include "b64.h"
#include "xml.h"
#include "crypt.h"
#include "xcrypt.h"
#include "payload.h"

/*
 * Process a payload envelope
 *
 * part has our MIME envelope
 * unc and pw used for decryption
 * dn gets DN found if empty, otherwise must match
 * data gets allocated payload or the error message if fails
 * filename get copy of payload name 
 * return len or 0 for failure
 */
int payload_process (MIME *part, unsigned char **data, char *filename,
    char *unc, char *dn, char *pw)
{
  char *ch;
  int len;

  /*
   * initialize all values
   */
  *filename = 0;
  *data = NULL;

  if (part == NULL)
    return (0);

  /*
   * first get the file name from the disposition...
   */
  if ((ch = mime_getHeader (part, MIME_DISPOSITION)) != NULL)
    ch = strchr (ch, '"');
  if (ch == NULL)
  {
    *data ="Missing Payload DISPOSITION";
    error ("%s\n", *data);
    return (0);
  }
  strcpy (filename, ch + 1);
  *strchr (filename, '"') = 0;
  debug ("filename=%s\n", filename);
  /*
   * next decrypt the data... assume it is not
   */
  if ((ch = mime_getHeader (part, MIME_CONTENT)) == NULL)
  {
    /*
     * use payload as is (assume text)
     */
    *data = (unsigned char *) malloc (len = mime_getLength (part));
    strcpy (*data, mime_getBody (part));
    return (len);
  }
  if (strstr (ch, MIME_XML) != NULL)
  {
    XML *payload;
    /*
     * encryption envelope attached
     */
    if ((payload = xml_parse (mime_getBody (part))) == NULL)
    {
      *data = "Malformed Payload"; 
      error ("%s for %s\n", *data, filename);
      return (0);
    }
    /*
     * decode payload
     */
    len = xcrypt_decrypt (payload, data, unc, dn, pw);
    if (len > 0)
    {
      info ("payload decryption for %s successful\n", filename);
    }
    else	/* PHINMS simply stores the XML envelope	*/
    {
      warn ("failed to decrypt payload for %s\n", filename);
      *data = xml_format (payload);
      len = strlen (*data);
    }
    xml_free (payload);
    return (len);
  }
  if (strstr (ch, MIME_OCTET) != NULL)
  {
    if (((ch = mime_getHeader (part, MIME_ENCODING)) != NULL)
      && (strstarts (ch, "base64\r\n")))
    {
      /*
       * base64 decode payload
       */
      *data = (char *) malloc (mime_getLength (part));
      return (b64_decode (*data, mime_getBody (part)));
    }
    *data = "Unknown payload encoding";
    error ("%s '%s' for %s\n", *data, ch, filename);
    return (0);
  }
  *data = "Unsupported payload Content-Type";
  error ("%s: %s for %s\n", *data, ch, filename);
  return (0);
}

/*
 * Create a payload envelope
 *
 * data for the payload
 * len of payload 
 * fname and org for the organization for MIME headers
 * unc and pw for encryption
 * dn gets DN of certificate used and inserted into envelope
 * return the MIME envelope or NULL if fails
 */
MIME *payload_create (unsigned char *data, int len, 
    char *fname, char *org, char *unc, char *dn, char *pw)
{
  MIME *msg;
  char *ch;
  char buf[MAX_PATH];

  if ((data == NULL) || (len < 1) || (fname == NULL) || (org == NULL))
    return (NULL);
  debug ("getpayload container...\n");
  msg = mime_alloc ();
  sprintf (buf, "<%s@%s>", basename (fname), org);
  debug ("content ID: %s\n", buf);
  mime_setHeader (msg, MIME_CONTENTID, buf, 0);
  if ((unc == NULL) || (*unc == 0))	/* not encrypted		*/
  {
    debug ("no encryption... plain payload\n");
    mime_setHeader (msg, MIME_CONTENT, MIME_OCTET, 99);
    mime_setHeader (msg, MIME_ENCODING, MIME_BASE64, 99);
    /*
     * base64 encode the payload
     */
    ch = (char *) malloc ((int) (1.4 * len));
    len = b64_encode (ch, data, len ,76);
  }
  else					/* encrypted			*/
  {
    XML *xml;

    mime_setHeader (msg, MIME_CONTENT, MIME_XML, 99);
    debug ("creating payload encryption xml using %s\n", unc);
    if ((xml = xcrypt_encrypt (data, len, unc, dn, pw, TRIPLEDES)) == NULL)
    {
      mime_free (msg);
      return (NULL);
    }
#ifdef UNITTEST
    xml_beautify (xml, 2);
#endif
    ch = xml_format (xml);
    len = strlen (ch);
    xml_free (xml);
  }
  sprintf (buf, "attachment; name=\"%s\"", basename (fname));
  mime_setHeader (msg, MIME_DISPOSITION, buf, 99);
  mime_setBody (msg, ch, len);
  free (ch);
  return (msg);
}

#ifdef UNITTEST

int main (int argc, char **argv)
{
  MIME *env;
  char *msg = "The quick brown fox jumped over the lazy dogs!\n";
  unsigned char *data;
  char *unc = "../security/phineas.pfx";
  char dn[MAX_PATH];
  char fname[MAX_PATH];
  char *pw = "changeit";
  int len;

  debug ("initializing...\n");
  SSL_load_error_strings();
  SSL_library_init();
  OpenSSL_add_all_ciphers();
  *dn = 0;

  debug ("creating a mime envelope\n");
  strcpy (fname, "foobar");
  env = payload_create (msg, strlen (msg) + 1, fname, "some org",
    unc, dn, pw);
  if (env == NULL)
    fatal ("failed to create envelope!\n");
  data = mime_format (env);
  debug ("MIME: %s\n", data);
  free (data);
  len = payload_process (env, &data, fname, unc, dn, pw); 
  if (len <= 0)
    fatal ("failed to decode envelope\n");
  if (len != strlen (msg) + 1)
    fatal ("wrong len: %d vs %d\n", len, strlen (msg));
  if (strcmp (data, msg))
    fatal ("message decrypted wrong:%.*s\n", len, data);
  free (data);
  mime_free (env);
  info ("succeeded dn=%s filename=%s\n", dn, fname);
  exit (0);
}

#endif
