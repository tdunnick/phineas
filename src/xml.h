/*
 * xml.h
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


#ifndef __XML__
#define __XML__

/*
 * xml document
 */
typedef struct xml
{
  char path_sep,			/* for path parsing		*/
       indx_sep;
  void *doc;				/* XMLNODE *			*/
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
 * free an xml document 
 */
XML *xml_free (XML *xml);
/* 
 * allocate an xml document 
 */
XML *xml_alloc ();

/*
 * add an XML declaration if needed
 * return non-zero if problem
 */
int xml_declare (XML *xml);
/*
 * Retrieve first text value from a node.  
 * Returns empty strinig if path not found.
 */
char *xml_get_text (XML *xml, char *path);
/*
 * Retrieve text with a printf formatted path.
 * Return empty string if path not found.
 */
char *xml_getf (XML *xml, char *fmt, ...);
/*
 * Retrieve the snippet from path.  
 */
char *xml_get (XML *xml, char *path);
/*
 * force a text value to a node
 * return node set.
 */
int xml_set_text (XML *xml, char *path, char *text);
/*
 * force a text value to a node with a printf style path
 * return 0 if ok
 */
int xml_setf (XML *xml, char *text, char *fmt, ...);
/*
 * set the value of path, parsing the xml snippet at doc
 * return 0 if ok
 */
int xml_set (XML *xml, char *path, char *doc);
/*
 * get value as integer
 */
int xml_get_int (XML *xml, char *path);
/*
 * delete at path
 */
int xml_delete (XML *xml, char *path);
/*
 * delete attribute - if this attrib name is NULL, delete all of them
 */
int xml_delete_attribute (XML *xml, char *path, char *attrib);
/*
 * recusively delete all attributes from this part of the document
 * return non-zero if problem
 */
int xml_clear_attributes (XML *xml, char *path);
/*
 * cut a chunk of xml from the document
 */
char *xml_cut (XML *xml, char *path);
/*
 * copy a chunk of xml from the document
 */
char *xml_copy (XML *xml, char *path);
/*
 * paste a chunk of xml into the document before or after path
 * return non-zero if fails
 */
int xml_paste (XML *xml, char *path, char *doc, int append);
/*
 * Append a snippet into the document as a child of path.  Create
 * path if needed.
 * return non-zero if fails
 */
int xml_append (XML *xml, char *path, char *doc);
/*
 * Insert a snippet into the document as a child of path. Create path 
 * if needed.
 * return non-zero if fails
 */
int xml_insert (XML *xml, char *path, char *doc);
/*
 * get attribute value at path
 * return NULL if not found
 */
char *xml_get_attribute (XML *xml, char *path, char *name);
/*
 * set attribute value at path
 * if a new attribute append it to the list
 * return non-zero if fails
 */
int xml_set_attribute (XML *xml, char *path, char *name, char *value);
/*
 * return count of elements matching path
 */
int xml_count (XML *xml, char *path);
/*
 * append a key to this path with given index. Return new path 
 * length or -1 if bigger than MAX_PATH
 */
int xml_pathadd (XML *xml, char *path, int index, char *key);
/*
 * return the root key tag
 */
char *xml_root (XML *xml);
/*
 * Replace path with path of the first child.  Return the path length
 * or 0 if there are no children.
 */
int xml_first (XML *xml, char *path);
/*
 * Replace path with path of the last child.  Return the path length
 * or 0 if there are no children.
 */
int xml_last (XML *xml, char *path);
/*
 * Replace path with the path of the next sibling.  Return the path
 * length or 0 if no more siblings.
 */
int xml_next (XML *xml, char *path);
/*
 * Replace path with the path of the previous sibling.  Return the path
 * length or 0 if no more siblings.
 */
int xml_prev (XML *xml, char *path);
/*
 * set the path separators
 */
int xml_path_opts (XML *xml, int path_sep, int indx_sep);
/*
 * Parse and return XML document in this buf  
 */
XML *xml_parse (char *buf);
/*
 * coellese all text nodes in the docuement
 * return non-zero if fails
 */
int xml_normalize (XML *xml);
/*
 * beautify all the nodes in a document
 * indent sets number of spaces indent for each child tag
 * return non-zero if fails
 */
int xml_beautify (XML *xml, int indent);
/*
 * format a document to text
 */
char *xml_format (XML *xml);
/*
 * write xml to a stream
 * return number of bytes written
 */
int xml_write (XML *xml, FILE *fp);
/*
 * save a document to a file
 * return 0 if successful
 */
int xml_save (XML *xml, char *filename);
/*
 * load xml document from a stream
 */
XML *xml_read (FILE *fp);
/*
 * load XML from a file
 */
XML *xml_load (char *filename);


#endif /* __XML__ */
