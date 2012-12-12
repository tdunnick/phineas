/*
 * xmln.h
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


#ifndef __XMLN__
#define __XMLN__

#include "dbuf.h"

/* 
 * xml node types 
 */
#define XML_TEXT '"' 
#define XML_DECL '?'
#define XML_COMMENT '!'
#define XML_ELEMENT '<'
#define XML_ATTRIB '='

/* 
 * xml node 
 */
typedef struct xmlnode 
{
  struct xmlnode *next;			/* sibling node			*/
  int type;				/* node type			*/
  char *key;				/* the tag			*/
  struct xmlnode *attributes;		/* a list of attribute nodes	*/
  void *value;				/* text or list of child nodes	*/
} XMLNODE;


/********************* data manipulations **************************/

/*
 * convert text to xml entities
 */
char *xml_to_entity (char *dst, char *s);
/*
 * convert xml entities back to text
 */
char *xml_from_entity (char *dst, char *s);
/* 
 * remove leading and trailing white space from a string
 * return the resulting string
 */
char *xml_trim (char *value);

/*********************** node manipulations **************************/

/* 
 * free an xml node (subtree)
 */
XMLNODE *xmln_free (XMLNODE *node);
/* 
 * allocate an xml node setting it's type and copying the key (tag)
 * until an EOS, space, '=', '>', or '/>' is found.
 */
XMLNODE *xmln_alloc (int type, char *key);
/* 
 * remove leading and trailing white space from a value
 * return the resulting value length
 */
int xmln_trim (XMLNODE *node);
/* 
 * Set a text or attribute node value with size len.  This reallocates
 * value.
 */
char *xmln_set_val (XMLNODE *node, char *value, int len);
/*
 * return a nodes value if a leaf, otherwise NULL
 */
char *xmln_get_val (XMLNODE *n);
/*
 * set attribute value at node
 * if a new attribute append it to the list
 * return the attribute node
 */
XMLNODE *xmln_set_attr (XMLNODE *n, char *name, char *value);
/*
 * get attribute value a node for name
 * return NULL if not found
 */
char *xmln_get_attr (XMLNODE *n, char *name);
/*
 * allocate a text node
 */
XMLNODE *xmln_text_alloc (char *value, int len);
/*
 * insert a node (list) into a chain at specified place
 * if place is NULL append to the chain
 */
XMLNODE *xmln_insert (XMLNODE *chain, XMLNODE *place, XMLNODE *n);
/*
 * delete a node in this chain
 */
XMLNODE *xmln_delete (XMLNODE *chain, XMLNODE *place);
/*
 * recursively copy a branch of the document tree
 */
XMLNODE *xmln_copy (XMLNODE *node);
/*
 * return number of nodes in this chain (siblings)
 */
int xmln_len (XMLNODE *node);
/*
 * return true if this node can have child nodes for it's value
 */
int xmln_isparent (XMLNODE *node);
/*
 * return true if this node has a child node
 */
int xmln_haschild (XMLNODE *node);
/*
 * search a node list for matching name (key) and index (from 0)
 * return the node or NULL if not found
 */
XMLNODE *xmln_key (XMLNODE *node, char *name, int index);
/*
 * search a node list for a matching type and index (from 0)
 * return the node or NULL if not found
 */
XMLNODE *xmln_type (XMLNODE *node, int type, int index);

/****************************** parsing *****************************/

/*
 * parse attributes
 * ATTRIB :: SPACE ATTRIB
 *        :: TEXT '="' TEXT '"'
 *        :: NULL
 * return attribute node list
 */
XMLNODE *xmln_parse_attr (char *buf);
/*
 * parse one xml element
 * ELEMENT  :: '<' TAG ATTRIB '>' XMLNODES '</' TAG '>'
 *          :: '<' TAG ATTRIB '/>'
 *          :: NULL
 * return element XMLNODE and update parse position
 */
XMLNODE *xmln_parse_elem (char **buf);
/* parse decls
 * DECLS    :: '<' NONALPHA TEXT '>' DECLS
 *          :: COMMENT DECLS
 *          :: TEXT DECLS
 *          :: NULL
 * return list of decl nodes parsed and update parse position
 */
XMLNODE *xmln_parse_decls (char **buf);
/*
 * parse a node list
 * XMLNODES    :: DECLS XMLNODES
 *          :: ELEMENT XMLNODES
 *          :: NULL
 * return head of the node list and update parse position
 */
XMLNODE *xmln_parse (char **buf);
/*
 * Parse a complete document.
 *
 * DOC      :: XMLDECL DECLS ELEMENT DECLS
 * XMLDECL  :: '<?xml' ATTRIB '/>' 
 *          :: NULL
 */
XMLNODE *xmln_parse_doc (char **buf);

/********************** global manipulations *********************/
/*
 * add an XML declaration if needed
 */
XMLNODE *xmln_declare (XMLNODE *n);
/*
 * coellese all the text nodes, removing those that are empty
 */
XMLNODE *xmln_normalize (XMLNODE *node);
/*
 * beautify a text node
 */
int xmln_text_beautify (XMLNODE *node, int indent, int level);
/* 
 * modify text nodes for pretty indenting 
 */
XMLNODE *xmln_beautify (XMLNODE *node, int indent, int level);
/* 
 * format to text 
 */
int xmln_format (XMLNODE *node, DBUF *b);

/**************************** i/o *******************************/

/*
 * read xml from a stream
 */
XMLNODE *xmln_read (FILE *fp, int doc);
/*
 * load XML from a file
 */
XMLNODE *xmln_load (char *filename, int doc);
/*
 * write xml to a stream
 * return number of bytes written
 */
int xmln_write (XMLNODE *x, FILE *fp);
/*
 * save a document to a file
 * return 0 if successful
 */
int xmln_save (XMLNODE *x, char *filename);

#endif /* __XMLN__ */
