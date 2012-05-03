/*
 * ebxml.h
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
#ifndef __EBXML__
#define __EBXML__

#include "dbuf.h"
#include "xml.h"
#include "queue.h"
#include "net.h"

/*
 * useful tags for accessing ebXML data
 */
extern char
   *soap_hdr,
   *soap_bdy,
   *soap_manifest,
   *soap_dbinf,
   *soap_ack;

/*
 * Get a field from configuration with prefix
 */
char *ebxml_get (XML *xml, char *prefix, char *name);

/*
 * Set a field from a configuration with prefix
 */
int ebxml_set (XML *xml, char *prefix, char *suffix, char *value);

/*
 * receive a reply and put it in this buffer
 */
DBUF *ebxml_receive (NETCON *conn);

/*
 * Beautify and format an ebXML message.  Returns an allocated
 * message which the caller should free.
 */
char *ebxml_beautify (char *buf);

/*
 * A folder polling processor for ebxml queues - register this with
 * the fpoller.
 *
 * This initializes and pushes a queue row.  Once queued it moves
 * the file to a processed point.
 *
 * xml - sender's configuration
 * prefix - xml path to this folder map
 * fname - file to be queued
 */
int ebxml_fprocessor (XML *xml, char *prefix, char *fname);

/*
 * A queue polling processor for ebxml queues - register this with
 * the qpoller.
 *
 * This builds an ebXML MIME message, opens a connection to a
 * receiver, sends the request, processes the response, and
 * finally updates the queue status.
 */
int ebxml_qprocessor (XML *xml, QUEUEROW *r);

/*
 * Process an incoming request and return the response.  The caller
 * should free the response after sending.
 */
char *ebxml_process_req (XML *xml, char *buf);

/*
 * Load, allocate, and return an xml template
 */
XML *ebxml_template (XML *xml, char *tag);

#endif /* __EBXML__ */
