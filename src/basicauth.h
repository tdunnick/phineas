/*
 * basicauth.h
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
 *
 * Functions used to run data through programs that act as filters.
 * These independent programs can be configured and selectively run
 * creating data processing extensions for PHINEAS senders and receivers.
 */

#ifndef __BASICAUTH__
#define __BASICAUTH__

#include "dbuf.h"
#include "xml.h"

/*
 * Check basic authentication.  Return zero if authenticated.
 * Return -1 if failed authentication.
 * Return 1 if authentication not attempted.
 */
int basicauth_check (XML *xml, char *path, char *req);
/*
 * Allocate and return an authorization required response
 */
DBUF *basicauth_response (char *realm);
/*
 * Create a basic authentication HTTP header in a buffer
 * b is the buffer location
 * uid and password are the authentication credentials
 */
char *basicauth_request (char *b, char *uid, char *passwd);

#endif
