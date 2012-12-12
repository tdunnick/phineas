/*
 * xmln.c
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

#ifdef UNITTEST
#include "unittest.h"
#define __XMLTEST__
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>


#include "dbuf.h"
#include "xmln.h"

#ifndef debug
#define debug(fmt...)
#endif

static char *XmlDecl = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
static char *Ent_codes [] = { "lt;", "gt;", "amp;", "apos;", "quot;" };
static char Ent_chars [] = "<>&'\"";

/********************* data manipulations **************************/

/*
 * convert text to xml entities
 */
char *xml_to_entity (char *dst, char *s)
{
  char *d,
       *ent;

  d = dst;
  while (*d = *s++)
  {
    if ((ent = strchr (Ent_chars, *d)) != NULL)
    {
      *d++ = '&';
      ent = Ent_codes[ent - Ent_chars];
      strcpy (d, ent);
      while (*++d);
    }
    else
      d++;
  }
  return (dst);
}

/*
 * convert xml entities back to text
 */
char *xml_from_entity (char *dst, char *s)
{
  char *d;
  int i, l;
  
  d = dst;
  while (*d = *s++)
  {
    if (*d == '&')
    {
      for (i = 0; i < sizeof (Ent_chars); i++)
      {
        l = strlen (Ent_codes[i]);
	if (strncmp (s, Ent_codes[i], l) == 0)
	{
	  *d = Ent_chars[i];
	  s += l;
	  break;
	}
      }
    }
    d++;
  }
  return (dst);
}

/* 
 * remove leading and trailing white space from a string
 * return the resulting string
 */
char *xml_trim (char *value)
{
  char *ch;

  for (ch = value + strlen (value); ch > value; ch--)
    if (!isspace (*(ch-1))) break;
  *ch = 0;
  for (ch = value; isspace (*ch); ch++);
  if (ch > value)
    strcpy (value, ch);
  return (value);
}

/************************* allocation ***************************/

/* 
 * free an xml node (subtree)
 */
XMLNODE *xmln_free (XMLNODE *node)
{
  XMLNODE *n;
  while ((n = node) != NULL)
  {
    node = n->next;
    free (n->key);
    if (n->value != NULL)
    {
      if ((n->type == XML_TEXT) || (n->type == XML_ATTRIB))
        free (n->value);
      else
        xmln_free (n->value);
    }
    xmln_free (n->attributes);
    free (n);
  }
  return (NULL);
}

/* 
 * allocate an xml node setting it's type and copying the key (tag)
 * until an EOS, space, '=', '>', or '/>' is found.
 */
XMLNODE *xmln_alloc (int type, char *key)
{
  int len = 0, c;
  XMLNODE *n;
  
  while ((c = key[len]) && !isspace (c))
  {
    if (c == '=')
      break;
    if (c == '>')
    {
      if (key[len-1] == '/')
        len--;
      break;
    }
    len++;
  }
  n = (XMLNODE *) malloc (sizeof (XMLNODE));
  n->key = (char *) malloc (len + 1);
  n->value = NULL;
  n->next = NULL;
  n->attributes = NULL;
  n->type = type;
  if (len)
    strncpy (n->key, key, len);
  n->key[len] = 0;
  debug ("allocated node type=%c key='%s'\n", n->type, n->key);
  return (n);
}

/********************** node manipulations ************************/

/* 
 * remove leading and trailing white space from a value
 * return the resulting value length
 */
int xmln_trim (XMLNODE *node)
{
  if (xmln_isparent (node))
    return (-1);
  return (strlen (xml_trim (node->value)));
}

/* 
 * Set a text or attribute node value with size len.  This reallocates
 * value. return value or NULL if fails
 */
char *xmln_set_val (XMLNODE *node, char *value, int len)
{
  char *ch;

  if ((len < 0) || xmln_isparent (node))
    return (NULL);
  node->value = realloc (node->value, len + 1);
  ch = node->value;
  if (len)
    strncpy (ch, value, len);
  ch[len] = 0;
}

/*
 * return a nodes value if a leaf, otherwise NULL
 */
char *xmln_get_val (XMLNODE *n)
{
  if (!xmln_isparent (n))
    return (n->value);
  return (NULL);
}

/*
 * set attribute value at node
 * if a new attribute append it to the list
 */
XMLNODE *xmln_set_attr (XMLNODE *n, char *name, char *value)
{
  XMLNODE *a;

  if ((a = xmln_key (n->attributes, name, 0)) == NULL)
  {
    debug ("creating attribute %s\n", name);
    a = xmln_text_alloc (" ", 1);
    a->next = xmln_alloc (XML_ATTRIB, name);
    n->attributes = xmln_insert (n->attributes, NULL, a);
    a = a->next;
  }
  xmln_set_val (a, value, strlen (value));
  return (a);
}

/*
 * get attribute value a node for name
 * return NULL if not found
 */
char *xmln_get_attr (XMLNODE *n, char *name)
{
  if ((n = xmln_key (n->attributes, name, 0)) == NULL)
    return (NULL);
  return (n->value);
}

/*
 * allocate a text node
 */
XMLNODE *xmln_text_alloc (char *value, int len)
{
  XMLNODE *n = xmln_alloc (XML_TEXT, "");
  if (len <= 0)
    len = strlen (value);
  xmln_set_val (n, value, len);
  return (n);
}

/*
 * insert a node (list) into a chain at specified place
 * if place is NULL append to the chain
 */
XMLNODE *xmln_insert (XMLNODE *chain, XMLNODE *place, XMLNODE *n)
{
  XMLNODE *e, **p = &chain;

  while (*p != place)
  {
    if (*p == NULL)
      return (NULL);
    p = &(*p)->next;
  }
  *p = n;
  while (n->next != NULL)
    n = n->next;
  n->next = place;
  return (chain);
}

/*
 * delete a node in this chain
 */
XMLNODE *xmln_delete (XMLNODE *chain, XMLNODE *place)
{
  XMLNODE **p = &chain;

  while (*p != place)
  {
    if (*p == NULL)
      return (NULL);
    p = &(*p)->next;
  }
  *p = place->next;
  place->next = NULL;
  xmln_free (place);
  return (chain);
}

/*
 * recursively copy a branch of the document tree
 */
XMLNODE *xmln_copy (XMLNODE *node)
{
  XMLNODE *n, **na, *a;

  if (node == NULL)
    return (NULL);
  n = xmln_alloc (node->type, node->key);
  na = &n->attributes;
  for (a = node->attributes; a != NULL; a = a->next)
  {
    *na = xmln_copy (a);
    na = &(*na)->next;
  }
  *na = NULL;
  if (node->value != NULL)
  {
    if ((n->type == XML_TEXT) || (n->type == XML_ATTRIB))
      xmln_set_val (n, node->value, strlen (node->value));
    else
      n->value = xmln_copy (node->value);
  }
  return (n);
}

/*
 * return number of sibling nodes
 */
int xmln_len (XMLNODE *node)
{
  int n = 0;
  while (node != NULL)
  {
    n++;
    node = node->next;
  }
  return (n);
}

/*
 * return true if this node can have child nodes for it's value
 */
int xmln_isparent (XMLNODE *node)
{
  if ((node->type != XML_ELEMENT) && (node->type != XML_COMMENT))
    return (0);
  return (1);
}

/*
 * return true if this node has a child node
 */
int xmln_haschild (XMLNODE *node)
{
  int cnt = 0;
  XMLNODE *n;

  if (!xmln_isparent (node))
    return (0);
  return (node->value != NULL);
}

/*
 * search a node list for matching name (key) and index (from 0)
 * return the node or NULL if not found
 */
XMLNODE *xmln_key (XMLNODE *node, char *name, int index)
{
  int i = 0;

  while (node != NULL)
  {
    if ((strcmp (name, node->key) == 0) && (i++ >= index))
      return (node);
    node = node->next;
  }
  return (NULL);
}

/*
 * search a node list for a matching type and index (from 0)
 * return the node or NULL if not found
 */
XMLNODE *xmln_type (XMLNODE *node, int type, int index)
{
  int i = 0;

  while (node != NULL)
  {
    if ((node->type == type) && (i++ >= index))
      return (node);
    node = node->next;
  }
  return (NULL);
}

/********************* XML parsing *******************************/

/*
 * parse attributes
 * ATTRIB :: SPACE ATTRIB
 *        :: TEXT '="' TEXT '"'
 *        :: NULL
 * return attribute node list
 */
XMLNODE *xmln_parse_attr (char *buf)
{
  XMLNODE *n = NULL, **node;
  char *p = buf;

  node = &n;
  while (*p && (*p != '>'))
  {
    if (isspace (*buf))			/* getting text node		*/
    {
      if (!isspace (*p))
      {
        debug ("setting attribute text node\n");
        *node = xmln_text_alloc (buf, p - buf);
	node = &(*node)->next;
	buf = p;
      }
    }
    else if (*buf == '"')		/* getting value	*/
    {
      if (*p == '"')			/* got it		*/
      {
        debug ("setting attribute value %.*s\n", p - buf + 1, buf);
        xmln_set_val (*node, buf + 1, p - buf - 1);
	node = &(*node)->next;
	buf = p + 1;
      }
    }
    else if (isspace (*p) || (*p == '='))/* got key		*/
    {
      debug ("setting attribute key\n");
      *node = xmln_alloc (XML_ATTRIB, buf);
      buf = p;
      while (isspace (*p)) p++;
      if (*p++ != '=')
      {
	p--;
        debug ("Missing assignment\n");
	continue;
      }
      while (isspace (*p)) p++;
      if (*p != '"')
      {
	debug ("Missing quote\n");
	continue;
      }
      buf = p;
    }
    p++;
  }
  if (*node != NULL)
    debug ("Missing closing quote in attribute\n");
  debug ("finished attribute parse\n");
  return (n);
}

/*
 * parse one xml element
 * ELEMENT  :: '<' TAG ATTRIB '>' XMLNODES '</' TAG '>'
 *          :: '<' TAG ATTRIB '/>'
 *          :: NULL
 * return element XMLNODE and update parse position
 */
XMLNODE *xmln_parse_elem (char **buf)
{
  XMLNODE *node;
  char *la, *ra;

  debug ("parsing element\n");
  la = *buf;
  if ((*la != '<') || !isalpha (la[1]) || ((ra = strchr (la, '>')) == NULL))
  {
    debug ("not an element\n");
    return (NULL);
  }

  node = xmln_alloc (XML_ELEMENT, la + 1);
  la += strlen (node->key) + 1;
  node->attributes = xmln_parse_attr (la);
  if (ra[-1] != '/')			/* not self closed key	*/
  {
    la = ra + 1;			/* gather up children	*/
    node->value = xmln_parse (&la);
  					/* expect closing key	*/
    if ((*la != '<') || (la[1] != '/') ||
     ((ra = strchr (la, '>')) == NULL) ||
     strncmp (la + 2, node->key, strlen (node->key)))
    {
      debug ("closing key not found for %s\n", node->key);
      xmln_free (node);
      return (NULL);
    }
  }
  debug ("returning element\n");
  *buf = ra + 1;
  return (node);
}

/* parse decls
 * DECLS    :: '<' NONALPHA TEXT '>' DECLS
 *          :: COMMENT DECLS
 *          :: TEXT DECLS
 *          :: NULL
 * return list of decl nodes parsed and update parse position
 */
XMLNODE *xmln_parse_decls (char **buf)
{
  XMLNODE *n = NULL, **node = &n;
  char *la = *buf, *ra;

  debug ("parsing decls\n");
  while ((la != NULL) && *la)
  {
    if (*la != '<')			/* text node		*/
    {
      ra = la;
      if ((la = strchr (ra, '<')) == NULL)
        la = ra + strlen (ra);
      *node = xmln_text_alloc (ra, la - ra);
      debug ("parsed text\n");
    }
    else if ((ra = strchr (la, '>')) == NULL)
    {
      debug ("no closing '>'\n");
      break;
    }
    else if (isalpha (la[1]) || (la[1] == '/'))
    {
      debug ("closing tag\n");
      break;
    }
    else if (strncmp (la, "<!--", 4) == 0)
    {
      *node = xmln_alloc (XML_COMMENT, "");
      la += 4;
      if ((ra = strstr (la, "-->")) == NULL)
      {
        debug ("Missing closing comment");
      }
      else
      {
        *ra = 0;
        debug ("comment %s\n", la);
	(*node)->value = xmln_parse (&la);
	*ra = '-';
	la = ra + 3;
      }
    }
    else
    {
      *node = xmln_alloc (la[1], la + 2);
      if ((la += 2 + strlen ((*node)->key)) < ra)
        (*node)->attributes = xmln_parse_attr (la);
      la = ra + 1;
      debug ("parsed decl %s\n", (*node)->key);
    }
    node = &(*node)->next;
  }
  debug ("parsed decls to %s\n", la);
  debug ("returning %sdecls\n", n == NULL ? "no " : "");
  *node = NULL;
  *buf = la;
  return (n);
}

/*
 * parse a node list
 * XMLNODES    :: DECLS XMLNODES
 *          :: ELEMENT XMLNODES
 *          :: NULL
 * return head of the node list and update parse position
 */
XMLNODE *xmln_parse (char **buf)
{
  XMLNODE *n, **node;

  debug ("parsing children\n");
  node = &n;
  while (((*node = xmln_parse_decls (buf)) !=NULL) ||
    ((*node = xmln_parse_elem (buf)) != NULL)) 
  {
    do
    {
      node = &(*node)->next;
    } while (*node != NULL);
  }
  debug ("returning %schildren\n", n == NULL ? "no " : "");
  return (n);
}

/*
 * Parse a complete document.
 *
 * DOC      :: XMLDECL DECLS ELEMENT DECLS
 * XMLDECL  :: '<?xml' ATTRIB '/>' 
 *          :: NULL
 */
XMLNODE *xmln_parse_doc (char **buf)
{
  XMLNODE *doc, **node;

  doc = NULL;
  node = &doc;
  *node = xmln_parse_decls (buf);
  while (*node != NULL) 
    node = &(*node)->next;
  debug ("parsing element at %s\n", *buf);
  if ((*node = xmln_parse_elem (buf)) != NULL)
  {
    do
    {
      node = &(*node)->next;
    } while (*node != NULL);
    debug ("parsing final decls at %s\n", *buf);
    *node = xmln_parse_decls (buf);
  }
  else
    return (xmln_free (doc));
  if (**buf)				
    return (xmln_free (doc));	
  debug ("parse completed at %s\n", *buf);
  return (doc);
}

/********************** global manipulations *********************/
/*
 * add an XML declaration if needed
 */
XMLNODE *xmln_declare (XMLNODE *n)
{
  XMLNODE *t;
  char *p;

  if (n == NULL)
    return (NULL);
  if ((n->type != '?') || strcmp (n->key, "xml")) 
  {
    p = XmlDecl;
    n = xmln_insert (n, n, xmln_parse_decls (&p));
  }
  return (n);
}

/*
 * coellese all the text nodes, removing those that are empty
 */
XMLNODE *xmln_normalize (XMLNODE *node)
{
  XMLNODE *n, *p, *t;

  n = node;
  while (n != NULL)
  {
    if (n->type == XML_TEXT)	/* past text nodes together	*/
    {
      while (((t = n->next) != NULL) && (t->type == XML_TEXT))
      {
        n->value = (char *) realloc (n->value, 
          strlen (n->value) + strlen (t->value) + 3);
        strcat (n->value, t->value);
	n->next = t->next;
	t->next = NULL;
        xmln_free (t);
      }
      if (n->value[0] == 0)	/* remove empty text node	*/
      {
	t = n;
	n = n->next;
	if (t == node)
	  node = n;
	else
	  p->next = n;
	t->next = NULL;
	xmln_free (t);
	continue;
      }
    }
    else
    {
      n->attributes = xmln_normalize (n->attributes);
      if (xmln_haschild (n))
      {
        n->value = xmln_normalize (n->value);
      }
    }
    p = n;
    n = p->next;
  }
  return (node);
}

/*
 * beautify a text node
 */
int xmln_text_beautify (XMLNODE *node, int indent, int level)
{
  int cnt, len;
  char *ch, *nl, *t;

  if ((node == NULL) || (node->type != XML_TEXT) || (node->value == NULL))
    return (0);
  debug ("beautify text %s\n", node->value);
  /* 
   * remove any leading or trailing white space 
   * if not indenting, or nothing left, we are done
   */
  ch = xml_trim (node->value);
  if ((*ch == 0) || (indent == 0))
    return (0);
  /*
   * determine how many indents we'll need by counting newlines
   */
  cnt = 2;
  while ((ch = strchr (ch, '\n')) != NULL)
  {
    cnt++;
    ch++;
  }
  if (cnt == 2)
    return (strlen (node->value));
  /*
   * add leading white space and newline for indents
   */
  t = (char *) malloc (cnt * (indent * level + 1) + strlen (node->value));
  ch = node->value;
  len = 0;
  while (ch != NULL)
  {
    if ((nl = strchr (ch, '\n')) != NULL)
      *nl = 0;
    xml_trim (ch);
    len += sprintf (t + len, "\n%*s%s", indent * level, "", ch);
    if ((ch = nl) != NULL)
      ch++;
  }
  len += sprintf (t + len, "\n%*s", indent * (level - 1), "");
  free (node->value);
  node->value = t;
  return (len);
}

/* 
 * modify text nodes for pretty indenting 
 */
XMLNODE *xmln_beautify (XMLNODE *node, int indent, int level)
{
  XMLNODE *t;
  int n;
  int l;
  char buf[80];


  if (node == NULL)
    return (NULL);
  if (level == 0)
    node = xmln_normalize (node);
  t = node;
  /*
   * if the only child and a text node, just trim it
   */
  if ((t->next == NULL) && (t->type == XML_TEXT))
  {
    xmln_text_beautify (t, indent, level);
    return (node);
  }
  if (indent)
    l = sprintf (buf, "\n%*s", indent * level, "");
  else
    buf[l = 0] = 0;
  while (t != NULL)
  {
    debug ("beautify %s type=%c\n", t->key, t->type);
    if (t->type == XML_TEXT)
    {
      if (xmln_text_beautify (t, indent, level))
        node = xmln_insert (node, t, xmln_text_alloc (buf, l));
      t = t->next;
      continue;
    }
    node = xmln_insert (node, t, xmln_text_alloc (buf, l));
    if (xmln_haschild (t))
    {
      t->value = xmln_beautify (t->value, indent, level + 1);
      if (((XMLNODE *) t->value)->next != NULL)
        xmln_insert (t->value, NULL, xmln_text_alloc (buf, l));
    }
    t = t->next;
  } 
  if (level == 0)	/* re-normalize and remove leading text	*/
  {
    t = xmln_normalize (node);
    node = t->next;
    t->next = NULL;
    xmln_free (t);
  }
  return (node);
}

/* 
 * format to text 
 */
int xmln_format (XMLNODE *node, DBUF *b)
{
  int n;

  while (node != NULL)
  {
    n = 0;
    debug ("node type=%c\n", node->type);
    switch (node->type)
    {
      case XML_ELEMENT :
	dbuf_printf (b, "<%s", node->key);
	if ((n = xmln_format (node->attributes, b)) < 0)
	  break;
	if (node->value != NULL)
	{
	  dbuf_printf (b, ">");
	  n = xmln_format (node->value, b);
	  dbuf_printf (b, "</%s>", node->key);
	}
	else 
	{
	  dbuf_printf (b, "/>");
	}
	break;
      case XML_TEXT :
        dbuf_printf (b, "%s", node->value);
	break;
      case XML_COMMENT :
        dbuf_printf (b, "<!--");
	n = xmln_format (node->value, b);
	dbuf_printf (b, "-->");
	break;
      case XML_ATTRIB :
        dbuf_printf (b, "%s=\"%s\"", node->key, node->value);
	break;
      case XML_DECL :
        dbuf_printf (b, "<%c%s", node->type, node->key);
	n = xmln_format (node->attributes, b);
	dbuf_printf (b, "%c>", node->type);
	break;
      default :
	debug ("failed at %s\n", dbuf_getbuf (b));
	n = -1;
    }
    if (n < 0)
    {
      debug ("format Error return!\n");
      return (-1);
    }
    node = node->next;
  }
  return (dbuf_size (b));
}

/************************* io ************************************/

/*
 * read xml document or snippet from a stream
 */
XMLNODE *xmln_read (FILE *fp, int doc)
{
  XMLNODE *x;
  char *buf, **p;
  int bufsz = 4096, 
      sz = 0, 
      n;

  buf = (char *) malloc (bufsz);
  while ((n = fread (buf + sz, sizeof (char), bufsz - sz, fp)) > 0)
  {
    sz += n;
    if (sz >= bufsz - 1)
    {
      bufsz <<= 2;
      buf = (char *) realloc (buf, bufsz);
    }
  }
  buf[sz] = 0;
  p = &buf;
  if (doc)
    x = xmln_parse_doc (p);
  else
    x = xmln_parse (p);
  free (buf);
  return (x);
}

/*
 * load XML document or snippet from a file
 */
XMLNODE *xmln_load (char *filename, int doc)
{
  XMLNODE *x;
  FILE *fp;

  if (filename == NULL)
    return (NULL);
  if ((fp = fopen (filename, "rb")) == NULL)
    return (NULL);
  x = xmln_read (fp, doc);
  fclose (fp);
  return (x);
}

/*
 * write xml to a stream
 * return number of bytes written
 */
int xmln_write (XMLNODE *x, FILE *fp)
{
  DBUF *b;
  int n;

  if (x == NULL)
    return (-1);
  b = dbuf_alloc ();
  n = xmln_format (x, b);
  if (fwrite (dbuf_getbuf(b), sizeof (char), n, fp) != n)
    n = -1;
  dbuf_free (b);
  return (n);
}

/*
 * save a document to a file
 * return 0 if successful
 */
int xmln_save (XMLNODE *x, char *filename)
{
  FILE *fp;
  int sz;

  if ((fp = fopen (filename, "w")) == NULL)
    return (-1);
  sz = xmln_write (x, fp);
  fclose (fp);
  if (sz < 1)
    unlink (filename);
  return (sz < 1);
}

#ifdef __XMLTEST__

xmldump_nodes (XMLNODE *node, char *b, int maxsz, int level)
{
  int sz = 0, n;

  while (node != NULL)
  {
    if ((node->type == XML_TEXT) || (node->type == XML_ATTRIB))
      n = snprintf (b, maxsz - sz, "%*s%c%s '%s'\n", level, "", 
	  node->type, node->key, node->value);
    else
    {
      n = snprintf (b, maxsz - sz, "%*s%c%s\n%*s%s\n", 
	level, "", node->type, node->key,
	level + 2, "", "attributes");
      sz += n;
      n = xmldump_nodes (node->attributes, b, maxsz - sz, level + 2);
      sz += n;
      n = snprintf (b, maxsz - sz, "%*s%s\n", level + 2, "", "children");
      sz += n;
      n = xmldump_nodes (node->value, b, maxsz - sz, level + 2);
    }
    sz += n;
    node = node->next;
  }
  if (level == 0)
  {
    debug ("\n----- dump --------\n%s\n", b);
  }
  return (sz);
}

void xmlndisplay (XMLNODE *n, DBUF *b, char *msg)
{
  if (n == NULL)
  {
    debug ("%s - NULL XMLNODE!\n", msg);
    return;
  }
  dbuf_clear (b);
  xmln_format (n, b);
  debug ("\n----- %s -----\n%s\n", msg, dbuf_getbuf(b));
}

#define xmldiff(m,b,e) strdiff(__FILE__,__LINE__,m,dbuf_getbuf(b),e)

char *TestXML =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
"<EncryptedData Id=\"ed1\" Type=\"http://www.w3.org/2001/04/xmlenc#Element\" "
"xmlns=\"http://www.w3.org/2001/04/xmlenc#\">"
"<EncryptionMethod Algorithm=\"http://www.w3.org/2001/04/xmlenc#tripledes-cbc\"/>"
"<KeyInfo xmlns=\"http://www.w3.org/2000/09/xmldsig#\">"
"<EncryptedKey xmlns=\"http://www.w3.org/2001/04/xmlenc#\">"
"<EncryptionMethod Algorithm=\"http://www.w3.org/2001/04/xmlenc#rsa-1_5\"/>"
"<KeyInfo xmlns=\"http://www.w3.org/2000/09/xmldsig#\">"
"<KeyName>key</KeyName>"
"</KeyInfo><CipherData><CipherValue/></CipherData></EncryptedKey></KeyInfo>"
"<CipherData><CipherValue/></CipherData> </EncryptedData>";

char *NormalizedXML =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?><EncryptedData Id=\"ed1\" Type=\"http://www.w3.org/2001/04/xmlenc#Element\" xmlns=\"http://www.w3.o"
"rg/2001/04/xmlenc#\"><EncryptionMethod Algorithm=\"http://www.w3.org/2001/04/xmlenc#tripledes-cbc\"/><KeyInfo xmlns=\"http://www.w3.org/"
"2000/09/xmldsig#\"><EncryptedKey xmlns=\"http://www.w3.org/2001/04/xmlenc#\"><EncryptionMethod Algorithm=\"http://www.w3.org/2001/04/xml"
"enc#rsa-1_5\"/><KeyInfo xmlns=\"http://www.w3.org/2000/09/xmldsig#\"><KeyName>key</KeyName></KeyInfo><CipherData><CipherValue/></Cipher"
"Data></EncryptedKey></KeyInfo><CipherData><CipherValue/></CipherData> </EncryptedData>";

char *BeautifiedXML =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<EncryptedData Id=\"ed1\" Type=\"http://www.w3.org/2001/04/xmlenc#Element\" xmlns=\"http://www.w3.org/2001/04/xmlenc#\">\n"
"  <EncryptionMethod Algorithm=\"http://www.w3.org/2001/04/xmlenc#tripledes-cbc\"/>\n"
"  <KeyInfo xmlns=\"http://www.w3.org/2000/09/xmldsig#\">\n"
"    <EncryptedKey xmlns=\"http://www.w3.org/2001/04/xmlenc#\">\n"
"      <EncryptionMethod Algorithm=\"http://www.w3.org/2001/04/xmlenc#rsa-1_5\"/>\n"
"      <KeyInfo xmlns=\"http://www.w3.org/2000/09/xmldsig#\">\n"
"        <KeyName>key</KeyName>\n"
"      </KeyInfo>\n"
"      <CipherData>\n"
"        <CipherValue/>\n"
"      </CipherData>\n"
"    </EncryptedKey>\n"
"  </KeyInfo>\n"
"  <CipherData>\n"
"    <CipherValue/>\n"
"  </CipherData>\n"
"</EncryptedData>";

#endif /* __XMLTEST__ */

#ifdef UNITTEST
#undef UNITTEST
#undef debug
#include "dbuf.c"

int main (int argc, char **argv)
{
  XMLNODE *n;
  int sz, bsz;
  DBUF *b;
  char *ch, *ch2, *tfile = "xmlnTest.xml";
  
  b = dbuf_alloc ();
  // x = xml_alloc ();
  // goto scratch;
  /* load test */
  ch = TestXML;
  n = xmln_parse_doc (&ch);
  xmlndisplay (n, b, "load test");
  xmldiff ("load test doesn't match", b, NormalizedXML);

  /* normalization test */
  n = xmln_normalize (n);
  xmlndisplay (n, b, "normalized");
  xmldiff ("normalized test doesn't match", b, NormalizedXML);

  /* beautify test */
  n = xmln_beautify (n, 2, 0);
  xmlndisplay (n, b, "beautified");
  xmldiff ("beautified test doesn't match", b, BeautifiedXML);

  xmln_save (n, tfile);
  xmln_free (n);
  n = xmln_load (tfile, 1);
  xmlndisplay (n, b, "loaded");
  xmldiff ("(re)loaded test doesn't match", b, BeautifiedXML);
  xmln_free (n);
  dbuf_free (b);
  unlink (tfile);
  info ("%s %s\n", argv[0], Errors?"failed":"passed");
  exit (Errors);
}

#endif /* UNITTEST */
