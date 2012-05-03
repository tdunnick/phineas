/*
 * xcrypt.c
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
#define CMDLINE
#define __TEST__
#endif

#ifdef CMDLINE
#include "applink.c"
#include "util.c"
#include "dbuf.c"
#include "log.c"
#include "b64.c"
#include "xml.c"
#include "crypt.c"
#else
#include <stdio.h>
#include "util.h"
#include "dbuf.h"
#include "log.h"
#include "b64.h"
#include "xml.h"
#include "crypt.h"
#endif

#ifdef __TEST__
#undef debug
#define debug(fmt...) \
  log(LOGFILE, LOG_DEBUG, __FILE__, __LINE__, fmt)
#endif

/*
 * Common encryption suffixes
 */
char
  *payload_dn = "EncryptedData.KeyInfo.EncryptedKey.KeyInfo.KeyName",
  *payload_key = 
    "EncryptedData.KeyInfo.EncryptedKey.CipherData.CipherValue",
  *payload_data = "EncryptedData.CipherData.CipherValue";

/*
 * Payload template
 */
char *template = 
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
"<EncryptedData Id=\"ed1\" Type=\"http://www.w3.org/2001/04/xmlenc#Element\" xmlns=\"http://www.w3.org/2001/04/xmlenc#\">"
"  <EncryptionMethod Algorithm=\"http://www.w3.org/2001/04/xmlenc#tripledes-cbc\"/>"
"  <KeyInfo xmlns=\"http://www.w3.org/2000/09/xmldsig#\">"
"    <EncryptedKey xmlns=\"http://www.w3.org/2001/04/xmlenc#\">"
"      <EncryptionMethod Algorithm=\"http://www.w3.org/2001/04/xmlenc#rsa-1_5\"/>"
"      <KeyInfo xmlns=\"http://www.w3.org/2000/09/xmldsig#\">"
"        <KeyName>key</KeyName>"
"      </KeyInfo>"
"      <CipherData>"
"        <CipherValue/>"
"      </CipherData>"
"    </EncryptedKey>"
"  </KeyInfo>"
"  <CipherData>"
"    <CipherValue/>"
"  </CipherData>"
"</EncryptedData>";

/*
 * Fill in an ebxml encryption envelope.
 *
 * data, len - data of len to encrypt
 * unc, passwd - identify certificate to use
 * dn gets DN of certificate and should be DNSZ
 *
 * This uses triple DES encryption for the data, and the certificates
 * asymetric public key to encrypt the DES key.
 *
 * Return envelope or NULL if fails
 */
XML *xcrypt_encrypt (unsigned char *data, int len,
  char *unc, char *dn, char *passwd)
{
  XML *xml;
  DESKEY key;
  char *enc,
    ekey[512], 			/* big enough for a 4096 bit RSA key	*/
    bkey[512+256],		/* +50% for b64 encoding		*/
    dnbuf[DNSZ],		/* subject in the cert			*/
    path[MAX_PATH];


  if ((unc == NULL) || (data == NULL) || (len < 1))
    return (NULL);
 
  /*
   * first triple DES encrypt the payload and convert it to base64
   */
  debug ("encrypting %d bytes data...\n", len);
  enc = (unsigned char *) malloc (len + sizeof (key));
  if ((len = crypt_des3_encrypt (enc, key, data, len)) < 1)
  {
    error ("DES3 encoding failed\n");
    free (enc);
    return (NULL);
  }
  debug ("unc=%s\n", unc);
  data = (unsigned char *) malloc ((int) (1.40 * len));
  len = b64_encode (data, enc, len, 76);
  xml = xml_parse (template);
  xml_set_text (xml, payload_data, data);
  free (data);
  /*
   * now use the certificate to encrypt the triple DES key
   */
  if (dn == NULL)
    dn = dnbuf;
  pathf (path, unc);
  debug ("encrypting key for %s\n", path);
  if ((len = crypt_pk_encrypt (path, passwd, dn, ekey, key, DESKEYSZ)) < 1)
  {
    error ("Public key encoding failed\n");
    free (enc);
    return (NULL);
  }
  len = b64_encode (bkey, ekey, len, 76);
  xml_set_text (xml, payload_key, bkey);
  xml_set_text (xml, payload_dn, dn);
  /*
   * clean up
   */
  free (enc);
  debug ("encryption completed\n");
  return (xml);
}

/*
 * decrypt the payload and save it to data, returning it's len
 * payload has the XML payload envelope
 * data gets allocated the resulting decrypted payload
 * unc and passwd used to get the decryption key
 * dn checked against payload if given, otherwise gets filled in with 
 * payload's DN or ignored if NULL.
 * returns data length or 0 if fails
 */

int xcrypt_decrypt (XML *payload, unsigned char **data,
    char *unc, char *dn, char *passwd)
{
  int i, len;
  unsigned char *ch, *enc, *keyfile;
  unsigned char rsakey[512];
  DESKEY deskey;
  FILE *pfp;
  char path[MAX_PATH];

  if ((unc == NULL) || (payload == NULL))
    return (0);
  debug ("beginning decryption...\n");
  *data = NULL;
  pathf (path, unc);
  /*
   * check DN against payload to insure match, or fill in if
   * not provided
   */
  if (dn != NULL)
  {
    ch = xml_get_text (payload, payload_dn);
    if (*dn)
    {
      if (strcmp (dn, ch))
      {
        error ("DN %s does not match payload\n", dn);
        return (0);
      }
    }
    else
      strcpy (dn, ch);
  }
  /*
   * get and decrypt the DES key
   */
  debug ("getting des key\n");
  if ((ch = xml_get_text (payload,
    "EncryptedData.KeyInfo.EncryptedKey.CipherData.CipherValue")) == NULL)
  {
    error ("Couldn't get cypher key\n");
    return (0);
  }
  len = b64_decode (rsakey, ch);
  debug ("attempting RSA decryption %d bytes using %s with %s\n",
    len, unc, passwd);
  if ((len = crypt_pk_decrypt (path, passwd, deskey, rsakey)) < 1)
  {
    error ("Couldn't decrypt DES key\n");
    return (0);
  }
  /*
   * get and decrypt the payload
   */
  debug ("decrypting payload\n");
  if ((ch = xml_get_text (payload,
    "EncryptedData.CipherData.CipherValue")) == NULL)
  {
    error ("Couldn't get cypher payload\n");
    return (0);
  }
  len = strlen (ch);
  enc = (char *) malloc (len);
  len = b64_decode (enc, ch);
  *data = (char *) malloc (len);
  if ((len = crypt_des3_decrypt (*data, deskey, enc, len)) < 1)
    error ("Couldn't decrypt payload\n");
  debug ("final decoding to %d bytes\n", len);
  free (enc);
  return (len);
}

#ifdef CMDLINE

usagerr (char *msg)
{
  fprintf (stderr, "ERROR: %s\nusage: xcrypt options\n"
    "where options are...\n"
    "\t-e          encrypt (decryption default)\n"
    "\t-l log      log file (stdout)\n"
    "\t-L level    log level (INFO)\n"
    "\t-p passwd   keyfile is protected by passwd\n"
    "\t-o outfile  write output to outfile (stdout)\n"
    "\t-d dn       match or insert distinguished name dn\n"
    "\t-i infile   input is read from infile (stdin)\n"
    "\tkeyfile     RSA key or certificate\n",
    msg);
  exit (1);
}

#define NEEDARG(a) if (*++ch) a = ch; \
  else if (++i < argc) a=argv[i]; \
  else usagerr ("argument expected")

/*
 *  xcrypt -e -p password -d distinguished_name -o outfile -i infile keyfile
 */

int main (int argc, char **argv)
{
  XML *xml;			// xml payload
  unsigned char *payload;	// binary payload
  int encrypt = 0;		// default is to decrypt
  char *keyfile = NULL;		// RSA key file or certificate
  char *password = NULL;	// password to key file
  char dn[DNSZ] = "";		// DN to match
  FILE *ifp = stdin;		// input file
  FILE *ofp = stdout;		// output file
  int len;			// payload length
  int i;
  char *ch, *p;

  LOGFILE = log_open (NULL);

#ifdef __TEST__
  keyfile = "../security/phineas.pfx";
  password = "changeit";
#endif

  for (i = 1; i < argc; i++)
  {
    ch = argv[i];
    if (*ch == '-')
    {
nextarg:
      switch (*++ch)
      {
	case 'e' : 
	  encrypt = 1; 
	  if (ch[1]) goto nextarg;
	  break;
	case 'l' : 
	  NEEDARG (p);
	  LOGFILE = log_open (p);
	  break;
	case 'L' :
	  NEEDARG (p);
	  log_level (LOGFILE, p);
	  break;
	case 'd' : 
	  NEEDARG (p); 
	  strcpy (dn, p);
	  break;
	case 'p' : NEEDARG (password); break;
	case 'o' : 
	  NEEDARG (p); 
	  if ((ofp = fopen (p, "wb")) == NULL)
	  {
	    perror ("Can't open output file");
	    usagerr (p);
	  }
	  break;
	case 'i' : 
	  NEEDARG (p); 
	  if ((ifp = fopen (p, "rb")) == NULL)
	  {
	    perror ("Can't open input file");
	    usagerr (p);
	  }
	  break;
	default : usagerr (argv[i]); break;
      }
    }
    else
    {
      keyfile = argv[i];
    }
  }

  if (keyfile == NULL)
    usagerr ("missing keyfile");

  debug ("initializing...\n");
  SSL_load_error_strings();
  SSL_library_init();
  OpenSSL_add_all_ciphers();

#ifdef __TEST__

  log_setlevel (LOGFILE, LOG_DEBUG);
  debug ("setting up test environment\n");
  p = "The quick brown fox jumped over the lazy dogs!\n";
  len = strlen (p) + 1;
  xml = xcrypt_encrypt (p, len, keyfile, dn, password);
  debug ("DN=%s\n", dn);
  if (xml == NULL)
    fatal ("failed encryption\n");
  if ((len = xcrypt_decrypt (xml, &payload, keyfile, dn, password)) < 1)
    fatal ("failed decryption\n");
  if (len != strlen (p) + 1)
    fatal ("length %d doesn't match expected %d\n", len, strlen (p) + 1);
  if (strcmp (payload, p))
    fatal ("payload differs: %s", payload);
  info ("%s passed testing\n", argv[0]);

#else

  if (encrypt)
  {
    debug ("encrypting...\n");
    if ((payload = readstream (ifp, &len)) == NULL)
      fatal ("failed reading encryption payload\n");
    xml = xcrypt_encrypt (payload, len, keyfile, dn, password);
    if (xml == NULL)
      fatal ("failed encryption\n");
    xml_write (xml, ofp);
  }
  else
  {
    debug ("decrypting...\n");
    if ((xml = xml_read (ifp)) == NULL)
      fatal ("failed reading decryption payload\n");
    if ((len =xcrypt_decrypt (xml, &payload, keyfile, dn, password)) < 1)
      fatal ("failed decryption\n");
    len = fwrite (payload, sizeof (unsigned char), len, ofp);
    debug ("wrote %d bytes\n", len);
  }

#endif

  if (payload != NULL)
    free (payload);
  if (xml != NULL)
    xml_free (xml);
  if (ofp != stdout)
    fclose (ofp);
  if (ifp != stdin)
    fclose (ifp);
  return (0);
}

#endif
