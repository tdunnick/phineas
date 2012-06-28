/*
 * payload.h
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
#ifndef __PAYLOAD__
#define __PAYLOAD__
#include "mime.h"

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
    char *unc, char *dn, char *pw); 

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
    char *fname, char *org, char *unc, char *dn, char *pw);

#endif
