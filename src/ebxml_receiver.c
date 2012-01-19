/*
 * ebxml_receiver.c
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
#define __RECEIVER__
#endif

#ifdef __RECEIVER__

#ifdef UNITTEST
#undef UNITTEST
#include "unittest.h"
#include "util.c"
#include "dbuf.c"
#include "log.c"
#include "xml.c"
#include "mime.c"
#include "queue.c"
#include "task.c"
#include "fileq.c"
#include "b64.c"
#include "basicauth.c"
#include "crypt.c"
#include "find.c"
#include "fpoller.c"
#include "qpoller.c"
#include "net.c"
#include "ebxml.c"
#define UNITTEST
#define debug _DEBUG_
#else
#include "ebxml.h"
#endif

#include <stdio.h>
#include "util.h"
#include "log.h"
#include "mime.h"
#include "basicauth.h"
#include "crypt.h"
#include "ebxml.h"


#ifndef debug
#define debug(fmt...)
#endif

/***************************** receiver functions *******************/
/*
 * Some external function has opened a listen port and calls here
 * when a complete POST message has arrived.  This could be a stand-alone
 * process, or a host HTTP server.
 *
 * So in contrast to the sender, these functions only deal with buffers
 * and the network layer is found elsewhere.
 */

/*
 * An internal cache is used for duplicate detection.  At start-up we
 * might want to intialize this cache to the latest incoming.
 * The key is made up of the sender's party ID and the SOAP db Record ID.
 */
typedef struct ecache
{
  struct ebxmlcache *next;
  char *response;
  char key[1];
} ECACHE;


typedef struct
{
  MUTEX mutex;
  ECACHE *cache;
} EBXMLCACHE;

EBXMLCACHE *EbxmlCache = NULL;

/*
 * initialize
 */
void ebxml_receiver_init (XML *xml)
{
  if (EbxmlCache != NULL)
    return;
  EbxmlCache = (EBXMLCACHE *) malloc (sizeof (EBXMLCACHE));
  memset (EbxmlCache, 0, sizeof (EBXMLCACHE));
  init_mutex (EbxmlCache);
}

/*
 * return a response if found in the cache
 */
char *ebxml_duplicate (XML *soap)
{
  wait_mutex (EbxmlCache);
  end_mutex (EbxmlCache);
  return (NULL);
}


/*
 * return the map index for this service/action pair
 */
int ebxml_service_map (XML *xml, char *service, char *action, char *prefix)
{
  int i;

  debug ("getting service map for %s/%s\n", service, action);
  for (i = 0; i < xml_count (xml, "Phineas.Receiver.MapInfo.Map"); i++)
  {
    sprintf (prefix, "%s[%i].", "Phineas.Receiver.MapInfo.Map", i);
    if (strcmp (service, ebxml_get (xml, prefix, "Service")) ||
	strcmp (action, ebxml_get (xml, prefix, "Action")))
      continue;
    debug ("matched prefix %s\n", prefix);
    return (i);
  }
  *prefix = 0;
  return (-1);
}

/*
 * decrypt the payload and save it to data, returning it's len
 */

int ebxml_decrypt (XML *payload, char **data,
    char *unc, char *dn, char *passwd)
{
  int i, len;
  unsigned char *ch, *enc, *keyfile;
  unsigned char rsakey[512];
  DESKEY deskey;
  FILE *pfp;
  char path[MAX_PATH];

  *data = NULL;
  pathf (path, unc);
  /*
   * check DN against unc to insure match
   */
  if ((dn != NULL) && *dn)
  {
    // TODO
  }
  /*
   * get and decrypt the DES key
   */
  if ((ch = xml_get_text (payload,
    "EncryptedData.KeyInfo.EncryptedKey.CipherData.CipherValue")) == NULL)
  {
    error ("Couldn't get cypher key\n");
    return (-1);
  }
  len = b64_decode (rsakey, ch);
  debug ("attempting RSA decryption %d bytes using %s with %s\n",
    len, unc, passwd);
  if ((len = crypt_pk_decrypt (path, passwd, deskey, rsakey)) < 1)
  {
    error ("Couldn't decrypt DES key\n");
    return (-1);
  }
  debug ("keylen=%d\n", len);
  /*
   * get and decrypt the payload
   */
  if ((ch = xml_get_text (payload,
    "EncryptedData.CipherData.CipherValue")) == NULL)
  {
    error ("Couldn't get cypher payload\n");
    return (-1);
  }
  len = strlen (ch);
  debug ("base64 len=%d\n", len);
  enc = (char *) malloc (len);
  len = b64_decode (enc, ch);
  *data = (char *) malloc (len);
  debug ("base64 decoded len=%d\n", len);
  if ((len = crypt_des3_decrypt (*data, deskey, enc, len)) < 1)
    error ("Couldn't decrypt payload\n");
  debug ("final decoding to %d bytes\n", len);
  free (enc);
  return (len);
}

/*
 * construct a reply message
 */
char *ebxml_reply (XML *xml, XML *soap, QUEUEROW *r,
  char *status, char *error, char *appdata)
{
  XML *txml;
  MIME *rmsg, *smsg, *msg;
  DBUF *b;
  char *ch,
       *organization,
       today[30],
       pid[PTIMESZ],
       buf[DBUFSZ];
  time_t t;

  time (&t);
  strcpy (today, ctime (&t));
  today[24] = 0;


  organization = xml_get_text (xml, "Phineas.Organization");
  ppid (pid);
  debug ("pid=%s\n", pid);
  /*
   * build the soap container...
   */
  txml = ebxml_template (xml, "Phineas.AckTemplate");
  // txml = ebxml_template (xml, "Phineas.SoapTemplate");
  if (txml == NULL)
  {
    error ("can't get soap template\n");
    return (NULL);
  }
  ebxml_set (txml, soap_hdr, "eb:To.eb:PartyId",
    ebxml_get (soap, soap_hdr, "eb:From.eb:PartyId"));
  ebxml_set (txml, soap_hdr, "eb:From.eb:PartyId",
    ebxml_get (soap, soap_hdr, "eb:To.eb:PartyId"));
  ebxml_set (txml, soap_hdr, "eb:CPAId",
    ebxml_get (soap, soap_hdr, "eb:CPAId"));
  ebxml_set (txml, soap_hdr, "eb:ConversationId",
    ebxml_get (soap, soap_hdr, "eb:ConversationId"));
  if (strcmp (ebxml_get (soap, soap_hdr, "eb:Action"), "Ping") == 0)
    ch = "Pong";
  else if (strcmp (error, "none"))
    ch = "MessageError";
  else
    ch = "Acknowledgment";
  ebxml_set (txml, soap_hdr, "eb:Action", ch);
  sprintf (buf, "%s@%s", pid, organization);
  ebxml_set (txml, soap_hdr, "eb:MessageData.eb:MessageId", buf);
  ebxml_set (txml, soap_hdr, "eb:MessageData.eb:Timestamp",
    ptime (NULL, buf));
  ebxml_set (txml, soap_ack, "eb:Timestamp", ptime (NULL, buf));
  queue_field_set (r, "RECEIVEDTIME", ptime (NULL, buf));
  queue_field_set (r, "LASTUPDATETIME", buf);
  ebxml_set (txml, soap_hdr, "eb:MessageData.eb:RefToMessageId",
      "statusResponse@cdc.gov");
  ebxml_set (txml, soap_hdr, "eb:MessageData.eb:RefToMessageId[1]",
    ebxml_get (soap, soap_hdr, "eb:MessageData.eb:MessageId"));
  ebxml_set (txml, soap_ack, "eb:RefToMessageId",
    ebxml_get (soap, soap_hdr, "eb:MessageData.eb:MessageId"));

  smsg = mime_alloc ();
  mime_setHeader (smsg, MIME_CONTENTID, "<ebxml-envelope@cdc.gov>", 0);
  mime_setHeader (smsg, MIME_CONTENT, MIME_XML, 1);
  mime_setBody (smsg, ch = xml_format (txml), 0);
  xml_free (txml);
  free (ch);

  /*
   * build the response container...
   * we could do this using a template, but it is trivial...
   */
  if (strcmp (ebxml_get (soap, soap_hdr, "eb:Action"), "Ping"))
  {
    b = dbuf_alloc ();
    dbuf_printf (b, "<response><msh_response><status>%s</status>"
      "<error>%s</error><appdata>%s</appdata></msh_response></response>",
      status, error, appdata);
    queue_field_set (r, "APPLICATIONSTATUS", status);
    queue_field_set (r, "ERRORCODE", error);
    queue_field_set (r, "ERRORMESSAGE", appdata);
    rmsg = mime_alloc ();
    mime_setHeader (rmsg, MIME_CONTENTID, "<statusResponse@cdc.gov>", 0);
    mime_setHeader (rmsg, MIME_CONTENT, MIME_XML, 1);
    mime_setBody (rmsg, dbuf_getbuf (b), 0);
    dbuf_free (b);
  }
  else
    rmsg = NULL;
  /*
   * package it up for transport
   */
  msg = mime_alloc ();
  mime_setBoundary (msg, "");
  mime_setHeader (msg, "SOAPAction", "\"ebXML\"", 99);
  mime_setHeader (msg, "Date", today, 99);
  mime_setHeader (msg, "Connection", "close", 99);
  mime_setHeader (msg, "Server", "PHINEAS 0.1", 99);
  mime_setMultiPart (msg, smsg);
  if (rmsg != NULL)
    mime_setMultiPart (msg, rmsg);
  ch = mime_format (msg);
  mime_free (msg);
  debug ("Reply:\n%s\n", ch);
  /*
  organization = ebxml_beautify (ch);
  debug ("Reply:\n%s\n", organization);
  free (organization);
  */
  return (ch);
}

/*
 * delete all the attributes from an XMLNODE subtree
 */
void ebxml_delete_attributes (XMLNODE *n)
{
  while (n != NULL)
  {
    xml_node_free (n->attributes);
    n->attributes = NULL;
    if (xml_is_parent (n))
      ebxml_delete_attributes (n->value);
    n = n->next;
  }
}

/*
 * format up the metadata from the soap body for entry into a queue
 */
char *ebxml_format_arguments (XML *soap)
{
  XML *metadata;
  char *ch;
  char path[MAX_PATH];

  debug ("Construction metadata for ack\n");
  metadata = xml_alloc ();
  strcpy (path, soap_manifest);
  strcat (path, "MetaData");
  ch = xml_get (soap, path);
  xml_set (metadata, "Manifest.MetaData", ch);
  free (ch);
  debug ("Removing attributes from metada\n");
  ebxml_delete_attributes (metadata->doc);
  xml_beautify (metadata, 0);
  ch = xml_format (metadata);
  xml_free (metadata);
  debug ("Returning metadata\n%s\n", ch);
  return (ch);
}

/*
 * start filling in a queue entry for an ebXML reception
 */
int ebxml_request_row (QUEUEROW *r, XML *soap)
{
  char *ch;

  queue_field_set (r, "MESSAGEID",
    ebxml_get (soap, soap_dbinf, "MessageId"));
  queue_field_set (r, "SERVICE",
    ebxml_get (soap, soap_hdr, "eb:Service"));
  queue_field_set (r, "ACTION",
    ebxml_get (soap, soap_hdr, "eb:Action"));
  queue_field_set (r, "ARGUMENTS", ch = ebxml_format_arguments (soap));
  free (ch);
  queue_field_set (r, "FROMPARTYID",
    ebxml_get (soap, soap_hdr, "eb:From.eb:PartyId"));
  queue_field_set (r, "MESSAGERECIPIENT",
    ebxml_get (soap, soap_dbinf, "MessageRecipient"));
  queue_field_set (r, "PROCESSINGSTATUS", "received");
  queue_field_set (r, "PROCESSID",
    ebxml_get (soap, soap_hdr, "eb:ConversationId"));
  return (0);
}

/*
 * Process an incoming request and return the response.  The caller
 * should free the response after sending.
 */
char *ebxml_process_req (XML *xml, char *buf)
{
  int len;
  MIME *msg, *part;
  XML *soap, *payload;
  QUEUEROW *r;
  FILE *fp;
  char *ch,
       *p,
       prefix[MAX_PATH],
       name[MAX_PATH];

  msg = NULL;
  soap = NULL;
  payload = NULL;
  r = NULL;

  info ("Begin processing ebXML request...\n");
  /*
   * Check Authorization if needed.  To keep it simple, we'll
   * use a single "realm" for all the service maps. This behavior 
   * can be modified to a per map basis, albeit with quite a bit
   * of extra work.
   */
  if (basicauth_check (xml, "Phineas.Receiver.BasicAuth", buf))
  {
    DBUF *b = basicauth_response ("Phineas Receiver");
    return (dbuf_extract (b));
  }
  debug ("request:%s\n", buf);
  if ((msg = mime_parse (buf)) == NULL)
  {
    error ("Failed to parse MIME payload\n");
    return (NULL);
  }
  if ((part = mime_getMultiPart (msg, 1)) == NULL)
  {
    error ("Failed to get SOAP envelope\n");
    mime_free (msg);
    return (NULL);
  }
  //debug ("Parsing soap part\n%s\n", mime_getBody (part));
  if ((soap = xml_parse (mime_getBody (part))) == NULL)
  {
    error ("Failed to parse SOAP xml\n");
    mime_free (msg);
    return (NULL);
  }
  /*
   * check for ping
   */
  ch = ebxml_get (soap, soap_hdr, "eb:Action");
  if (strcmp (ch, "Ping") == 0)
  {
    ch = ebxml_reply (xml, soap, NULL, "success", "none", "none");
    goto done;
  }
  /*
   * find the service map and initialize a queue entry
   */
  if (ebxml_service_map (xml, ebxml_get (soap, soap_hdr, "eb:Service"),
    ch, prefix) < 0)
  {
    error ("Unknown service/action %s/%s\n",
	ebxml_get (soap, soap_hdr, "eb:Service"), ch);
    ch = ebxml_reply (xml, soap, NULL, "InsertFailed",
      "Unknown Service/Action", "none");
    goto done;
  }
  r = queue_row_alloc (queue_find (ch = ebxml_get (xml, prefix, "Queue")));
  if (r == NULL)
  {
    error ("queue not found for %s\n", ch);
    return (NULL);
  }
  ebxml_request_row (r, soap);
  /*
   * get the payload
   */
  if ((part = mime_getMultiPart (msg, 2)) == NULL)
  {
    error ("Failed to get PAYLOAD envelope\n");
    ch = ebxml_reply (xml, soap, r, "InsertFailed",
      "Missing Payload Envelope", "none");
    goto done;
  }
  /*
   * encryption envelope
   * first get the file name from the disposition...
   */
  if ((ch = mime_getHeader (part, MIME_DISPOSITION)) != NULL)
    ch = strchr (ch, '"');
  if (ch != NULL)
  {
    p = ebxml_get (xml, prefix, "Directory");
    pathf (name, "%s%s", p, ch + 1);
    ch = name + strlen (p);
    *strchr (ch, '"') = 0;
    if (r != NULL)
    {
      queue_field_set (r, "PAYLOADNAME", ch);
      queue_field_set (r, "LOCALFILENAME", name);
    }
  }
  else
  {
    error ("No payload DISPOSITION\n");
    ch = ebxml_reply (xml, soap, r, "InsertFailed",
      "Missing Payload DISPOSITION", "none");
    goto done;
  }
  /*
   * next decrypt the data... assume it is not
   */
  queue_field_set (r, "ENCRYPTION", "no");
  if ((ch = mime_getHeader (part, MIME_CONTENT)) == NULL)
  {
    /*
     * use payload as is (assume text)
     */
    ch = (char *) malloc (len = mime_getLength (part));
    strcpy (ch, mime_getBody (part));
  }
  else if (strstr (ch, MIME_XML) != NULL)
  {
    /*
     * encryption envelope attached
     */
    if ((payload = xml_parse (mime_getBody (part))) == NULL)
    {
      error ("Failed to parse PAYLOAD envelope\n");
      ch = ebxml_reply (xml, soap, r, "InsertFailed",
        "Malformed Payload", "none");
      goto done;
    }
    /*
     * decode payload
     */
    len = ebxml_decrypt (payload, &ch,
      ebxml_get (xml, prefix, "Encryption.Unc"),
      ebxml_get (xml, prefix, "Encryption.Id"),
      ebxml_get (xml, prefix, "Encryption.Password"));
    queue_field_set (r, "ENCRYPTION", "yes");
    info ("ebXML payload decryption successful\n");
  }
  else if (strstr (ch, MIME_OCTET) != NULL)
  {
    if (((ch = mime_getHeader (part, MIME_ENCODING)) != NULL)
      && (strstarts (ch, "base64\r\n")))
    {
      /*
       * base64 decode payload
       */
      ch = (char *) malloc (mime_getLength (part));
      len = b64_decode (ch, mime_getBody (part));
    }
    else
    {
      error ("Unknown encoding '%s' for payload\n", ch);
      ch = ebxml_reply (xml, soap, r, "InsertFailed",
        "Unknown Payload Encoding", "none");
      goto done;
    }
  }
  else
  {
    error ("Unsupported payload Content-Type: %s\n", ch);
    ch = ebxml_reply (xml, soap, r, "InsertFailed",
      "Unsuppported Payload Content Type", "none");
    goto done;
  }
  if ((len < 1) || (ch == NULL))
  {
    ch = ebxml_reply (xml, soap, r, "InsertFailed",
      "Can't decrypt payload", "none");
    goto done;
  }
  /*
   * TODO save payload to disk using filter if given
   */
  info ("Writing ebXML payload to %s\n", name);
  if ((fp = fopen (name, "wb")) == NULL)
  {
    free (ch);
    error ("Can't open %s for write\n", name);
    ch = ebxml_reply (xml, soap, r, "InsertFailed",
      "Can not save file", "none");
    goto done;
  }
  fwrite (ch, 1, len, fp);
  fclose (fp);
  free (ch);
  /*
   * construct a reply and insert a queue entry
   */
  ch = ebxml_reply (xml, soap, r, "InsertSuceeded", "none", "none");

done:

  debug ("completing ebxml request processing\n");
  if (r != NULL)
  {
    queue_push (r);
    queue_row_free (r);
  }
  if (soap != NULL)
    xml_free (soap);
  if (payload != NULL)
    xml_free (payload);
  if (msg != NULL)
    mime_free (msg);
  debug ("ebXML reply: %s\n", ch);
  info ("ebXML request processing completed\n");
  return (ch);
}

#ifdef UNITTEST

int ran = 0;
int phineas_running  ()
{
  return (ran++ < 3);
}

int main (int argc, char **argv)
{
  XML *xml;
  int len;
  char *in, *out;

  debug ("initializing...\n");
  SSL_load_error_strings();
  SSL_library_init();
  OpenSSL_add_all_ciphers();
  xml = xml_parse (PhineasConfig);
  loadpath (xml_get_text (xml, "Phineas.InstallDirectory"));
  queue_init (xml);

  debug ("Reading test request\n");
  if ((in = readfile ("examples/request2.txt", &len)) != NULL)
  {
    out = ebxml_process_req (xml, in);
    free (in);
    free (out);
  }
  queue_shutdown ();
  xml_free (xml);
  info ("%s unit test completed\n", argv[0]);
  exit (0);
}

#endif /* UNITTEST */
#endif /* __RECEIVER__ */
