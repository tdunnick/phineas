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
#undef UNITTEST
#include "unittest.h"
#include "dbuf.c"
#define UNITTEST
#define debug _DEBUG_
#endif

#include "log.h"
#include "dbuf.h"
#include "xml.h"

#ifndef debug
#define debug(fmt...)
#endif

#define XBUFSZ 0x4000		/* nominal XML buffer size	*/

/*
 * internal declarations
 */

char *xml_pathkey (XML *xml, char *path, char *key, int *index);
XMLNODE *xml_node_alloc (int, char *);
XMLNODE *xml_node_free (XMLNODE *node);
XMLNODE *xml_node_copy (XMLNODE *node);
XMLNODE *xml_node_find (XMLNODE *, char *, int);
XMLNODE *xml_force (XML *xml, char *path, XMLNODE **parent);
XMLNODE *xml_parse_decls (char **);
XMLNODE *xml_parse_nodes (char **);
XMLNODE *xml_node_normalize (XMLNODE *node);
XMLNODE *xml_node_beautify (XMLNODE *node, int tabsz, int level);
void xml_set_value (XMLNODE *node, char *value, int len);

static char *XmlDecl = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
static char *Ent_codes [] = { "lt;", "gt;", "amp;", "apos;", "quot;" };
static char Ent_chars [] = "<>&'\"";

/********************* data manipulations **************************/

/*
 * convert text to xml entities
 */
char *xml_to_entity (char *dst, char *s)
{
  char *d, *ent;

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
}

/*
 * convert xml entities back to text
 */
char *xml_from_entity (char *dst, char *s)
{
  char *d;
  int i, l;
  
  while (*d == *s++)
  {
    if (*d == '&')
    {
      for (i = 0; i < sizeof (Ent_chars); i++)
      {
        l = strlen (Ent_codes[i]);
	if (strncmp (++d, Ent_codes[i], l) == 0)
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
 * free an xml document 
 */
XML *xml_free (XML *xml)
{
  if (xml == NULL)
    return (NULL);
  xml_node_free (xml->doc);
  free (xml);
  return (NULL);
}

/* 
 * free an xml (subtree) 
 */
XMLNODE *xml_node_free (XMLNODE *node)
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
        xml_node_free (n->value);
    }
    xml_node_free (n->attributes);
    free (n);
  }
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
 * allocate an xml node 
 */
XMLNODE *xml_node_alloc (int type, char *key)
{
  int len, c;
  XMLNODE *n;
  
  c = key[len = 0];
  while (!isspace (c))
  {
    if (c == '=')
      break;
    if (c == '>')
    {
      if (key[len-1] == '/')
        len--;
      break;
    }
    c = key[++len];
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
  // debug ("allocated node type=%c key='%s'\n", n->type, n->key);
  return (n);
}

/********************** node manipulations ************************/

/* 
 * remove leading and trailing white space from a value
 * return the resulting value length
 */
int xml_trimvalue (XMLNODE *node)
{
  if (xml_is_parent (node))
    return (-1);
  return (strlen (xml_trim (node->value)));
}

/* 
 * set a node value 
 */
void xml_set_value (XMLNODE *node, char *value, int len)
{
  char *ch;

  if (len < 0)
  {
    error ("negative length for value\n");
    return;
  }
  if ((node->type != XML_TEXT) && (node->type != XML_ATTRIB))
  {
    error ("not a text valued node\n");
    return;
  }
    
  if (node->value == NULL)
    node->value = malloc (len + 1);
  else
    node->value = realloc (node->value, len + 1);
  ch = node->value;
  if (len)
    strncpy (ch, value, len);
  ch[len] = 0;
  // debug ("set node type=%c value to '%s'\n", node->type, node->value);
}

/*
 * allocate a text node
 */
XMLNODE *xml_text_alloc (char *value, int len)
{
  XMLNODE *n = xml_node_alloc (XML_TEXT, "");
  if (len <= 0)
    len = strlen (value);
  xml_set_value (n, value, len);
  return (n);
}

/*
 * insert a node (list) into a chain at specified place
 * if place is NULL append to the chain
 */
XMLNODE *xml_node_insert (XMLNODE *chain, XMLNODE *place, XMLNODE *n)
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
XMLNODE *xml_node_delete (XMLNODE *chain, XMLNODE *place)
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
  xml_node_free (place);
  return (chain);
}

/*
 * recursively copy a branch of the document tree
 */
XMLNODE *xml_node_copy (XMLNODE *node)
{
  XMLNODE *n, **na, *a;

  if (node == NULL)
    return (NULL);
  n = xml_node_alloc (node->type, node->key);
  na = &n->attributes;
  for (a = node->attributes; a != NULL; a = a->next)
  {
    *na = xml_node_copy (a);
    na = &(*na)->next;
  }
  *na = NULL;
  if (node->value != NULL)
  {
    if ((n->type == XML_TEXT) || (n->type == XML_ATTRIB))
      xml_set_value (n, node->value, strlen (node->value));
    else
      n->value = xml_node_copy (node->value);
  }
  return (n);
}

/*
 * return number of sibling nodes
 */
int xml_num_nodes (XMLNODE *node)
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
 * this could be a "branch" if true, and must be a "leaf" if not
 */
int xml_is_parent (XMLNODE *node)
{
  if ((node->type != XML_ELEMENT) && (node->type != XML_COMMENT))
    return (0);
  return (1);
}

/*
 * return number of child nodes
 */
int xml_num_children (XMLNODE *node)
{
  int cnt = 0;
  XMLNODE *n;

  if (!xml_is_parent (node))
    return (0);
  for (n = node->value; n != NULL; n = n->next)
    cnt += 1 + xml_num_children (n);
  return (cnt);
}

/*
 * return the node matching this name
 */
XMLNODE *xml_node_find (XMLNODE *node, char *name, int index)
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

/********************* XML parsing *******************************/

/*
 * parse attributes
 * ATTRIB :: SPACE ATTRIB
 *        :: TEXT '="' TEXT '"'
 *        :: NULL
 * return attribute node list
 */
XMLNODE *xml_parse_attrib (char *buf)
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
        *node = xml_text_alloc (buf, p - buf);
	node = &(*node)->next;
	buf = p;
      }
    }
    else if (*buf == '"')		/* getting value	*/
    {
      if (*p == '"')			/* got it		*/
      {
        debug ("getting attribute value\n");
        xml_set_value (*node, buf + 1, p - buf - 1);
	node = &(*node)->next;
	buf = p + 1;
      }
    }
    else if (*p == '=')			/* got key		*/
    {
      debug ("setting attribute key\n");
      *node = xml_node_alloc (XML_ATTRIB, buf);
      if (*(buf = ++p) != '"')
        error ("Missing attribute opening quote\n");
    }
    p++;
  }
  if (*node != NULL)
    error ("Missing closing quote in attribute\n");
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
XMLNODE *xml_parse_element (char **buf)
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

  node = xml_node_alloc (XML_ELEMENT, la + 1);
  la += strlen (node->key) + 1;
  node->attributes = xml_parse_attrib (la);
  if (ra[-1] != '/')			/* not self closed key	*/
  {
    la = ra + 1;			/* gather up children	*/
    node->value = xml_parse_nodes (&la);
  					/* expect closing key	*/
    if ((*la != '<') || (la[1] != '/') ||
     ((ra = strchr (la, '>')) == NULL) ||
     strncmp (la + 2, node->key, strlen (node->key)))
    {
      error ("closing key not found for %s\n", node->key);
      xml_node_free (node);
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
XMLNODE *xml_parse_decls (char **buf)
{
  XMLNODE *n, **node;
  char *la, *ra;

  debug ("parsing decls\n");
  la = *buf;
  node = &n;
  while ((la != NULL) && *la)
  {
    if (*la != '<')			/* text node		*/
    {
      ra = la;
      if ((la = strchr (ra, '<')) == NULL)
        la = ra + strlen (ra);
      *node = xml_text_alloc (ra, la - ra);
      debug ("parsed text\n");
    }
    else if ((ra = strchr (la, '>')) == NULL)
    {
      break;
    }
    else if (isalpha (la[1]) || (la[1] == '/'))
    {
      break;
    }
    else if (strncmp (la, "<!--", 4) == 0)
    {
      *node = xml_node_alloc (XML_COMMENT, "");
      la += 4;
      if ((ra = strstr (la, "-->")) == NULL)
        error ("Missing closing comment");
      else
      {
        *ra = 0;
	(*node)->value = xml_parse_nodes (&la);
	*ra = '-';
	la = ra + 3;
      }
    }
    else
    {
      *node = xml_node_alloc (la[1], la + 2);
      for (la += 2; la < ra; la++)
      {
        if (isspace (*la))
	{
	  (*node)->attributes = xml_parse_attrib (la);
	  break;
	}
      }
      la = ra + 1;
      debug ("parsed decl %s\n", (*node)->key);
    }
    node = &(*node)->next;
  }
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
XMLNODE *xml_parse_nodes (char **buf)
{
  XMLNODE *n, **node;

  debug ("parsing children\n");
  node = &n;
  while (((*node = xml_parse_decls (buf)) !=NULL) ||
    ((*node = xml_parse_element (buf)) != NULL)) 
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
 * coellese all the text nodes, removing those that are empty
 */
XMLNODE *xml_node_normalize (XMLNODE *node)
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
        xml_node_free (t);
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
	xml_node_free (t);
	continue;
      }
    }
    else
    {
      n->attributes = xml_node_normalize (n->attributes);
      if (xml_num_children (n) > 0)
      {
        n->value = xml_node_normalize (n->value);
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
int xml_text_beautify (XMLNODE *node, int indent, int level)
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
XMLNODE *xml_node_beautify (XMLNODE *node, int indent, int level)
{
  XMLNODE *t;
  int n;
  int l;
  char buf[80];


  if (node == NULL)
    return (NULL);
  if (level == 0)
    node = xml_node_normalize (node);
  t = node;
  /*
   * if the only child and a text node, just trim it
   */
  if ((t->next == NULL) && (t->type == XML_TEXT))
  {
    xml_text_beautify (t, indent, level);
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
      if (xml_text_beautify (t, indent, level))
        node = xml_node_insert (node, t, xml_text_alloc (buf, l));
      t = t->next;
      continue;
    }
    node = xml_node_insert (node, t, xml_text_alloc (buf, l));
    if ((n = xml_num_children (t)) > 0)
    {
      t->value = xml_node_beautify (t->value, indent, level + 1);
      if (n > 1)
        xml_node_insert (t->value, NULL, xml_text_alloc (buf, l));
    }
    t = t->next;
  } 
  if (level == 0)	/* re-normalize and remove leading text	*/
  {
    t = xml_node_normalize (node);
    node = t->next;
    t->next = NULL;
    xml_node_free (t);
  }
  return (node);
}

/* 
 * format to text 
 */
int xml_node_format (XMLNODE *node, DBUF *b)
{
  int n;

  while (node != NULL)
  {
    n = 0;
    switch (node->type)
    {
      case XML_ELEMENT :
	dbuf_printf (b, "<%s", node->key);
	if ((n = xml_node_format (node->attributes, b)) < 0)
	  break;
	if (node->value != NULL)
	{
	  dbuf_printf (b, ">");
	  n = xml_node_format (node->value, b);
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
	n = xml_node_format (node->value, b);
	dbuf_printf (b, "-->");
	break;
      case XML_ATTRIB :
        dbuf_printf (b, "%s=\"%s\"", node->key, node->value);
	break;
      case XML_DECL :
        dbuf_printf (b, "<%c%s", node->type, node->key);
	n = xml_node_format (node->attributes, b);
	dbuf_printf (b, "%c>", node->type);
	break;
      default :
        error ("Unknown node '%c' at...\n%s\n", node->type, node->key);
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

/*********************** document level **************************/

/*
 * add an XML declaration if needed
 */
XMLNODE *xml_declare (XML *xml)
{
  XMLNODE *n;
  char *p;

  if (xml == NULL)
    return (NULL);
  if ((xml->doc == NULL) || 
    (xml->doc->type != '?') ||
    strcmp (xml->doc->key, "xml")) 
  {
    p = XmlDecl;
    xml->doc = xml_node_insert (xml->doc, xml->doc, xml_parse_decls (&p));
  }
  return (xml->doc);
}

/*
 * return the root node
 */
XMLNODE *xml_root (XML *xml)
{
  XMLNODE *n = xml->doc;

  while (n != NULL)
  {
    if (n->type == XML_ELEMENT)
      return (n);
    n = n->next;
  }
  return (NULL);
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
      n = xml_node_find (n, key, i);
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
  p = &xml->doc;
  if (parent != NULL)
    *parent = NULL;
  n = NULL;
  while (1)
  {
    path = xml_pathkey (xml, path, key, &i);
    if (*key == 0)
      break;
    while ((n = xml_node_find (*p, key, i)) == NULL)
    {
      if ((p == &xml->doc) && (xml_root (xml) != NULL))
      {
        debug ("trying to force new root!\n");
        return (NULL);
      }
      n = xml_node_alloc (XML_ELEMENT, key);
      *p = xml_node_insert (*p, NULL, n);
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
 * Retrieve first text value from a node
 */
char *xml_get_text (XML *xml, char *path)
{
  XMLNODE *n = xml_find (xml, path);
  if ((n != NULL) && xml_is_parent (n))
  {
    for (n = (XMLNODE *) n->value; n != NULL; n = n->next)
    {
      if (n->type == XML_TEXT)
       return (n->value);
    }
  }
  return ("");
}

/*
 * Retrieve text with a printf formatted path
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
  
  n = xml_find (xml, path);
  b = dbuf_alloc ();
  xml_node_format (n->value, b);
  return (dbuf_extract (b));
}

/*
 * force a text value to a node
 */
XMLNODE *xml_set_text (XML *xml, char *path, char *text)
{
  XMLNODE *n, *p;
  n = xml_force (xml, path, &p);
  xml_node_free (n->value);
  n->value = xml_text_alloc (text, strlen (text));
  return (n);
}

/*
 * force a text value to a node with a printf style path
 */
XMLNODE *xml_setf (XML *xml, char *text, char *fmt, ...)
{
  va_list ap;
  char path[MAX_PATH];

  va_start (ap, fmt);
  vsprintf (path, fmt, ap);
  va_end (ap);
  return (xml_set_text (xml, path, text));
}

/*
 * set the value of path
 */
XMLNODE *xml_set (XML *xml, char *path, char *doc)
{
  XMLNODE *p, *n;

  if ((n = xml_force (xml, path, &p)) == NULL)
    return (NULL);
  xml_node_free (n->value);
  n->value = xml_parse_nodes (&doc);
  return (n);
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
    xml->doc = xml_node_free (xml->doc);
  else  
    p->value = xml_node_delete (p->value, c);
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
    c->attributes = xml_node_free (c->attributes);
  else if ((p = xml_node_find (c->attributes, attrib, 0)) == NULL)
    return (-1);
  else
    c->attributes = xml_node_delete (c->attributes, p);
  return (0);
}

/*
 * cut a chunk of xml from the document
 */
XMLNODE *xml_cut (XML *xml, char *path)
{
  XMLNODE *c, *p;
  
  if ((c = xml_find_parent (xml, path, &p)) == NULL)
    return (NULL);
  if (p == NULL)
    xml->doc = NULL;
  else  
    p->value = NULL;
  return (c);
}

/*
 * copy a chunk of xml from the document
 */
XMLNODE *xml_copy (XML *xml, char *path)
{
  return (xml_node_copy (xml_find (xml, path)));
}

/*
 * paste a chunk of xml into the document before or after path
 */
XMLNODE *xml_paste (XML *xml, char *path, XMLNODE *n, int append)
{
  XMLNODE *p, *c;
  
  if (((c = xml_force (xml, path, &p)) == NULL) || (p == NULL))
  {
    debug ("force failed for %s\n", path);
    return (NULL);
  }
  if (append)
  {
    n->next = c->next;
    c->next = n;
  }
  else
  {
    p->value = xml_node_insert (p->value, c, n);
  }
  return (n);
}

/*
 * Add an snippet into the document just before or after path
 * with the given name.  Path can NOT be the root.
 */
XMLNODE *xml_add (XML *xml, char *path, char *doc, int append)
{
  XMLNODE *n;
  
  n = xml_parse_nodes (&doc);
  if (xml_paste (xml, path, n, append) == NULL)
  {
    debug ("paste failed\n");
    n = xml_node_free (n);
  }
  return (n);
}

/*
 * Append a snippet into the document as a child of path.  Create
 * path if needed.
 */
XMLNODE *xml_append (XML *xml, char *path, char *doc)
{
  XMLNODE *n, *p, *pp;
  
  n = xml_parse_nodes (&doc);
  if ((p = xml_force (xml, path, &pp)) == NULL)
    return (NULL);
  p->value = xml_node_insert (p->value, NULL, n);
  return (n);
}

/*
 * Insert a snippet into the document as a child of path. Create path 
 * if needed.
 */
XMLNODE *xml_insert (XML *xml, char *path, char *doc)
{
  XMLNODE *n, *p, *pp;
  
  n = xml_parse_nodes (&doc);
  if ((p = xml_force (xml, path, &pp)) == NULL)
    return (NULL);
  p->value = xml_node_insert (p->value, p->value, n);
  return (n);
}

/*
 * get attribute value at path
 */
char *xml_get_attribute (XML *xml, char *path, char *name)
{
  XMLNODE *n = xml_find (xml, path);
  if (n == NULL)
    return (NULL);
  for (n = n->attributes; n != NULL; n = n->next)
  {
    if ((n->type == XML_ATTRIB) && (strcmp (n->key, name) == 0))
      return (n->value);
  }
  return (NULL);
}

/*
 * set attribute value at path
 */
XMLNODE *xml_set_attribute (XML *xml, char *path, char *name, char *value)
{
  XMLNODE *a, *n;

  if ((n = xml_find (xml, path)) == NULL)
    return (NULL);
  debug ("looking for attribute %s\n", name);
  for (a = n->attributes; a != NULL; a = a->next)
  {
    if ((a->type == XML_ATTRIB) && (strcmp (a->key, name) == 0))
      break;
  }
  if (a == NULL)
  {
    debug ("creating attribute %s\n", name);
    a = xml_text_alloc (" ", 1);
    a->next = xml_node_alloc (XML_ATTRIB, name);
    n->attributes = xml_node_insert (n->attributes, NULL, a);
    a = a->next;
  }
  xml_set_value (a, value, strlen (value));
  return (a);
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
 * Replace path with path of the first child.  Return the path length
 * or 0 if there are no children.
 */
int xml_first (XML *xml, char *path)
{
  XMLNODE *n;
  
  if ((n = xml_find (xml, path)) == NULL)
    return (0);
  if (xml_num_children (n) < 1)
    return (0);
  n = n->value;
  // debug ("looking for first child to %s\n", path);
  while (n->type != XML_ELEMENT)
  {
    if ((n = n->next) == NULL)
      return (0);
  }
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
  if (xml_num_children (n) == 0)
    return (0);
  // find the last one
  e = l = n = n->value;
  while (l->next != NULL)
  {
    l = l->next;
    if (l->type == XML_ELEMENT)
      e = l;
  }
  if (e->type != XML_ELEMENT)
    return (0);
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
  while ((n = n->next) != NULL)
  {
    if (n->type == XML_ELEMENT)
      break;
  }
  if (n == NULL)
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
  e = NULL;
  // determine index
  for (t = p->value; t != n; t = t->next)
  {
    if (t->type == XML_ELEMENT)
      e = t;
  }
  if (e == NULL)
    return (0);
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
 * Parse XML in this buf.  Return the (sub) tree parsed and buf
 * set to where the parse ended.
 *
 * XML      :: XMLDECL DECLS ELEMENT DECLS
 * ELEMENT  :: '<' TAG ATTRIB '>' XMLNODES '</' TAG '>'
 *          :: '<' TAG ATTRIB '/>'
 *          :: NULL
 * XMLNODES    :: DECLS XMLNODES
 *          :: ELEMENT XMLNODES
 *          :: NULL
 * ATTRIB   :: TEXT '="' TEXT '"' ATTRIB
 *          :: NULL
 * DECLS    :: '<' NONALPHA TEXT '>' DECLS
 *          :: COMMENT DECLS
 *          :: TEXT DECLS
 *          :: NULL
 * COMMENT  :: '<!--' TEXT '-->'
 *          :: NULL
 * XMLDECL  :: '<?xml version="1.0" encoding="UTF-8"?>'
 *          :: NULL
 */

XML *xml_parse (char *buf)
{
  XML *xml;
  XMLNODE **node;
  char **p = &buf;

  xml = xml_alloc ('.', '[');
  node = &(xml->doc);
  *node = xml_parse_decls (p);
  while (*node != NULL) 
    node = &(*node)->next;
  if ((*node = xml_parse_element (p)) != NULL)
  {
    while (*node != NULL) 
      node = &(*node)->next;
    *node = xml_parse_decls (p);
  }
  debug ("parse complete\n");
  if (**p)				/* incomplete parse	*/
    return (xml_free (xml));		/* indicates failure	*/
  return (xml);
}

/*
 * coellese all text nodes in the docuement
 */
XMLNODE *xml_normalize (XML *xml)
{
  xml->doc = xml_node_normalize (xml->doc);
  return (xml->doc);
}

/*
 * beautify all the nodes in a document
 */
XMLNODE *xml_beautify (XML *xml, int indent)
{
  xml->doc = xml_node_beautify (xml->doc, indent, 0);
  return (xml->doc);
}

/*
 * format a document
 */
char *xml_format (XML *xml)
{
  DBUF *b;

  b = dbuf_alloc ();
  xml_node_format (xml->doc, b);
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
  DBUF *b;
  int n;

  if (xml == NULL)
  {
    error ("NULL xml\n");
    return (-1);
  } 
  b = dbuf_alloc ();
  xml_declare (xml);
  n = xml_node_format (xml->doc, b);
  if (fwrite (dbuf_getbuf(b), sizeof (char), n, fp) != n)
    n = -1;
  dbuf_free (b);
  return (n);
}

/*
 * save a document to a file
 * return 0 if successful
 */
int xml_save (XML *xml, char *filename)
{
  FILE *fp;
  int sz;

  if ((fp = fopen (filename, "w")) == NULL)
    return (-1);
  sz = xml_write (xml, fp);
  fclose (fp);
  if (sz < 1)
    unlink (filename);
  return (sz < 1);
}

/*
 * load xml from a stream
 */
XML *xml_read (FILE *fp)
{
  XML *x;
  char *buf;
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
  x = xml_parse (buf);
  free (buf);
  return (x);
}

/*
 * load XML from a file
 */
XML *xml_load (char *filename)
{
  XML *x;
  FILE *fp;

  if (filename == NULL)
    return (NULL);
  if ((fp = fopen (filename, "rb")) == NULL)
    return (NULL);
  x = xml_read (fp);
  fclose (fp);
  return (x);
}

#ifdef UNITTEST

dump_nodes (XMLNODE *node, char *b, int maxsz, int level)
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
      n = dump_nodes (node->attributes, b, maxsz - sz, level + 2);
      sz += n;
      n = snprintf (b, maxsz - sz, "%*s%s\n", level + 2, "", "children");
      sz += n;
      n = dump_nodes (node->value, b, maxsz - sz, level + 2);
    }
    sz += n;
    node = node->next;
  }
  if (level == 0)
  {
    printf ("\n----- dump --------\n%s\n", b);
  }
  return (sz);
}

void display (XML *x, DBUF *b, char *msg)
{
  dbuf_clear (b);
  xml_node_format (x->doc, b);
  printf ("\n----- %s -----\n%s\n", msg, dbuf_getbuf(b));
}

void dive (XML *xml, char *path, int level)
{
  char p[MAX_PATH], c[MAX_PATH];

  // debug ("path=%s level=%d\n", path, level);
  strcpy (p, path);
  do
  {
    printf ("%*s%s\n", level * 2, "", p);
    strcpy (c, p);
    if (xml_first (xml, c) > 0)
      dive (xml, c, level + 1);
  } while (xml_next (xml, p) > 0);
 }

void rdive (XML *xml, char *path, int level)
{
  char p[MAX_PATH], c[MAX_PATH];
  strcpy (p, path);
  do
  {
    strcpy (c, p);
    if (xml_last (xml, c) > 0)
      rdive (xml, c, level + 1);
    printf ("%*s%s\n", level * 2, "", p);
  } while (xml_prev (xml, p) > 0);
}

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
  x = xml_parse (PhineasConfig);
  display (x, b, "load test");

  /* normalization test */
  x->doc = xml_node_normalize (x->doc);
  display (x, b, "normalized");

  /* beautify test */
  x->doc = xml_node_beautify (x->doc, 2, 0);
  display (x, b, "beautified");

scratch:

  /* build from scratch test */
  debug ("scratch build...\n");
  x->doc = xml_node_free (x->doc);
  xml_path_opts (x, '.', '[');
  xml_set (x, "foo", "<goofy>really goofy</goofy>");
  xml_set (x, "foo.bar.stuff[2]", "third stuff");
  xml_set (x, "foo.phue", "<goof>phuey</goof>");
  xml_set_attribute (x, "foo.bar.stuff[1]", "crud", "junk");
  xml_add (x, "foo.bar[1].crap", "<junk>this is junk</junk>", 0);
  xml_add (x, "foo.bar[1].crap", "<junk>second junk</junk>", 1);
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
  x->doc = xml_node_beautify (x->doc, 2, 0);
  debug ("setting declaration...\n");
  xml_declare (x);
  debug ("displaying...\n");
  display (x, b, "constructed");
  debug ("diving...\n");
  dive (x, xml_root (x)->key, 0);
  debug ("reverse diving...\n");
  rdive (x, xml_root (x)->key, 0);
  xml_free (x);
}

#endif /* UNITTEST */
