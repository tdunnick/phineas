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

/*
 * XML encryption - see http://www.w3.org/TR/xmlenc-core/
 */

#ifdef UNITTEST
#include "unittest.h"
#undef UNITTEST
#define CMDLINE
#define __TEST__
#endif

#include <stdio.h>
#include "util.h"
#include "dbuf.h"
#include "log.h"
#include "b64.h"
#include "xml.h"
#include "crypt.h"

#ifndef debug
#define debug(fmt...)
#endif

/*
 * Common encryption tags
 */
char
  *KeyInfoName = "EncryptedData.KeyInfo.KeyName",
  *EncryptedKey = "EncryptedData.KeyInfo.EncryptedKey",
  *KeyName = "EncryptedData.KeyInfo.EncryptedKey.KeyInfo.KeyName",
  *KeyValue = 
    "EncryptedData.KeyInfo.EncryptedKey.CipherData.CipherValue",
  *DataValue = "EncryptedData.CipherData.CipherValue",
  *Method = "EncryptedData.EncryptionMethod",
  *KeyMethod = "EncryptedData.KeyInfo.EncryptedKey.EncryptionMethod";

/*
 * symetric Method Algorithm attribute
 * note these are ordered to match up with the algorithms in crypt.h
 */
char *xcrypt_Algorithm[] =
{
  NULL,
 "http://www.w3.org/2001/04/xmlenc#tripledes-cbc",
 "http://www.w3.org/2001/04/xmlenc#aes128-cbc",
 "http://www.w3.org/2001/04/xmlenc#aes192-cbc", /* optional */
 "http://www.w3.org/2001/04/xmlenc#aes256-cbc"
};

/*
 * asymetric KeyMethod Algorithm attribute
 */
char *Rsa15 = "http://www.w3.org/2001/04/xmlenc#rsa-1_5";
char *RsaOaep = "http://www.w3.org/2001/04/xmlenc#rsa-oaep-mgf1p";

/*
 * XML Payload template
 */
char *xcrypt_Template = 
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
 * unc, passwd - identify certificate and/or password to use
 * dn gets DN of certificate and should be DNSZ or may be NULL
 * how - symetric encryption method
 *
 * This uses triple DES encryption for the data, and the certificates
 * asymetric public key to encrypt the DES key.
 *
 * Return envelope or NULL if fails
 */
XML *xcrypt_encrypt (unsigned char *data, int len,
  char *unc, char *dn, char *passwd, int how)
{
  XML *xml;
  unsigned char *enc,
    key[SKEYSZ],
    ekey[PKEYSZ],		/* big enough for a 4096 bit RSA key	*/
    bkey[PKEYSZ+PKEYSZ/2];	/* +50% for b64 encoding		*/
  char dnbuf[DNSZ],		/* subject in the cert			*/
    path[MAX_PATH];


  if (((unc == NULL) && (passwd == NULL)) || (data == NULL) || (len < 1))
  {
    debug ("missing unc/passwd or data for len=%d\n", len);
    return (NULL);
  }
 
  if ((how < FIRSTCIPHER) || (how > LASTCIPHER))
    how = TRIPLEDES;
  /*
   * first symetric encrypt the payload and convert it to base64
   */
  debug ("encrypting %d bytes data...\n", len);
  enc = (unsigned char *) malloc (len + crypt_blocksz (how) * 2);
  if (unc == NULL)
  {
    debug ("password based encryption\n");
    crypt_pbkey (key, passwd, NULL, how);
    len = crypt_copy (enc, data, key, len, how, 1);
    strcpy (path, "Password Based");
  }
  else if (crypt_fkey (key, pathf (path, unc)) == crypt_keylen (how))
  {
    debug ("encrypting using file key %s\n", path);
    len = crypt_copy (enc, data, key, len, how, 1);
    unc = NULL;
  }
  else
  {
    debug ("certificate based encryption\n");
    len = crypt_encrypt (enc, data, key, len, how);
  }
  if (len < 1)
  {
    error ("Encryption failed\n");
    free (enc);
    return (NULL);
  }
  data = (unsigned char *) malloc ((int) (1.40 * len));
  len = b64_encode (data, enc, len, 76);
  xml = xml_parse (xcrypt_Template);
  xml_set_text (xml, DataValue, data);
  free (data);
  /*
   * now use the certificate to encrypt the symetric key or
   * simply skip keyinfo if only password is supplied
   */
  if (unc == NULL)
  {
    debug ("no certificate, setting raw key name\n");
    xml_delete (xml, EncryptedKey);
    xml_set_text (xml, KeyInfoName, path);
  }
  else
  {
    debug ("unc=%s\n", unc);
    if (dn == NULL)
      dn = dnbuf;
    debug ("encrypting key for %s\n", path);
    if ((len = crypt_pk_encrypt (path, passwd, dn, ekey, key, 
        crypt_keylen (how))) < 1)
    {
      error ("Public key encoding failed\n");
      free (enc);
      return (NULL);
    }
    len = b64_encode (bkey, ekey, len, 76);
    xml_set_text (xml, KeyValue, bkey);
    xml_set_text (xml, KeyName, dn);
  }
  xml_set_attribute (xml, Method, "Algorithm", xcrypt_Algorithm[how]);
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
  int how, len;
  unsigned char *ch, 
    *enc, 
    *keyfile,
    key[PKEYSZ],
    symkey[SKEYSZ];
  FILE *pfp;
  char path[MAX_PATH];

  if (((unc == NULL) && (passwd == NULL)) || (payload == NULL))
    return (0);
  debug ("beginning decryption...\n");
  *data = NULL;
  /*
   * determine how this got encrypted
   */
  ch = xml_get_attribute (payload, Method, "Algorithm");
  if (*ch) 
  {
    for (how = FIRSTCIPHER; strcmp (ch, xcrypt_Algorithm[how]); how++)
    {
      if (how == LASTCIPHER)
      {
        error ("No matching cipher %s\n", ch);
        return (0);
      }
    }
  }
  else
  {
    warn ("No cipher given - assuming triple des\n");
    how = TRIPLEDES;
  }
  /*
   * generate password based key, or use keyfile (certificate)
   */
  if (unc == NULL)
  {
    crypt_pbkey (symkey, passwd, NULL, how);
  }
  else
  {
    pathf (path, unc);
    /*
     * check DN against payload to insure match, or fill in if
     * not provided
     */
    if (dn != NULL)
    {
      ch = xml_get_text (payload, KeyName);
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
     * get and decrypt the symetric key
     */
    debug ("getting symetric key\n");
    ch = xml_get_text (payload, KeyValue);
    if (*ch == 0)  		/* assume keyfile IS the key	*/
    {
      debug ("no KeyValue, assuming %s is key\n", unc);
      len = crypt_fkey (symkey, path);
      if (len != crypt_keylen (how))
      {
	error ("incorrect key length for %s\n", unc);
        return (0);
      }
    }
    else
    {
      len = b64_decode (key, ch);
      debug ("attempting RSA decryption %d bytes using %s with %s\n",
        len, unc, passwd);
      if ((len = crypt_pk_decrypt (path, passwd, symkey, key)) < 1)
      {
        error ("Couldn't decrypt symetric key\n");
        return (0);
      }
    }
  }
  /*
   * get and decrypt the payload
   */
  debug ("decrypting payload\n");
  if ((ch = xml_get_text (payload, DataValue)) == NULL)
  {
    error ("Couldn't get cypher payload\n");
    return (0);
  }
  len = strlen (ch);
  enc = (unsigned char *) malloc (len);
  len = b64_decode (enc, ch);
  *data = (unsigned char *) malloc (len);
  if ((len = crypt_decrypt (*data, enc, symkey, len, how)) < 1)
    error ("Couldn't decrypt payload\n");
  debug ("final decoding to %d bytes\n", len);
  free (enc);
  return (len);
}

#ifdef CMDLINE
#undef debug
#include "applink.c"
#include "util.c"
#include "dbuf.c"
#include "log.c"
#include "b64.c"
#include "xmln.c"
#include "xml.c"
#include "crypt.c"

#ifdef __TEST__
int dbuftest ()
{
  int n;
  DBUF *b;

  b = dbuf_alloc ();
  if (dbuf_size (b) != 0)
    fatal ("initial DBUF size is wrong!\n");
  n = dbuf_printf (b, "The %s brown fox jumped over %d lazy dogs\n",
    "quick", 27);
  if (n != 45)
    fatal ("expected at least 48 chars but got %d\n", n);
  if (n != dbuf_size (b))
    fatal ("size=%d but got %d\n", dbuf_size (b), n);
  if (strcmp (dbuf_getbuf (b), 
      "The quick brown fox jumped over 27 lazy dogs\n"))
    fatal ("string mis-match: %s", dbuf_getbuf (b));
}

int runtest (char *keyfile, char *password, int how)
{
  XML *xml;
  int len;
  char 
    *method = "certificate",
    dn[DNSZ];
  unsigned char
    *payload,
    *p = "The quick brown fox jumped over the lazy dogs!\n";


  debug ("testing %s\n", xcrypt_Algorithm[how]);
  while (1)
  {
    len = strlen (p) + 1;
    *dn = 0;
    xml = xcrypt_encrypt (p, len, keyfile, dn, password, how);
    debug ("DN=%s\n", dn);
    if (xml == NULL)
      fatal ("failed encryption\n");
    if ((len = xcrypt_decrypt (xml, &payload, keyfile, dn, password)) < 1)
      fatal ("failed decryption\n");
    if (len != strlen (p) + 1)
      fatal ("length %d doesn't match expected %d\n", len, strlen (p) + 1);
    if (strcmp (payload, p))
      fatal ("payload differs: %s", payload);
    debug ("passed testing using %s\n", method);
    free (payload);
    xml = xml_free (xml);
    if (strcmp (method, "certificate") == 0)
    {
      method = "key file";
      keyfile = "test.key";
      if (how == TRIPLEDES)
      {
	strcpy (dn, "F00C5EC4727A66D98E86483FD99871A146DA6F714A07038A");
	len = strlen (dn);
      }
      else
      {
        len = crypt_key (dn, how);
      }
      writefile (keyfile, dn, len);
    }
    else if (strcmp (method, "key file") == 0)
    {
      method = "password";
      unlink ("test.key");
      keyfile = NULL;
    }
    else
      break;
  }
  return (0);
}

#endif /* __TEST__ */

usagerr (char *msg)
{
  fprintf (stderr, "ERROR at '%s'\nusage: xcrypt options\n"
    "where options are...\n"
    "\t-des3       triple DES encryption (default)\n"
    "\t-aes128     128 bit AES encryption\n"
    "\t-aes192     192 bit AES encryption\n"
    "\t-aes256     256 bit AES encryption\n"
    "\t-e          encrypt (decryption default)\n"
    "\t-l log      log file (stdout)\n"
    "\t-L level    log level (INFO)\n"
    "\t-p passwd   keyfile or encryption passwd\n"
    "\t-o outfile  write output to outfile (stdout)\n"
    "\t-d dn       match or insert distinguished name dn\n"
    "\t-i infile   input is read from infile (stdin)\n"
    "\tkeyfile     symetric key, RSA key, or certificate\n",
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
  XML *xml = NULL;		// xml and binary payload
  unsigned char *payload = NULL;
  int encrypt = 0;		// default is to decrypt
  char *keyfile = NULL;		// RSA key file or certificate
  char *password = NULL;	// password to key file
  char dn[DNSZ] = "";		// DN to match
  FILE *ifp = stdin;		// input file
  FILE *ofp = stdout;		// output file
  int len;			// payload length
  int i,
      how = TRIPLEDES;
  char *ch, *p;


#ifdef __TEST__
  debug ("setting up test environment\n");
  dbuftest ();
  keyfile = "../security/phineas.pfx";
  password = "changeit";
#else
  LOGFILE = log_open (NULL);
#endif


  for (i = 1; i < argc; i++)
  {
    ch = argv[i];
    if (*ch == '-')
    {
nextarg:
      switch (*++ch)
      {
	case 'a' :
	  if (!strcmp (ch, "aes128"))
	    how = AES128;
	  else if (!strcmp (ch, "aes192"))
	    how = AES192;
	  else if (!strcmp (ch, "aes256"))
	    how = AES256;
	  else
	    usagerr (ch);
	  break;
	case 'e' : 
	  encrypt = 1; 
	  if (ch[1]) goto nextarg;
	  break;
#ifndef __TEST__
	case 'l' : 
	  NEEDARG (p);
	  LOGFILE = log_open (p);
	  break;
	case 'L' :
	  NEEDARG (p);
	  log_level (LOGFILE, p);
	  break;
#endif
	case 'd' : 
	  if (!strcmp (ch, "des3"))
	  {
	    how = TRIPLEDES;
	    break;
	  }
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

  if ((keyfile == NULL) && (password == NULL))
    usagerr ("a keyfile and/or password must be specified");

  debug ("initializing...\n");
  SSL_load_error_strings();
  SSL_library_init();
  OpenSSL_add_all_ciphers();


#ifdef __TEST__

  for (how = FIRSTCIPHER; how <= LASTCIPHER; how++)
    runtest (keyfile, password, how);

#else

  if (encrypt)
  {
    debug ("encrypting...\n");
    if ((payload = readstream (ifp, &len)) == NULL)
      fatal ("failed reading encryption payload\n");
    xml = xcrypt_encrypt (payload, len, keyfile, dn, password, how);
    if (xml == NULL)
      fatal ("failed encryption\n");
    if ((len = xml_write (xml, ofp)) < 1)
      fatal ("failed writing output\n");
    debug ("wrote %d bytes\n", len);
  }
  else
  {
    debug ("decrypting...\n");
    if ((xml = xml_read (ifp)) == NULL)
      fatal ("failed reading decryption payload\n");
    /* force decryption method if none found in envelope	*/
    if (*xml_get_text (xml, Method) == 0)
      xml_set (xml, Method, xcrypt_Algorithm[how]);
    if ((len =xcrypt_decrypt (xml, &payload, keyfile, dn, password)) < 1)
      fatal ("failed decryption\n");
    len = fwrite (payload, sizeof (unsigned char), len, ofp);
    debug ("wrote %d bytes\n", len);
  }

#endif /* __TEST__ */

  if (payload != NULL)
    free (payload);
  if (xml != NULL)
    xml_free (xml);
  if (ofp != stdout)
    fclose (ofp);
  if (ifp != stdin)
    fclose (ifp);

#ifdef __TEST__
  info ("%s %s\n", argv[0], Errors ? "failed" : "passed");
  exit (Errors);
#else
  return (0);
#endif
}

#endif
