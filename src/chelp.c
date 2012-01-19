/*
 * chelp.c - generate configuration web help
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
#include <string.h>
#include <errno.h>

#ifdef UNITTEST
#undef UNITTEST
#define INAME "../console/config.xml"
#define ONAME "../console/chelp.html"
#else
#define INAME "console/config.xml"
#define ONAME "console/chelp.html"
#endif
#include "util.c"
#include "log.c"
#include "dbuf.c"
#include "xml.c"
/*
 * configuration tag types
 */
#define TAG_TAB 1
#define TAG_SET 2
#define TAG_INPUT 3

#define PATH_SEP '.'


/*
 * return the TAG_ type for a configuration path
 */
int chelp_tagtype (XML *xml, char *path)
{
  char *tag;

  if ((tag = strrchr (path, xml->path_sep)) == NULL)
    return (0);
  tag++;
  if (strncmp (tag, "Tab", 3) == 0)
    return (TAG_TAB);
  if (strncmp (tag, "Set", 3) == 0)
    return (TAG_SET);
  if (strncmp (tag, "Input", 5) == 0)
    return (TAG_INPUT);
  return (0);
}

char *chelp_fixpath (char *path)
{
  char *ch;

  for (ch = strchr (path, ' '); ch != NULL; ch = strchr (ch, ' '))
      *ch = PATH_SEP;
  return (path);
}

/*
 * build a new path into our data
 * xml - data in edit
 * buf - destination for new prefix
 * path - path into configuration item
 * pre - current path (prefix) to prepend
 * ix - index for repeated data tacked onto the end
 */
char *chelp_path (XML *xml, char *buf, char *path, char *pre)
{
  char p[MAX_PATH];
  
  *buf = 0;
  				/* get tags for this path	*/
  strcpy (p, xml_getf (xml, "%s%cTags", path, PATH_SEP));
  if (*p == 0)			/* no tags, just use pre(fix)	*/
    strcpy (buf, pre);
  else if (*pre)		/* if pre, append with delim	*/
    sprintf (buf, "%s %s", pre, p);
  else				/* no pre, just use tags	*/
    strcpy (buf, p);
  chelp_fixpath (buf);
  return (buf);
}

/*
 * show opening tags
 */
int chelp_opentag (char *p, DBUF *b)
{
  char *ch, *prefix[10], buf[128];
  int i, n;

  if (*p == 0)
  {
    return (1);
  }
  strcpy (buf, p);
  n = 1;
  prefix[0] = ch = buf;
  while (*ch)
  {
    if ((*ch == ' ') || (*ch == PATH_SEP))
    {
      *ch = 0;
      prefix[n++] = ch + 1;
    }
    ch++;
  }
  for (i = 0; i < n; i++)
  {
    dbuf_printf (b, "<div style=\"padding-left: 20px;\">"
      "<b class='gtag'>&lt;%s&gt;</b>", prefix[i]);
  }
  return (n);
}

/*
 * show closing tags
 */
int chelp_closetag (char *p, DBUF *b)
{
  char *ch, *prefix[10], buf[128];
  int i, n;

  if (*p == 0)
  {
    return (1);
  }
  strcpy (buf, p);
  n = 1;
  prefix[0] = ch = buf;
  while (*ch)
  {
    if ((*ch == ' ') || (*ch == PATH_SEP))
    {
      *ch = 0;
      prefix[n++] = ch + 1;
    }
    ch++;
  }
  for (i = n - 1; i >= 0; i--)
  {
    dbuf_printf (b, "<b class='gtag'>&lt;/%s&gt;</b></div>", prefix[i]);
  }
  return (n);
}

/*
 * show the tabs for a section
 */
int chelp_tag (char *p, DBUF *b)
{
  chelp_opentag (p, b);
  return (chelp_closetag (p, b));
}

char *chelp_type (char *type)
{
  if (strcmp (type, "dir") == 0)
    return ("a folder or directory");
  if (strcmp (type, "file") == 0)
    return ("a file");
  if (strcmp (type, "select") == 0)
    return ("a drop down list of item selections");
  if (strcmp (type, "password") == 0)
    return ("a password");
  if (strcmp (type, "number") == 0)
    return ("a numeric value");
  if (strcmp (type, "submit") == 0)
    return ("a submit button");
  return ("text");
}

/*
 * HTML help for an input
 */
int chelp_input (XML *xml, char *path, char *prefix, DBUF *b)
{
  int i, n;
  char *tags, *name,
       pre[MAX_PATH];

  chelp_path (xml, pre, path, prefix);
  name = tags = xml_getf (xml, "%s%cTags", path, xml->path_sep);
  chelp_opentag (tags, b);
  if (*name == 0)
    name = xml_getf (xml, "%s%cName", path, xml->path_sep);
  dbuf_printf (b, "<div style=\"padding-left: 20px\">");
  if (*name)
    dbuf_printf (b, "<b>%s</b> - ", name);
  dbuf_printf (b, "%s<div style=\"padding-left: 20px\">%s</div></div>\n",
    chelp_type (xml_getf (xml, "%s%cType", path, xml->path_sep)),
    xml_getf (xml, "%s%cHelp", path, xml->path_sep));
  chelp_closetag (tags, b);
  return (0);
}

/*
 * HTML help for a set
 */
int chelp_set (XML *xml, char *path, char *prefix, DBUF *b)
{
  int i, n;
  char *ch,
       suffix[MAX_PATH],
       pre[MAX_PATH];

				/* get a key suffix		*/
  ch = xml_getf (xml, "%s%cKey", path, xml->path_sep);
  if (*ch == 0)
  {
    ch = xml_getf (xml, "%s%cInput%cTags", path,
      xml->path_sep, xml->path_sep);
    if (*ch == 0)
    {
      error ("No Key or Tags found for %s\n", path);
      return (-1);
    }
  }
  dbuf_printf (b, "<div class='gset'>"
    "<span class='gtab'>&lt;%s&gt;'s...</span> ", ch);
  chelp_fixpath (strcpy (suffix, ch));
  chelp_path (xml, pre, path, prefix);
  ch = xml_getf (xml, "%s%cName", path, xml->path_sep);
  if (*ch)
    dbuf_printf (b, "The <b>%s</b> set - %s</div>\n", ch,
      xml_getf (xml, "%s%cHelp", path, xml->path_sep));
  else
    dbuf_printf (b, "Repeated set items for %s</div>\n", 
      xml_getf (xml, "%s%cTags", path, xml->path_sep));
  ch = xml_getf (xml, "%s%cTags", path, xml->path_sep);
  chelp_opentag (ch, b);
  chelp (xml, path, pre, b);
  chelp_closetag (ch, b);
  return (0);
}

/*
 * HTML help for a tab
 */
int chelp_tab (XML *xml, char *path, char *prefix, DBUF *b)
{
  char *ch,
       pre[MAX_PATH];

  chelp_path (xml, pre, path, prefix);
  dbuf_printf (b, "<div class='gtabs'>"
    "<span class='gtab'>%s</span> - %s</div>\n",
    xml_getf (xml, "%s%cName", path, xml->path_sep),
    xml_getf (xml, "%s%cHelp", path, xml->path_sep));
  ch = xml_getf (xml, "%s%cTags", path, xml->path_sep);
  chelp_opentag (ch, b);
  chelp (xml, path, pre, b);
  chelp_closetag (ch, b);
  return (0);
}

/*
 * HTML help for a configuration block
 */
int chelp (XML *xml, char *path, char *prefix, DBUF *b)
{
  char  p[MAX_PATH];

  if (xml == NULL)
    return (-1);
  debug ("help path=%s prefix=%s\n", path, prefix);
  strcpy (p, path);
  if (xml_first (xml, p) < 1)
    return (0);
  do
  {
    debug ("next path=%s\n", p);
    switch (chelp_tagtype (xml, p))
    {
      case TAG_TAB :
	chelp_tab (xml, p, prefix, b);
	break;
      case TAG_SET :
	chelp_set (xml, p, prefix, b);
	break;
      case TAG_INPUT :
	chelp_input (xml, p, prefix, b);
	break;
      default :
	break;
    }
  } while (xml_next (xml, p));
  debug ("finished with %s\n", path);
  return (0);
}

int main (int argc, char **argv)
{
  FILE *fp;
  char *iname = NULL;
  char *oname = NULL;
  DBUF *b;
  XML *xml;
  char *r, prefix[MAX_PATH];
  char *style =
    "<style type=\"text/css\"> <!--"
    ".gtag { color:#f00; }"
    ".gtab { background:#ccc; font-weight:bold; border:1px solid #888;"
    " color:#000; padding:2px 8px; }"
    ".gtabs { padding:20px; } "
    ".gset { padding:20px; } "
    ".legend td { padding-right:15px; } "
    "--> </style>";
  char *legend =
    "<h3>Legend</h3>"
    "<table class='legend'><tr><td><span class='gtab'>Tab</span></td>"
    "<td>This is a section &quot;tab&quot;.  Click on a tab to "
    " select a page of related configuration data.  A page may "
    " have sub-tabs.  Each tab generally has informative help "
    "associated with it.</td></tr>"
    "<tr><td><span class='gtab'>&lt;Set&gt;'s...</span></td>"
    "<td>&quot;Sets&quot; are very similar to the tab above, "
    "except that the tab itself is a data value used to identify a set "
    "of (repeated) data.  Clicking on a set tab selects a page with "
    "data related to that data value.  Set's always have a "
    "<span class='gtab'>NEW</span> tab used to add additional set data."
    "</td></tr><tr><td><b class='gtag'>&lt;Tag&gt;</b></td>"
    "<td>This is an XML configuration &quot;tag&quot;.  These tags appear "
    "in the configuration to denote and identify data values. "
    "They are indented here to show their relative hierarchy.</td></tr>"
    "<tr><td><b>Prompt</b></td><td>Each data item is identified by a "
    "&quot;prompt&quot;.  The prompt includes a &quot;type&quot; and "
    "description of what that data is used for or represents</td></tr>"
    "</table><hr/>\n";

  while (--argc)
  {
    if (**++argv == '-') switch (*argv[1])
    {
      case 'i' :
	if (argc > 1)
	{
	  argc--;
	  iname = *++argv;
	}
	break;
      case 'o' :
	if (argc > 1)
	{
	  argc--;
	  oname = *++argv;
	}
	break;
      default :
	break;
    }
  }
  if (iname == NULL)
    iname = INAME;
  if (oname == NULL)
    oname = ONAME;
  if ((xml = xml_load (iname)) == NULL)
  {
    fprintf (stderr, "Can't load %s\n", iname);
    exit (1);
  }
  b = dbuf_alloc ();
  r = xml_root (xml)->key;
  chelp_path (xml, prefix, r, "");
  chelp (xml, r, prefix, b);
  fp = fopen (oname, "w");
  fprintf (fp, "<html>%s<body>%s%s</body></html>\n", 
    style, legend, dbuf_getbuf (b));
  fclose (fp);
  dbuf_free (b);
  xml_free (xml);
  exit (0);
}

