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
#include "util.h"
#include "log.h"
#endif
#include "crypt.h"

#ifndef debug
#define debug(fmt...)
#endif

/*
 * Get a distinguished name from an X509 subject.
 *
 * cert X509 certificate
 * dn buffer for name
 * len buffer size
 * returns pointer to the dn
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
 *
 * Note we limit the total number of subject items to 20 (there should 
 * only be 8!) and limit the total size of the dn to DNSZ.
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
 * unc path to certificate
 * passwd password to read if needed
 * return X509 certificate or NULL if fails
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
 *
 * name name of the certificate file
 * passwd needed to decrypt if necessary
 * return the private key.
 *
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
 *
 * cert X509 certificate with public key
 * enc buffer for encrypted data
 * plain buffer for plain data
 * len of plain data
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
 *
 * unc location of certificate
 * passwd password used to access certificate if encrypted
 * dn DNSZ buffer for distinguished name found on the certificate
 * enc buffer for encrypted data
 * plain buffer for plain data
 * len length of plain data
 * returns length of encrypted data.
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
 * Asymetric private key decryption using a key file.  
 *
 * keyname name of private key file
 * passwd password for private key file
 * plain buffer for decrypted data
 * enc buffer for encrypted data
 * returns length of plain data
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
 * returns EVP encryption cipher
 * how one of the encryption algorithms in crypt.h
 */
const EVP_CIPHER *crypt_cipher (int how)
{
  typedef const EVP_CIPHER *(*CIPHERP)(void);
  CIPHERP c[] =
  {
     NULL, 
     EVP_des_ede3_cbc, 
     EVP_aes_128_cbc, 
     EVP_aes_192_cbc, 
     EVP_aes_256_cbc
  };

  if ((how < FIRSTCIPHER) || (how > LASTCIPHER))
    return (NULL);
  return ((c[how])());
}

/* 
 * returns size of encryption key
 * how one of the encryption algorithms in crypt.h
 */
int crypt_keylen (int how)
{
  int keylen[] = { 0, 24, 16, 24, 32 };
  if ((how < FIRSTCIPHER) || (how > LASTCIPHER))
    return (0);
  return (keylen[how]);
}

/*
 * returns size of encryption block
 * how one of the encryption algorithms in crypt.h
 */
int crypt_blocksz (int how)
{
  if ((how < FIRSTCIPHER) || (how > LASTCIPHER))
    return (0);
  if (how == TRIPLEDES)
    return (8);
  return (16);
}

/*
 * initialize randomizer if needed
 */
void crypt_random ()
{
  time_t t;
  if (!RAND_status ())
  {
    debug ("seeding RAND_\n");
    time (&t);
    RAND_seed (ctime (&t), 26);
  }
}

/*
 * generate an initial vector
 * iv is vector destination
 * how one of the encryption algorithms in crypt.h
 * return vector size
 */
int crypt_iv (unsigned char *iv, int how)
{
  int len;

  if (len = crypt_blocksz (how))
  {
    crypt_random ();
    RAND_bytes (iv, len);
  }
  return (len);
}

/*
 * sets parity for a DES key 
 */
void crypt_parity (unsigned char *key)
{
  int k, b, p;

  for (k = 0; k < 24; k++)
  {
    p = 1;				/* assume odd parity		*/
    for (b = 0x80; b > 1; b >>= 1)
    {
      if (key[k] & b)
        p = !p;
    }
    if (p)
      key[k] |= 1;
    else
      key[k] &= 0xfe;			/* reset parity bit		*/
  }
}

/*
 * generate a cyptographic key
 *
 * key 32 byte buffer for encryption key 
 * how one of the encryption algorithms in crypt.h
 * returns key length
 */
int crypt_key (unsigned char *key, int how)
{
  int klen;

  if ((klen = crypt_keylen (how)) == 0)
    return (0);

  crypt_random ();
  RAND_bytes (key, klen);
  if (how == TRIPLEDES)			/* set key parity bits		*/
    crypt_parity (key);
  return (klen);
}

/*
 * generate a password based key for encryption use
 *
 * key generated key
 * pw password
 * salt 8 byte buffer for perturbing the key - may be NULL
 * how one of the encryption algorithms in crypt.h
 * returns key length
 */
int crypt_pbkey (unsigned char *key, char *pw, char *salt, int how)
{
  int len;
  const EVP_CIPHER *type;
  unsigned char iv[32];		/* we throw this away		*/

  if ((type = crypt_cipher (how)) == NULL)
    return (0);
  len =  EVP_BytesToKey (type, EVP_sha1(), salt, pw, strlen (pw),
      5, key, iv);
  if (how == TRIPLEDES)
    crypt_parity (key);
  return (len);
}

/*
 * read binary, hex, or base 64 encoded symetric key from a file
 *
 * key 32 byte buffer for encryption key 
 * unc path to file
 * return it's length or 0 if not valid key
 *
 * First try hex encoded, then base 64, and finally binary.  Test
 * for whether we have a key by looking at the derived key length
 * to see if it "fits" one of our algorithms. We have to fudge a bit
 * on the parse in order to accomodate extraneous trailing stuff
 * like CR/LF pairs. This is of course not fool proof, but hopefully 
 * good enough.
 */
#define ISKEY(len) ((len==16)||(len==24)||(len==32))

int crypt_fkey (unsigned char *key, char *unc)
{
  unsigned char *ch;
  int l, len;

  if ((key == NULL) || (unc == NULL))
    return (0);
  ch = readfile (unc, &len);
  debug ("read %d from %s\n", len, unc);
  if (len > SKEYSZ * 3)		/* that is just too much crap	*/
    return (0);
  l = hex_decode (key, ch);	/* try hex			*/
  if (!ISKEY (l))
  {
    l = b64_decode (key, ch);	/* then base 64			*/
    if (!ISKEY (l))
    {
      if (ISKEY (len))		/* binary must be perfect size	*/
	memcpy (key, ch, l = len);
      else
	l = 0;
    }
  }
  free (ch);
  return (l);
}

/*
 * general purpose EVP based crypto function
 *
 * dst destination data (may be same a src)
 * src source data
 * key encryption key
 * len source data length
 * how one of the encryption algorithms in crypt.h
 * encrypt encrypts when true, decrypts when false
 * returns length of dst buffer
 */
int crypt_copy (unsigned char *dst, unsigned char *src,
    unsigned char *key, int len, int how, int encrypt)
{
  EVP_CIPHER_CTX ctx;
  const EVP_CIPHER *cipher;
  unsigned char *s, *d, iv[16];
  int l, blocksz;

  if ((cipher = crypt_cipher (how)) == NULL)
    return (0);

  blocksz = crypt_blocksz (how);
  s = (unsigned char *) malloc (len + blocksz * 2);
  if (encrypt)
  {
    crypt_iv (iv, how);
    memcpy (s, iv, blocksz);
    memcpy (s + blocksz, src, len);
    len += blocksz;
    d = dst;
  }
  else
  {
    d = s;
    s = src;
  }

  debug ("initializing cipher\n");
  EVP_CIPHER_CTX_init (&ctx);
  EVP_CipherInit (&ctx, cipher, key, iv, encrypt);
  debug ("running cipher\n");
  EVP_CipherUpdate (&ctx, d, &l, s, len);
  len = l;
  EVP_CipherFinal (&ctx, d + len, &l);
  len += l;
  debug ("cleaning up final len=%d\n", len);
  EVP_CIPHER_CTX_cleanup (&ctx);
  if (!encrypt) 		/* remove the iv		*/
  {
    len -= blocksz;
    memcpy (dst, d + blocksz, len);
    s = d;
  }
  free (s);
  return (len);
}

/*
 * general purpose EVP based encryption
 *
 * enc encrypted data (may be same a plain)
 * plain data to be encrypted
 * key encryption key generated by call
 * len source data length
 * how one of the encryption algorithms in crypt.h
 * returns length of dst buffer
 */
int crypt_encrypt (unsigned char *enc, unsigned char *plain,
    unsigned char *key, int len, int how)
{
  crypt_key (key, how);
  return (crypt_copy (enc, plain, key, len, how, 1));
}

/*
 * general purpose EVP based decryption
 *
 * plain decrypted data (may be same a enc)
 * enc encrypted data
 * key encryption key
 * len source data length
 * how one of the encryption algorithms in crypt.h
 * returns length of decrypted data
 */
int crypt_decrypt (unsigned char *plain, unsigned char *enc,
    unsigned char *key, int len, int how)
{
  return (crypt_copy (plain, enc, key, len, how, 0));
}

#ifdef UNITTEST

testpbk2 (int how)
{
  char *n[] = { NULL, "des3", "aes128", "aes192", "aes256" };
  char b1[PKEYSZ], b2[PKEYSZ];
  char *pw = "thePassword";
  int l1, l2;

  l1 = crypt_pbkey (b1, pw, NULL, how);
  l2 = crypt_pbkey (b2, pw, NULL, how);
  if (l1 != l2)
    error ("%s keys length doesn't match\n", n[how]);
  else if (memcmp (b1, b2, l1))
    error ("%s key values don't match\n", n[how]);
}

testpbk ()
{
  int i;

  for (i = FIRSTCIPHER; i <= LASTCIPHER; i++)
    testpbk2 (i);
}

int main (int argrc, char **argv)
{
  int type, l, len;
  char *ch, *ch2;
  char *s = "The quick brown fox jumped over the lazy dogs";
  char enc[1024], plain[1024];
  X509 *cert;
  unsigned char key[32];

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
  ch = readfile ("cc.bat", &l);
  debug ("read %d\n", l);
  ch = (char *) realloc (ch, l + 64);
  type = TRIPLEDES;
  len = crypt_encrypt (ch, ch, key, l, type);
  debug ("encrypted len %d...\n", len);
  len = crypt_decrypt (ch, ch, key, len, type);
  debug ("decrypt len %d/%d\n%.*s", len, l, len, ch);
  free (ch);
  testpbk ();
  info ("%s unit test completed\n", argv[0]);
}

#endif /* UNITTEST */
