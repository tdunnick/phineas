/*
 * queue.h
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
#ifndef __QUEUE__
#define __QUEUE__

#include "task.h"
#include "xml.h"

#define QMAXFIELDS 100			/* most fields pre queue	*/
#define QBUFSZ 4096			/* biggest expected row		*/

#define EBXMLSENDQ "EbXmlSndQ"
#define EBXMLRCVQ "EbXmlRcvQ"

typedef struct queue QUEUE;
typedef struct queuerow QUEUEROW;

/*
 * A connection must have an API implementation.  It includes
 * a pointer to the implementations needed data structures and a
 * set of API callback functions as follow:
 *
 * connect() parse the unc for host, port, db, etc
 * shutdown () closes the connecition
 * push() add a row
 * pop() remove a row
 * next() read the next row
 * prev() read the previous row
 */
typedef struct queueconn
{
  struct queueconn *next;	/* the list			*/
  char *name;			/* the name of this connection	*/
  char *type; 			/* the connection type		*/
  char *unc;			/* the resource path		*/
  char *user;			/* UID for authenticaton	*/
  char *passwd;			/* password for authentication	*/
  char *driver;			/* driver used			*/
  void *conn;			/* private data			*/
  int (*close) (void *conn);
  int (*push) (QUEUEROW *row);
  int (*del) (QUEUE *q, int rowid);
  QUEUEROW *(*pop) (QUEUE *q);
  QUEUEROW *(*get) (QUEUE *q, int rowid);
  QUEUEROW *(*nextrow) (QUEUE *q, int rowid);
  QUEUEROW *(*prevrow) (QUEUE *q, int rowid);
} QUEUECONN;

/*
 * Describes the basic fields and data structure.  Each implemtation
 * needs
 */
typedef struct queuetype
{
  struct queuetype *next;
  int numfields;			/* number of fields		*/
  char *name;				/* needed by implementation API	*/
  char *field[];			/* the field names		*/
} QUEUETYPE;

/*
 * the shared queues
 */
typedef struct queue
{
  struct queue *next;			/* next known queue		*/
  MUTEX mutex;				/* for thread safety		*/
  char *name;				/* name of this queue		*/
  QUEUETYPE *type;			/* type of queue		*/
  QUEUECONN *conn;			/* the queue connection 	*/
  char *table;				/* "table" name			*/
} QUEUE;

/*
 * one data row of a queue
 */
typedef struct queuerow
{
  QUEUE *queue;				/* queue for this row		*/
  int rowid;				/* 0 if not set			*/
  char *field[];			/* fields for this row		*/
} QUEUEROW;

/********************** general queue functions ************************/
/* initialize queuing from a common configuration file */
int queue_init (XML *config);
/*  shut down the queuing system */
void queue_shutdown (void);
/* find a queue by name */
QUEUE *queue_find (char *name);
/* get "top" row of a queue */
QUEUEROW *queue_pop (QUEUE *q);
/* add or update row of a queue */
int queue_push (QUEUEROW *r);
/* get a row of a queue */
QUEUEROW *queue_get (QUEUE *q, int rowid);
/* get the next row of a queue */
QUEUEROW *queue_next (QUEUE *q, int rowid);
/* get the previous row of a queue */
QUEUEROW *queue_prev (QUEUE *q, int rowid);
/* delete a queue row */
int queue_delete (QUEUE *q, int rowid);

/************************ queue row function **************************/
/* get a fresh queue row */
QUEUEROW *queue_row_alloc (QUEUE *q);
/* free a queue row */
QUEUEROW *queue_row_free (QUEUEROW *r);
/* find a field by name */
int queue_field_find (QUEUE *q, char *name);
/* get a field value by name */
char *queue_field_get (QUEUEROW *r, char *name);
/* set a field value by name */
int queue_field_set (QUEUEROW *r, char *name, char *value);

#endif /* __QUEUE__ */
