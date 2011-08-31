/*
 * crypt.h
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
#ifndef __CRYPT__
#define __CRYPT__
#include <openssl/pem.h>
#include <openssl/des.h>
#include <openssl/pkcs12.h>
/*
 * note a DES_cblock is 8 bytes, so a DES3 key is 24
 */
#define DESKEYSZ 24
typedef unsigned char DESKEY[DESKEYSZ];

#define DNSZ 1024	/* size of a distinguish name (subject)	*/

/*
 * Get the distinguished name from an X509 cert in "PHINMS" format
 */
char *crypt_X509_dn (X509 *cert, char *dn, int len);
/*
 * Read an X509 certificate from the unc
 */
X509 *crypt_get_X509 (char *unc, char *passwd);

/*
 * Read a private key from the unc
 */
EVP_PKEY *crypt_get_pkey (char *unc, char *passwd);

/*
 * Use these for certificate related asymetric cyphers.  These
 * generally encrypt the symetric key which then encrypts the payload.
 * They return the resulting cypher length.
 */
int crypt_pk_encrypt (char *unc, char *passwd, char *dn, 
  char *enc, char *plain, int len);
int crypt_pk_decrypt (char *keyname, char *passwd, char *plain, char *enc);

/*
 * PHINMS appears to only use triple DES for the symetric cypher, so
 * we'll at least start by supporting it...
 * These return the resulting cypher length.
 */
DESKEY crypt_des3_keygen (DESKEY key);
int crypt_des3_encrypt (unsigned char *enc, DESKEY key,
    unsigned char *plain, int len);
int crypt_des3_decrypt (unsigned char *plain, DESKEY key,
    unsigned char *enc, int len);

#endif /* __CRYPT__ */
