/*
 * mime.h
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
#ifndef __MIME__
#define __MIME__

#include <stdio.h>
#include <string.h>

#define MIME_CONTENT "Content-Type"
#define MIME_TEXT "text/plain"
#define MIME_XML "text/xml"
#define MIME_OCTET "Application/Octet-Stream"
#define MIME_MULTIPART "multipart/related"
#define MIME_ENCODING "Content-Transfer-Encoding"
#define MIME_BASE64 "base64"
#define MIME_QUOTED "quoted/printable"
#define MIME_LENGTH "Content-Length"
#define MIME_CONTENTID "Content-ID"
#define MIME_MESSAGEID "Message-ID"
#define MIME_DISPOSITION "Content-Disposition"

typedef struct mime
{
  struct mime *next;
  int len;
  char *headers;
  unsigned char *body;
} MIME;

/*
 * allocate a mime structure
 */
MIME *mime_alloc ();
/*
 * free a mime structure including all parts of a multipart
 */
MIME *mime_free (MIME *m);
/*
 * parse a mime package
 */
MIME *mime_parse (char *buf);
/*
 * get the value location of a mime header, delimited by '\r\n' 
 */
char *mime_getHeader (MIME *mime, char *name);
/*
 * Set a header, preferably at the position given.  Return actual place.
 * Use a big pos to force it to append.
 */
int mime_setHeader (MIME *mime, char *header, char *value, int pos);
/*
 * get Content-Length
 */
int mime_getLength (MIME *mime);
/*
 * set Content-Length
 */
int mime_setLength (MIME *mime, int len);
/*
 * Get and format a boundary if there. Return it's size, -1 if too big
 * or 0 if not found.  If buf is not NULL, copy the boundary to it
 * to used in searching for MIME multi-parts.
 */
int mime_getBoundary (MIME *mime, char *boundary, int sz);
/*
 * set a mulitpart mime boundary - the current time and a random
 * number is used to set the boundary text
 */
int mime_setBoundary (MIME *mime, char *attr);
/*
 * add a multipart chunk - multipart must be set
 */
MIME *mime_setMultiPart (MIME *mime, MIME *part);
/*
 * Return a mime part - indexed from 1
 */
MIME *mime_getMultiPart (MIME *mime, int part);
/*
 * return a pointer to the body of this message
 */
unsigned char *mime_getBody (MIME *mime);
/*
 * copy the body from a buffer, set Content-Length, and return buffer len
 * if incoming len < 1, use the buffer string length
 */
int mime_setBody (MIME *mime, unsigned char *body, int len);
/*
 * Get the size of the formatted mime message.
 * Sets the Content-Length in the header...
 */
int mime_size (MIME *m);
/*
 * Format a MIME message to an allocated character buffer.  Caller
 * is responsible for freeing this buffer.
 */
char *mime_format (MIME *mime);

#endif /* __MIME__ */
