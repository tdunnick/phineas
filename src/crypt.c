/*
 * crypt.c
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
#include <stdio.h>
#include <string.h>

#ifdef UNITTEST
#undef UNITTEST
#include "unittest.h"
#include "util.c"
#define UNITTEST
#define debug _DEBUG_
#else
#include "log.h"
#endif
#include "crypt.h"

#ifndef debug
#define debug(fmt...)
#endif

/*
 * Get a distinguished name from an X509 subject.
 *
 * We could do this explicitly...
 *  char *sn_list[] =
 *  {
 *    SN_commonName,
 *    SN_countryName,
 *    SN_localityName,
 *    SN_stateOrProvinceName,
 *    SN_organizationName,
 *    SN_organizationalUnitName,
 *    SN_pkcs9_emailAddress
 *  };
 *  X509_NAME_get_text_by_NID(X509_NAME *name, int nid, char *buf,int len);
 *
 * Instead we'll just reverse the one line, changing the slashes to commas.
 * PHINMS (Java?) also folds everything to uppercase on the left side
 * of the '=', in particular the emailAddress.
 */
char *crypt_X509_dn (X509 *cert, char *dn, int len)
{
  int i;
  char buf[DNSZ], *list[20], *ch, *p;
  X509_NAME *subject;

  if ((cert == NULL) || (dn == NULL))
  {
    debug ("cert or dn is NULL\n");
    return (NULL);
  }
  if ((subject = X509_get_subject_name(cert)) == NULL)
  {
    error ("Can't get subject\n");
    return (NULL);
  }
  /* get the DN from the subject */
  X509_NAME_oneline (subject, buf, DNSZ);
  i = 0;
  for (ch = buf; ch != NULL; ch = strchr (ch, '/'))
  {
    *ch++ = 0;
    for (p = ch; *p && (*p != '='); p++)
      *p = toupper (*p);
    list[i++] = ch;
    if (i == 20)
    {
      error ("More than 20 items in certificate subject\n");
      break;
    }
  }
  debug ("found %d subject items\n", i);
  strcpy (ch = dn, list[--i]);
  while (i--)
  {
    ch += strlen (ch);
    if ((ch - dn + strlen (list[i]) + 2) >= len)
      break;
    strcpy (ch, ", ");
    strcpy (ch + 2, list[i]);
  }
  debug ("DN=%s\n", dn);
  return (dn);
}

/*
 * Get a certificate at location unc.  
 *
 * Trys PEM, DER, and finally PKCS12 formats coded certificates.
 *
 * PEM_read_X509 (fp, x, cb, u)
 * fp = FILE *
 * x = X509 ** - same as returned 
 * cb = call back for password
 * u = void * - the password or phrase
 */

X509 *crypt_get_X509 (char *unc, char *passwd)
{
  PKCS12 *p12;
  X509 *cert;
  FILE *fp;

  if (unc == NULL)
    return (NULL);
  if ((fp = fopen (unc, "r")) == NULL)
  {
    error ("Can't open certificate %s - %s\n", unc, strerror (errno));
    return (NULL);
  }
  debug ("trying PEM certificate\n");
  /* try reading as a PEM encoded certificate */
  if ((cert = PEM_read_X509 (fp, NULL, NULL, passwd)) == NULL)
  {
    /* if that failed try reading as a DER encoded certificate */
    rewind (fp);
    debug ("trying DER certificate\n");
    cert = d2i_X509_fp (fp, NULL);
    /* if that failed try reading as a PKCS12 store */
    if (cert == NULL)
    {
      rewind (fp);
      debug ("trying PKCS12 certificate\n");
      if ((p12 = d2i_PKCS12_fp (fp, NULL)) != NULL)
      {
        PKCS12_parse (p12, passwd, NULL, &cert, NULL);
        PKCS12_free (p12);
      }
    }
  }
  if (cert == NULL)
  {
    error ("Can't read certificate from %s\n", unc);
    return (NULL);
  }
  fclose (fp);
  return (cert);
}

/*
 * Get the private at the location specified.
 * Trys PEM, DER, and finally PKCS12 formats coded certificates.
 */
EVP_PKEY *crypt_get_pkey (char *name, char *passwd)
{
  EVP_PKEY *key;
  PKCS12 *p12;
  FILE *fp;

  if (name == NULL)
    return (NULL);
  if ((fp = fopen (name, "r")) == NULL)
  {
    error ("can't open key file %s\n", name);
    return (NULL);
  }
  debug ("trying PEM key\n");
  if ((key = PEM_read_PrivateKey (fp, NULL, NULL, passwd)) == NULL)
  {
    /* if that failed try reading as a DER encoded key */
    rewind (fp);
    debug ("trying DER key\n");
    key = d2i_PrivateKey_fp (fp, NULL);
    /* if that failed try reading as a PKCS12 store */
    if (key == NULL)
    {
      rewind (fp);
      debug ("trying PKCS12 key\n");
      if ((p12 = d2i_PKCS12_fp (fp, NULL)) != NULL)
      {
        if (!PKCS12_parse (p12, passwd, &key, NULL, NULL))
	  key = NULL;
        PKCS12_free (p12);
      }
    }
  }
  fclose (fp);
  if (key == NULL)
  {
    error ("can't read key from %s\n", name);
    return (NULL);
  }
  return (key);
}

/*
 * Asymetric encryption using PEM X509 certificate public key.
 * Return the encrypted length.
 */
int crypt_X509_encrypt (X509 *cert, char *enc, char *plain, int len)
{
  EVP_PKEY *key;
  FILE *fp;

  if (cert == NULL)
    return (-1);
  key = X509_get_pubkey (cert);
  if (key == NULL)
  {
    error ("Can't get public key from certificate\n");
    return (-1);
  }
  len = EVP_PKEY_encrypt (enc, plain, len, key);
  EVP_PKEY_free (key);
  return (len);
}

/*
 * Public key encryption from a certificate location.  The certificate
 * is located at unc. If dn is given, use it to find the cert.  Otherwise
 * fill it in with the subject (DN) of the certificate (it should have 
 * DNSZ len).  Return the encrypted length.
 */
int crypt_pk_encrypt (char *unc, char *passwd, char *dn,
  char *enc, char *plain, int len)
{
  X509 *cert;

  // TODO lookup certificate by DN... LDAP
  if ((cert = crypt_get_X509 (unc, passwd)) == NULL)
    return (0);
  crypt_X509_dn (cert, dn, DNSZ);
  len = crypt_X509_encrypt (cert, enc, plain, len);
  X509_free (cert);
  return (len);
}

/*
 * Asymetric private key decryption using a key file.  Return the decrypted
 * length.
 */
int crypt_pk_decrypt (char *keyname, char *passwd, char *plain, char *enc)
{
  EVP_PKEY *key;
  int len;
  
  if ((key = crypt_get_pkey (keyname, passwd)) == NULL)
    return (-1);
  len = EVP_PKEY_decrypt (plain, enc, EVP_PKEY_size(key), key);
  EVP_PKEY_free (key);
  return (len);
}

/*
 * generate key for DES3 cypher
 */
DESKEY crypt_des3_keygen (DESKEY key)
{
  DES_cblock *k;
  int i;

  k = (DES_cblock *) key;
  for (i = 0; i < 3; i++)
  {
    DES_random_key (k);
    k++;
  }
  return (key);
}

/*
 * Encrypt len bytes in plain to enc assigning key.  enc and plain may
 * point to the same buffer, but must be len + 2 * sizeof (DES_cblock)
 * in size.  Return the encrypted length.
 *
 * Note the salt is pre-pended to plain and should be removed on decryption.
 * Also padding is added to the end to enforce blocking requirements
 * of the cypher.  The character used is the pad length.
 */

int crypt_des3_encrypt (unsigned char *enc, DESKEY key,
    unsigned char *plain, int len)
{
  int i;
  DES_cblock *k, salt;
  DES_key_schedule ks[3];
  unsigned char *p;

  DES_random_key (&salt);
  crypt_des3_keygen (key);
  k = (DES_cblock *) key;
  for (i = 0; i < 3; i++)
    DES_set_key_checked (k + i, ks + i);
  p = (unsigned char *) malloc (len + sizeof (DES_cblock) * 2);
  /*
   * add salt to the front
   */
  memcpy (p, &salt, sizeof (DES_cblock));
  memcpy (p + sizeof (DES_cblock), plain, len);
  len += sizeof (DES_cblock);
  /*
   * pad it to block size
   */
  i = sizeof (DES_cblock) - len % sizeof (DES_cblock);
  memset (p + len, i, i);
  len += i;
  /*
   * encrypt it
   */
  DES_ede3_cbc_encrypt(p, enc, len, 
    ks, ks + 1, ks + 2, &salt, DES_ENCRYPT);
  free (p);
  return (len);
}

/*
 * Decrypt len bytes in enc to plain using key. enc and plain may be
 * the same buffer.  Return the decrypted length.
 *
 * Note the salt was pre-pended to plain and removed on decryption,
 * as is the end block padding.
 */

int crypt_des3_decrypt (unsigned char *plain, DESKEY key,
    unsigned char *enc, int len)
{
  int i, sz;
  unsigned char *ep;
  DES_cblock *k, salt;
  DES_key_schedule ks[3];
  unsigned char *p;

  memset (&salt, 0, sizeof (DES_cblock));
  k = (DES_cblock *) key;
  for (i = 0; i < 3; i++)
    DES_set_key_checked (k + i, ks + i);
  ep = enc;
  DES_ede3_cbc_encrypt (ep, ep, len, ks, ks + 1, ks + 2, 
    &salt, DES_DECRYPT) - sizeof (DES_cblock);
  len -= sizeof (DES_cblock);
  /*
   * remove salt and padding
   */
  memmove (plain, enc + sizeof (DES_cblock), len);
  len -= plain[len-1];
  return (len);
}

#ifdef UNITTEST

int main (int argrc, char **argv)
{
  int l, len;
  char *ch;
  char *s = "The quick brown fox jumped over the lazy dogs";
  char enc[1024], plain[1024];
  X509 *cert;
  DESKEY key;

  chdir ("..");
  SSLeay_add_all_algorithms();
  cert = crypt_get_X509 ("security/phineas.pem", NULL);
  if (cert == NULL)
    exit (1);
  debug ("DN: %s\n", crypt_X509_dn (cert, plain , 1024));
  X509_free (cert);
  cert = crypt_get_X509 ("security/sslcert.pfx", "123456");
  if (cert == NULL)
    exit (1);
  debug ("DN: %s\n", crypt_X509_dn (cert, plain , 1024));
  cert = crypt_get_X509 ("security/sslcert.pfx", "123457");
  if (cert != NULL)
  {
    error ("certs/sslcert.pfx shouldn't have opened!\n");
    X509_free (cert);
  }
  X509_free (cert);
  len = crypt_pk_encrypt ("security/phineaskey.pem", "", plain, 
    enc, s, strlen (s) + 1);
  debug ("DN: %s\n", plain);
  if (len > 0)
  {
    len = crypt_pk_decrypt ("security/phineas.pfx", "changeit", plain, enc);
    if (len > 0)
      debug ("decrypted to '%s'\n", plain);
    else
      error ("pem decryption failed\n");
  }
  else;
  ch = readfile ("certs/ssl_cert.pem", &l);
  debug ("read %d\n", l);
  ch = (char *) realloc (ch, l + DESKEYSZ);
  len = crypt_des3_encrypt (ch, key, ch, l);
  debug ("encrypted len %d...\n", len);
  len = crypt_des3_decrypt (ch, key, ch, len);
  debug ("decrypt len %d/%d\n%.*s", len, l, len, ch);
  free (ch);
  info ("%s unit test completed\n", argv[0]);
}

#endif /* UNITTEST */
