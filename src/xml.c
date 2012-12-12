/*
 * xml.c
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
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>

#ifdef UNITTEST
#include "unittest.h"
#endif

#include "dbuf.h"
#include "xmln.h"
#include "xml.h"

#ifndef debug
#define debug(fmt...)
#endif

/************************** private functions *******************/
/*
 * parse next key and index from path
 * return next parse point
 */
char *xml_pathkey (XML *xml, char *path, char *key, int *index);
/*
 * return the node and set parent matching this name path
 */
XMLNODE *xml_find_parent (XML *xml, char *path, XMLNODE **parent);
/*
 * return node matching path
 */
XMLNODE *xml_find (XML *xml, char *path);
/*
 * Force an element at the given path.
 * If it already exists, simply return it and set it's parent.  
 * Otherwise create it.
 */
XMLNODE *xml_force (XML *xml, char *path, XMLNODE **parent);


/************************* allocation ***************************/

/* 
 * free an xml document 
 */
XML *xml_free (XML *xml)
{
  if (xml == NULL)
    return (NULL);
  xmln_free (xml->doc);
  free (xml);
  return (NULL);
}

/* 
 * allocate an xml document 
 */
XML *xml_alloc ()
{
  XML *xml = (XML *) malloc (sizeof (XML));
  xml->path_sep = DFLT_PATH_SEP;
  xml->indx_sep = DFLT_INDX_SEP;
  xml->doc = NULL;
  return (xml);
}

/*
 * add an XML declaration if needed
 * return non-zero if problem
 */
int xml_declare (XML *xml)
{
  XMLNODE *n;
  char *p;

  if (xml == NULL)
    return (-1);
  return ((xml->doc = xmln_declare (xml->doc)) == NULL);
}

/*
 * parse next key and index from path
 * return next parse point
 */
char *xml_pathkey (XML *xml, char *path, char *key, int *index)
{
  int l;
  char *ip, *pp;

  *key = *index = l = 0;
  if ((xml == NULL) || (path == NULL) || (key == NULL) || (index == NULL))
    return (NULL);
  l = xml->path_sep;
  for (pp = path; *pp && (*pp != l); pp++);
  l = xml->indx_sep;
  for (ip = path; *ip && (*ip != l); ip++);
  if (ip < pp)
    *index = atoi (ip + 1);
  else 
    ip = pp;
  if (l = ip - path)
  {
    strncpy (key, path, l);
    key[l] = 0;
  }
  if (*pp)
    pp++;
  return (pp);
}

/*
 * return the node and set parent matching this name path
 */
XMLNODE *xml_find_parent (XML *xml, char *path, XMLNODE **parent)
{
  XMLNODE *n;
  int i, c = 0;
  char key[MAX_PATH];

  if ((xml == NULL) || (path == NULL))
    return (NULL);
  n = xml->doc;
  *parent = NULL;
  while (n != NULL)
  {
    path = xml_pathkey (xml, path, key, &i);
    if (*key != 0)
      n = xmln_key (n, key, i);
    if (*path == 0)
      return (n);
    if (n != NULL)
    {
      *parent = n;
      n = n->value;
    }
  } 
  return (NULL);
}

/*
 * return node matching path
 */
XMLNODE *xml_find (XML *xml, char *path)
{
  XMLNODE *parent;
  return (xml_find_parent (xml, path, &parent));
}

/*
 * Force an element at the given path.
 * If it already exists, simply return it and set it's parent.  
 * Otherwise create it.
 */
XMLNODE *xml_force (XML *xml, char *path, XMLNODE **parent)
{
  XMLNODE *n, **p;
  int c = 0, i;
  char key[MAX_PATH];

  if ((xml == NULL) || (path == NULL))
    return (NULL);
  p = (XMLNODE **) &xml->doc;
  if (parent != NULL)
    *parent = NULL;
  n = NULL;
  while (1)
  {
    path = xml_pathkey (xml, path, key, &i);
    if (*key == 0)
      break;
    while ((n = xmln_key (*p, key, i)) == NULL)
    {
      if ((p == (XMLNODE **) &xml->doc) && (xml_root (xml) != NULL))
      {
        debug ("trying to force new root!\n");
        return (NULL);
      }
      n = xmln_alloc (XML_ELEMENT, key);
      *p = xmln_insert (*p, NULL, n);
    }
    if (*path && (parent != NULL))
      *parent = n;
    p = (XMLNODE **) &n->value;
  } 
  debug ("forced node '%s' parent='%s'\n", n->key,
    ((parent == NULL) || (*parent == NULL)) ? "NULL" : (*parent)->key); 
  return (n);
}

/*
 * Retrieve first text value from a node.  
 * Returns empty string if path not found.
 */
char *xml_get_text (XML *xml, char *path)
{
  XMLNODE *n = xml_find (xml, path);
  if ((n != NULL) && xmln_isparent (n))
  {
    if ((n = xmln_type (n->value, XML_TEXT, 0)) != NULL)
     return (n->value);
  }
  return ("");
}

/*
 * Retrieve text with a printf formatted path.
 * Return empty string if path not found.
 */
char *xml_getf (XML *xml, char *fmt, ...)
{
  va_list ap;
  char path[MAX_PATH];

  va_start (ap, fmt);
  vsprintf (path, fmt, ap);
  va_end (ap);
  return (xml_get_text (xml, path));
}

/*
 * Retrieve the snippet from path.  
 */
char *xml_get (XML *xml, char *path)
{
  DBUF *b;
  XMLNODE *n;
  
  if ((n = xml_find (xml, path)) == NULL)
    return (NULL);
  b = dbuf_alloc ();
  xmln_format (n->value, b);
  return (dbuf_extract (b));
}

/*
 * force a text value to a node
 * return node set.
 */
int xml_set_text (XML *xml, char *path, char *text)
{
  XMLNODE *n, *p;
  if ((n = xml_force (xml, path, &p)) == NULL)
    return (-1);
  xmln_free (n->value);
  n->value = xmln_text_alloc (text, strlen (text));
  return (0);
}

/*
 * force a text value to a node with a printf style path
 * return node set.
 */
int xml_setf (XML *xml, char *text, char *fmt, ...)
{
  va_list ap;
  char path[MAX_PATH];

  va_start (ap, fmt);
  vsprintf (path, fmt, ap);
  va_end (ap);
  return (xml_set_text (xml, path, text));
}

/*
 * set the value of path, parsing the xml snippet at doc
 */
int xml_set (XML *xml, char *path, char *doc)
{
  XMLNODE *p, *n;

  if ((n = xml_force (xml, path, &p)) == NULL)
    return (-1);
  xmln_free (n->value);
  n->value = xmln_parse (&doc);
  return (0);
}

/*
 * get value as integer
 */
int xml_get_int (XML *xml, char *path)
{
  return (atoi (xml_get_text (xml, path)));
}

/*
 * delete at path
 */
int xml_delete (XML *xml, char *path)
{
  XMLNODE *c, *p;
  
  if ((c = xml_find_parent (xml, path, &p)) == NULL)
    return (-1);
  if (p == NULL)
    xml->doc = xmln_free (xml->doc);
  else  
    p->value = xmln_delete (p->value, c);
  return (0);
}

/*
 * delete attribute - if the attrib name is NULL, delete all of them
 */
int xml_delete_attribute (XML *xml, char *path, char *attrib)
{
  XMLNODE *c, *p;

  if ((c = xml_find_parent (xml, path, &p)) == NULL)
    return (-1);
  if (attrib == NULL)
    c->attributes = xmln_free (c->attributes);
  else if ((p = xmln_key (c->attributes, attrib, 0)) == NULL)
    return (-1);
  else
    c->attributes = xmln_delete (c->attributes, p);
  return (0);
}

/*
 * delete all the attributes from an XMLNODE subtree
 */
void xml_del_attributes (XMLNODE *n)
{
  while (n != NULL)
  {
    n->attributes = xmln_free (n->attributes);
    if (xmln_isparent (n))
      xml_del_attributes (n->value);
    n = n->next;
  }
}

/*
 * delete all attributes from this part of the document
 * return non-zero if problem
 */
int xml_clear_attributes (XML *xml, char *path)
{
  XMLNODE *c, *p;

  if ((c = xml_find_parent (xml, path, &p)) == NULL)
    return (-1);
  xml_del_attributes (c);
  return (0);
}

/*
 * cut a chunk of xml from the document
 */
char *xml_cut (XML *xml, char *path)
{
  XMLNODE *c, *p;
  DBUF *b;
  
  if ((c = xml_find_parent (xml, path, &p)) == NULL)
    return (NULL);
  if (p == NULL)
    xml->doc = NULL;
  else  
    p->value = NULL;
  b = dbuf_alloc ();
  xmln_format (c, b);
  xmln_free (c);
  return (dbuf_extract (b));
}

/*
 * copy a chunk of xml from the document
 */
char *xml_copy (XML *xml, char *path)
{
  XMLNODE *n;
  DBUF *b;
  
  if ((n = xml_find (xml, path)) == NULL)
    return (NULL);
  b = dbuf_alloc ();
  xmln_format (n, b);
  return (dbuf_extract (b));
}

/*
 * paste a chunk of xml into the document before or after path
 * return non-zero if fails
 */
int xml_paste (XML *xml, char *path, char *doc, int append)
{
  XMLNODE *p, *c, *n;
  
  if ((n = xmln_parse (&doc)) == NULL)
    return (-1);
  if (((c = xml_force (xml, path, &p)) == NULL) || (p == NULL))
  {
    debug ("force failed for %s\n", path);
    xmln_free (n);
    return (-1);
  }
  if (append)
  {
    for (p = n; p->next != NULL; p = p->next);
    p->next = c->next;
    c->next = n;
  }
  else
  {
    p->value = xmln_insert (p->value, c, n);
  }
  return (0);
}

/*
 * Append a snippet into the document as a child of path.  Create
 * path if needed.
 */
int xml_append (XML *xml, char *path, char *doc)
{
  XMLNODE *n, *p, *pp;
  
  if ((n = xmln_parse (&doc)) == NULL)
    return (-1);
  if ((p = xml_force (xml, path, &pp)) == NULL)
  {
    xmln_free (n);
    return (-1);
  }
  p->value = xmln_insert (p->value, NULL, n);
  return (0);
}

/*
 * Insert a snippet into the document as a child of path. Create path 
 * if needed.
 */
int xml_insert (XML *xml, char *path, char *doc)
{
  XMLNODE *n, *p, *pp;
  
  if ((n = xmln_parse (&doc)) == NULL)
    return (-1);
  if ((p = xml_force (xml, path, &pp)) == NULL)
  {
    xmln_free (n);
    return (-1);
  }
  p->value = xmln_insert (p->value, p->value, n);
  return (0);
}

/*
 * get attribute value at path
 * return NULL if not found
 */
char *xml_get_attribute (XML *xml, char *path, char *name)
{
  XMLNODE *n;

  if ((n = xml_find (xml, path)) == NULL)
    return (NULL);
  return (xmln_get_attr (n, name));
}

/*
 * set attribute value at path
 * if a new attribute append it to the list
 */
int xml_set_attribute (XML *xml, char *path, char *name, char *value)
{
  XMLNODE *a, *n;

  if ((n = xml_find (xml, path)) == NULL)
    return (-1);
  return (xmln_set_attr (n, name, value) == NULL);
}

/*
 * return count of elements matching path
 */
int xml_count (XML *xml, char *path)
{
  XMLNODE *n;
  int cnt = 0;
  char *key;

  debug ("getting count for %s\n", path);
  if (xml_find_parent (xml, path, &n) == NULL)
    return (0);
  if (n == NULL)
    return (1);
  if ((key = strrchr (path, xml->path_sep)) == NULL)
    return (0);
  key++;
  debug ("key=%s\n", key);
  for (n = n->value; n != NULL; n = n->next)
  {
    debug ("checking %s\n", n->key);
    if (strcmp (n->key, key) == 0)
      cnt++;
  }
  return (cnt);
}

/*
 * append a key to this path with given index. Return new path 
 * length or -1 if bigger than MAX_PATH
 */
int xml_pathadd (XML *xml, char *path, int index, char *key)
{
  int l = 0;
  int len = strlen (path);

  // debug ("add %s to %s for index %d\n", key, path, index);
  path += len;
  if (index)
    l = snprintf (path, MAX_PATH - len, "%c%s%c%d", xml->path_sep, key, 
      xml->indx_sep, index);
  else
    l = snprintf (path, MAX_PATH - len, "%c%s", xml->path_sep, key);
  if ((l < 1) || (l >= MAX_PATH - len))
    return (-1);
  return (len + l);
}

/*
 * return the root key tag
 */
char *xml_root (XML *xml)
{
  XMLNODE *n;
  if (xml == NULL)
    return (NULL);
  if ((n = xmln_type (xml->doc, XML_ELEMENT, 0)) == NULL)
    return (NULL);
  return (n->key);
}

/*
 * Replace path with path of the first child.  Return the path length
 * or 0 if there are no children.
 */
int xml_first (XML *xml, char *path)
{
  XMLNODE *n;
  
  if ((n = xml_find (xml, path)) == NULL)
    return (0);
  if (!xmln_haschild (n))
    return (0);
  if ((n = xmln_type (n->value, XML_ELEMENT, 0)) == NULL)
    return (0);
  // debug ("child is %s\n", n->key);
  return (xml_pathadd (xml, path, 0, n->key));
}

/*
 * Replace path with path of the last child.  Return the path length
 * or 0 if there are no children.
 */
int xml_last (XML *xml, char *path)
{
  XMLNODE *n, *e, *l;
  int index = 0;
  
  if ((n = xml_find (xml, path)) == NULL)
    return (0);
  if (!xmln_haschild (n))
    return (0);
  // find the last one
  if ((n = e = xmln_type (n->value, XML_ELEMENT, 0)) == NULL)
    return (0);
  while ((l = xmln_type (e->next, XML_ELEMENT, 0)) != NULL)
    e = l;
  // determine it's index
  while (n != e)
  {
    if (strcmp (n->key, e->key) == 0)
      index++;
    n = n->next;
  }
  return (xml_pathadd (xml, path, index, n->key));
}

/*
 * Replace path with the path of the next sibling.  Return the path
 * length or 0 if no more siblings.
 */
int xml_next (XML *xml, char *path)
{
  XMLNODE *n, *p;
  int index = 0;;
  
  if (((n = xml_find_parent (xml, path, &p)) == NULL) || (p == NULL))
    return (0);
  // debug ("looking for next\n");
  if ((n = xmln_type (n->next, XML_ELEMENT, 0)) == NULL)
    return (0);
  // determine index
  // debug ("getting index\n");
  for (p = p->value; p != n; p = p->next)
  {
    if (strcmp (p->key, n->key) == 0)
      index++;
  }
  *strrchr (path, xml->path_sep) = 0;
  return (xml_pathadd (xml, path, index, n->key));
}

/*
 * Replace path with the path of the previous sibling.  Return the path
 * length or 0 if no more siblings.
 */
int xml_prev (XML *xml, char *path)
{
  XMLNODE *n, *p, *t, *e;
  int index = 0;;
  
  if (((n = xml_find_parent (xml, path, &p)) == NULL) || (p == NULL))
    return (0);
  if ((e = xmln_type (p->value, XML_ELEMENT, 0)) == n)
    return (0);
  // determine index
  while ((t = xmln_type (e->next, XML_ELEMENT, 0)) != NULL)
  {
    if (t == n)
      break;
    e = t;
  }
  for (t = p->value; t != e; t = t->next)
  {
    if ((t->type == XML_ELEMENT) && (strcmp (t->key, e->key) == 0))
      index++;
  }
  *strrchr (path, xml->path_sep) = 0;
  return (xml_pathadd (xml, path, index, e->key));
}

/*
 * set the path separators
 */
int xml_path_opts (XML *xml, int path_sep, int indx_sep)
{
  int oldsep;

  oldsep = (xml->path_sep << 8) | xml->indx_sep;
  xml->path_sep = path_sep;
  xml->indx_sep = indx_sep;
  return (oldsep);
}


/*
 * Parse and return XML document in this buf  
 */

XML *xml_parse (char *buf)
{
  XML *xml;
  XMLNODE **node;
  char **p = &buf;

  xml = xml_alloc ();
  xml->doc = xmln_parse_doc (p);
  return (xml);
}

/*
 * coellese all text nodes in the docuement
 * return nonzero if fails
 */
int xml_normalize (XML *xml)
{
  xml->doc = xmln_normalize (xml->doc);
  return (xml->doc == NULL);
}

/*
 * beautify all the nodes in a document
 * indent sets number of spaces indent for each child tag
 * return nonzero if fails
 */
int xml_beautify (XML *xml, int indent)
{
  xml->doc = xmln_beautify (xml->doc, indent, 0);
  return (xml->doc == NULL);
}

/*
 * format a document to text
 */
char *xml_format (XML *xml)
{
  DBUF *b;

  b = dbuf_alloc ();
  xmln_format (xml->doc, b);
  /*
  if (dbuf_getbuf (&b)[dbuf_size (&b) - 1] != '\n')
    dbuf_printf (&b, "\n");
  */
  return (dbuf_extract (b));
}

/*
 * write xml to a stream
 * return number of bytes written
 */

int xml_write (XML *xml, FILE *fp)
{
  return (xmln_write (xml->doc, fp));
}

/*
 * save a document to a file
 * return 0 if successful
 */
int xml_save (XML *xml, char *filename)
{
  return (xmln_save (xml->doc, filename));
}

/*
 * load xml document from a stream
 */
XML *xml_read (FILE *fp)
{
  XML *x;
  XMLNODE *n;

  if ((n = xmln_read (fp, 1)) == NULL)
    return (NULL);
  x = xml_alloc ();
  x->doc = n;
  return (x);
}

/*
 * load XML from a file
 */
XML *xml_load (char *filename)
{
  XML *x;
  XMLNODE *n;

  if ((n = xmln_load (filename, 1)) == NULL)
    return (NULL);
  x = xml_alloc ();
  x->doc = n;
  return (x);
}

#ifdef UNITTEST
#undef UNITTEST
#undef debug
#define __XMLTEST__
#include "dbuf.c"
#include "xmln.c"

void xmldisplay (XML *xml, DBUF *b, char *msg)
{
  xmlndisplay (xml->doc, b, msg);
}

void dive (XML *xml, DBUF *b, char *path, int level)
{
  char p[MAX_PATH], c[MAX_PATH];

  // debug ("path=%s level=%d\n", path, level);
  strcpy (p, path);
  do
  {
    dbuf_printf (b, "%*s%s\n", level * 2, "", p);
    strcpy (c, p);
    if (xml_first (xml, c) > 0)
      dive (xml, b, c, level + 1);
  } while (xml_next (xml, p) > 0);
 }

void rdive (XML *xml, DBUF *b, char *path, int level)
{
  char p[MAX_PATH], c[MAX_PATH];
  strcpy (p, path);
  do
  {
    strcpy (c, p);
    if (xml_last (xml, c) > 0)
      rdive (xml, b, c, level + 1);
    dbuf_printf (b, "%*s%s\n", level * 2, "", p);
  } while (xml_prev (xml, p) > 0);
}

char *ConstructedXML =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<foo>\n"
"  <goofy>really goofy</goofy>\n"
"  <bar>\n"
"    <stuff/>\n"
"    <stuff crud=\"junk\"/>\n"
"    <stuff>third stuff</stuff>\n"
"  </bar>\n"
"  <phue>\n"
"    <goof>phuey</goof>\n"
"  </phue>\n"
"  <bar some:attr=\"a different value\">\n"
"    <junk>this is junk</junk>\n"
"    <crap/>\n"
"    <junk>second junk</junk>\n"
"  </bar>\n"
"</foo>";

char *DiveXML =
"foo\n"
"  foo.goofy\n"
"  foo.bar\n"
"    foo.bar.stuff\n"
"    foo.bar.stuff[1\n"
"    foo.bar.stuff[2\n"
"  foo.phue\n"
"    foo.phue.goof\n"
"  foo.bar[1\n"
"    foo.bar[1.junk\n"
"    foo.bar[1.crap\n"
"    foo.bar[1.junk[1\n";

char *RDiveXML =
"    foo.bar[1.junk[1\n"
"    foo.bar[1.crap\n"
"    foo.bar[1.junk\n"
"  foo.bar[1\n"
"    foo.phue.goof\n"
"  foo.phue\n"
"    foo.bar.stuff[2\n"
"    foo.bar.stuff[1\n"
"    foo.bar.stuff\n"
"  foo.bar\n"
"  foo.goofy\n"
"foo\n";

int main (int argc, char **argv)
{
  XMLNODE *n;
  XML *x;
  int sz, bsz;
  DBUF *b;
  char *ch, *ch2;
  
  b = dbuf_alloc ();
  // x = xml_alloc ();
  // goto scratch;
  /* load test */
  x = xml_parse (TestXML);
  xmldisplay (x, b, "load test");
  xmldiff ("load test doesn't match", b, NormalizedXML);  
  
  /* normalization test */
  x->doc = xmln_normalize (x->doc);
  xmldisplay (x, b, "normalized");
  xmldiff ("normalized test doesn't match", b, NormalizedXML);

  /* beautify test */
  x->doc = xmln_beautify (x->doc, 2, 0);
  xmldisplay (x, b, "beautified");
  xmldiff ("beautified test doesn't match", b, BeautifiedXML);

scratch:

  /* build from scratch test */
  debug ("scratch build...\n");
  x->doc = xmln_free (x->doc);
  xml_path_opts (x, '.', '[');
  xml_set (x, "foo", "<goofy>really goofy</goofy>");
  xml_set (x, "foo.bar.stuff[2]", "third stuff");
  xml_set (x, "foo.phue", "<goof>phuey</goof>");
  xml_set_attribute (x, "foo.bar.stuff[1]", "crud", "junk");
  xml_paste (x, "foo.bar[1].crap", "<junk>this is junk</junk>", 0);
  xml_paste (x, "foo.bar[1].crap", "<junk>second junk</junk>", 1);
  debug ("added all crap\n");
  ch = xml_get (x, "foo.bar.stuff[2]");
  debug ("got %s\n", ch);
  if (strcmp (ch, "third stuff"))
    printf ("failed lookup - got '%s'\n", ch);
  ch2 = xml_get_text (x, "foo.bar.stuff[2]");
  debug ("got %s\n", ch2);
  if (strcmp (ch, ch2))
    printf ("failed lookup (NULL buf) - got '%s'\n", ch2);
  free (ch);
  ch = xml_get (x, "foo.bar[1].junk[1]");
  if (strcmp (ch, "second junk"))
    printf ("failed lookup\n");
  free (ch);
  xml_set_attribute (x, "foo.bar[1]", "some:attr", "the attr value");
  ch = xml_get_attribute (x, "foo.bar[1]", "some:attr");
  if (strcmp (ch, "the attr value"))
    printf ("attribute lookup failed\n");
  xml_set_attribute (x, "foo.bar[1]", "some:attr", "a different value");
  ch = xml_get_attribute (x, "foo.bar[1]", "some:attr");
  if (strcmp (ch, "a different value"))
    printf ("attribute lookup failed\n");
  debug ("beautifying...\n");
  x->doc = xmln_beautify (x->doc, 2, 0);
  debug ("setting declaration...\n");
  xml_declare (x);
  debug ("displaying...\n");
  xmldisplay (x, b, "constructed");
  xmldiff ("constructed test doesn't match", b, ConstructedXML);
  debug ("diving...\n");
  dbuf_clear (b);
  dive (x, b, xml_root (x), 0);
  xmldiff ("dive test doesn't match", b, DiveXML);
  debug ("reverse diving...\n");
  dbuf_clear (b);
  rdive (x, b, xml_root (x), 0);
  xmldiff ("reverse dive test doesn't match", b, RDiveXML);
  dbuf_free (b);
  xml_free (x);
  info ("%s %s\n", argv[0], Errors?"failed":"passed");
  exit (Errors);
}

#endif /* UNITTEST */
