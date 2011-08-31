/*
 * mime.h
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

MIME *mime_alloc ();
MIME *mime_parse (char *buf);
char *mime_getHeader (MIME *mime, char *name);
int mime_setHeader (MIME *mime, char *header, char *value, int pos);
int mime_getLength (MIME *mime);
int mime_setLength (MIME *mime, int len);
int mime_getBoundary (MIME *mime, char *boundary, int sz);
int mime_setBoundary (MIME *mime, char *attr);
MIME *mime_setMultiPart (MIME *mime, MIME *part);
MIME *mime_getMultiPart (MIME *mime, int part);
unsigned char *mime_getBody (MIME *mime);
int mime_setBody (MIME *mime, unsigned char *body, int len);
char *mime_format (MIME *mime);

#endif __MIME__
