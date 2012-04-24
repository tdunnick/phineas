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

/*
 * create an XML encryption envelope
 */
XML *xcrypt_encrypt (unsigned char *data, int len,
  char *unc, char *id, char *passwd);
/*
 * extract data from an XML encryption envelope
 */
int xcrypt_decrypt (XML *payload, unsigned char **data,
    char *unc, char *dn, char *passwd);
