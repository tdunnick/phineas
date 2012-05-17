/*
 * xcrypt.h
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
#ifndef __XCRYPT__
#define __XCRYPT__
#include "xml.h"
#include "crypt.h"

/*
 * Fill in an ebxml encryption envelope.
 *
 * data, len - data of len to encrypt
 * unc, passwd - identify certificate to use
 * dn gets DN of certificate and should be DNSZ or may be NULL
 *
 * This uses triple DES encryption for the data, and the certificates
 * asymetric public key to encrypt the DES key.
 *
 * Return envelope or NULL if fails
 */
XML *xcrypt_encrypt (unsigned char *data, int len,
  char *unc, char *id, char *passwd, int how);
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
    char *unc, char *dn, char *passwd);
