/*
 * chelp.c - generate configuration web help
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

int tag_help (char *p, DBUF *b)
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
    if (*ch == '.')
    {
      *ch = 0;
      prefix[n++] = ch + 1;
    }
    ch++;
  }
  dbuf_printf (b, "<br><br>");
  for (i = 0; i < n; i++)
  {
    dbuf_printf (b, "<div style=\"padding-left: 20px;\">"
      "<b>&lt;%s&gt;</b>", prefix[i]);
  }
  dbuf_printf (b, "<div style=\"padding-left: 20px;\">"
    "The following configuration items are found here...</div>");
  for (i = n - 1; i >= 0; i--)
  {
    dbuf_printf (b, "<b>&lt;/%s&gt;</b></div>", prefix[i]);
  }
  dbuf_printf (b, "<br><br>");
  return (n);
}

char *type_help (char *type)
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

void input_help (XML *xml, char *prefix, int input, DBUF *b)
{
  char *suffix;

  suffix = xml_getf (xml, "%s[%d].Suffix", prefix, input);
  if (*suffix == 0)
    suffix = xml_getf (xml, "%s[%d].Name", prefix, input);
  dbuf_printf (b, "<div style=\"padding-left: 20px\">");
  if (*suffix)
  {
    dbuf_printf (b, "<b>");
    while (*suffix)
    {
      if (*suffix == '.')
	dbuf_putc (b, ' ');
      else
	dbuf_putc (b, *suffix);
      suffix++;
    }
    dbuf_printf (b, "</b> - ");
  }
  dbuf_printf (b, "%s<div style=\"padding-left: 20px\">%s</div></div>\n",
    type_help (xml_getf (xml, "%s[%d].Type", prefix, input)),
    xml_getf (xml, "%s[%d].Help", prefix, input));
}

void tab_help (XML *xml, char *prefix, int tab, DBUF *b)
{
  int i, inputs;
  char *path, p[128];

  dbuf_printf (b, "<div style=\"padding: 20px\">The <b>%s</b> tab - %s\n",
    xml_getf (xml, "%s[%d].Name", prefix, tab),
    xml_getf (xml, "%s[%d].Help", prefix, tab));
  path = xml_getf (xml, "%s[%d].Prefix", prefix, tab);
  tag_help (path, b);
  sprintf (p, "%s[%d].Input", prefix, tab);
  inputs = xml_count (xml, p);
  for (i = 0; i < inputs; i++)
  {
    input_help (xml, p, i, b);
  }
  dbuf_printf (b, "</div>\n");
}

void config_help (XML *xml, DBUF *b)
{
  int i, tabs;

  tabs = xml_count (xml, "Config.Tab");
  for (i = 0; i < tabs; i++)
  {
    tab_help (xml, "Config.Tab", i, b);
  }
}

int main (int argc, char **argv)
{
  FILE *fp;
  char *iname = NULL;
  char *oname = NULL;
  DBUF *b;
  XML *xml;

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
  config_help (xml, b);
  fp = fopen (oname, "w");
  fprintf (fp, "<html><body>%s</body></html>\n", dbuf_getbuf (b));
  fclose (fp);
  dbuf_free (b);
  xml_free (xml);
  exit (0);
}

