/*
 * xml.h
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


#ifndef __XML__
#define __XML__

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

/*
 * xml document
 */
typedef struct xml
{
  char path_sep,			/* for path parsing		*/
       indx_sep;
  XMLNODE *doc;
} XML;

/*
 * Documents are built and accessed by a "path".
 * Document paths consist of element keys separated by path separators with
 * optional indexes to distinguish multiple nodes for the same key
 * in a node list. Everything between the indx_sep and next path_sep is
 * ignored so that indexes may be "closed" if you like.  Also, the leading 
 * path separator is optional.  For example...
 *
 *  foo.bar[2.stuff
 *
 * If the path_sep was '/' and the indx_sep was '(' then...
 *  
 *  /foo/bar(2)/stuff
 */

#define DFLT_PATH_SEP '.';
#define DFLT_INDX_SEP '[';

/*
 * xml related string functions
 */
char *xml_to_entity (char *d, char *s);
char *xml_from_entity (char *d, char *s);
char *xml_trim (char *s);

/*
 * document oriented functions
 */
XML *xml_free (XML *xml);
XML *xml_alloc ();
XMLNODE *xml_declare (XML *xml);
XMLNODE *xml_root (XML *xml);
XMLNODE *xml_find_parent (XML *xml, char *path, XMLNODE **parent);
XMLNODE *xml_find (XML *xml, char *path);
XMLNODE *xml_force (XML *xml, char *path, XMLNODE **parent);
char *xml_get (XML *xml, char *path);
char *xml_get_text (XML *xml, char *path);
char *xml_getf (XML *xml, char *fmt, ...);
XMLNODE *xml_set (XML *xml, char *path, char *snippet);
XMLNODE *xml_set_text (XML *xml, char *path, char *text);
XMLNODE *xml_setf (XML *xml, char *text, char *fmt, ...);
int xml_get_int (XML *xml, char *path);
int xml_delete (XML *xml, char *path);
/*
 * delete attribute - if the attrib name is NULL, delete all of them
 */
int xml_delete_attribute (XML *xml, char *path, char *attrib);
XMLNODE *xml_cut (XML *xml, char *path);
XMLNODE *xml_copy (XML *xml, char *path);
XMLNODE *xml_paste (XML *xml, char *path, XMLNODE *n, int append);
XMLNODE *xml_add (XML *xml, char *path, char *snippet, int append);
XMLNODE *xml_append (XML *xml, char *path, char *snippet);
XMLNODE *xml_insert (XML *xml, char *path, char *snippet);
XMLNODE *xml_set_attribute (XML *xml, char *path, char *name, char *value);
char *xml_get_attribute (XML *xml, char *path, char *name);
int xml_count (XML *xml, char *path);
int xml_path_opts (XML *xml, int path_sep, int indx_sep);
XML *xml_parse (char *buf);
XMLNODE *xml_normalize (XML *xml);
XMLNODE *xml_beautify (XML *xml, int indent);
char *xml_format (XML *xml);
int xml_save (XML *xml, char *filename);
XML *xml_load (char *filename);


#endif /* __XML__ */
