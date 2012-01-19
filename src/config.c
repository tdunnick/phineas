/*
 * config.c
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
#define __CONSOLE__
#endif

#ifdef __CONSOLE__

#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifdef UNITTEST
#undef UNITTEST
#define __SERVER__
#define __SENDER__
#define __RECEIVER__
#include "unittest.h"
#include "dbuf.c"
#include "util.c"
#include "xml.c"
#include "cpa.c"
#define debug _DEBUG_
#define UNITTEST
char ConfigName[MAX_PATH];
#else
extern char ConfigName[];
#endif

#include "dbuf.h"
#include "log.h"
#include "util.h"
#include "xml.h"

#ifndef debug
#define debug(fmt,...)
#endif

/*
 * these tag names are magic
 */
char REQUEST[] = "ConfigurationRequest",
     CONFIG[] = "Configuration File Name",
     EXPORT[] = "Export CPA",
     SAVE[] = "Save Configuration",
     LOAD[] = "Load Configuration",
     UPDATE[] = "Update Configuration";

/*
 * current configuration in edit
 */
char EditConfig[MAX_PATH] = "";
XML *EditXml = NULL;
XML *ConfigXml = NULL;

/*
 * default tag widths
 */
#define FILE_WIDTH 50
#define PORT_WIDTH 4
#define DIR_WIDTH 50
#define PASS_WIDTH 14
#define NUM_WIDTH 4

/*
 * input types
 */
#define INPUT_TEXT 0
#define INPUT_DIR 1
#define INPUT_FILE 2
#define INPUT_SELECT 3
#define INPUT_RADIO 4
#define INPUT_PASSWD 5
#define INPUT_NUMBER 6
#define INPUT_SUBMIT 7

/*
 * configuration tag types
 */
#define TAG_TAB 1
#define TAG_SET 2
#define TAG_INPUT 3

// most options for a list
#define MAXOPT 50

/********************** non-generating support **********************/

/*
 * reset configuration globals
 */
void config_reset ()
{
  *EditConfig = 0;
  if (EditXml != NULL)
    EditXml = xml_free (EditXml);
  if (ConfigXml != NULL)
    ConfigXml = xml_free (ConfigXml);
}

/*
 * return tag type for an input
 */
int config_inputtype (char *path)
{
  char *ch;

  ch = xml_getf (ConfigXml, "%s%cType", path, ConfigXml->path_sep);
  if (strcmp (ch, "text") == 0)
    return (INPUT_TEXT);
  if (strcmp (ch, "dir") == 0)
    return (INPUT_DIR);
  if (strcmp (ch, "file") == 0)
    return (INPUT_FILE);
  if (strcmp (ch, "select") == 0)
    return (INPUT_SELECT);
  if (strcmp (ch, "password") == 0)
    return (INPUT_PASSWD);
  if (strcmp (ch, "number") == 0)
    return (INPUT_NUMBER);
  if (strcmp (ch, "radio") == 0)
    return (INPUT_RADIO);
  if (strcmp (ch, "submit") == 0)
    return (INPUT_SUBMIT);
  return (INPUT_TEXT);
}

/*
 * build a unique id suffix for a configuration prefix
 * used for YETII tab id's and classes
 */
char *config_id (char *id, char *path)
{
  char *p;

  p = id;
  while (*p = *path++)
  {
    if (!isalnum (*p))
      *p = '_';
    p++;
  }
  return (id);
}

/*
 * return the TAG_ type for a configuration path
 */
int config_tag (char *path)
{
  char *tag;

  if ((tag = strrchr (path, ConfigXml->path_sep)) == NULL)
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

char *config_fixpath (XML *xml, char *path)
{
  char *ch;

  if (xml->path_sep != ' ')
  {
    for (ch = strchr (path, ' '); ch != NULL; ch = strchr (ch, ' '))
      *ch = xml->path_sep;
  }
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
char *config_path (XML *xml, char *buf, char *path, char *pre, int ix)
{
  char p[MAX_PATH];
  
  *buf = 0;
  				/* get tags for this path	*/
  strcpy (p, xml_getf (ConfigXml, "%s%cTags", path, ConfigXml->path_sep));
  if (*p == 0)			/* no tags, just use pre(fix)	*/
    strcpy (buf, pre);
  else if (*pre)		/* if pre, append with delim	*/
    sprintf (buf, "%s %s", pre, p);
  else				/* no pre, just use tags	*/
    strcpy (buf, p);
  config_fixpath (xml, buf);
  if (ix && *buf)		/* add index if given		*/
    sprintf (buf + strlen (buf), "%c%d",  xml->indx_sep, ix);
  return (buf);
}

/*
 * create a list of option or references for a tag
 */
int config_list (XML *xml, char *path, char **list)
{
  char *ch,
       p[MAX_PATH];
  int i, n, nopt;

  n = 0;
  ch = xml_getf (ConfigXml, "%s%cRef", path, ConfigXml->path_sep);
  if (*ch)
  {
    config_fixpath (xml, strcpy (p, ch));
    nopt = xml_count (xml, p);
    for (i = 0; i < nopt; i++)
    {
      ch = xml_getf (xml, "%s%c%d%cName", p, 
	xml->indx_sep, i, xml->path_sep);
      if (*ch && (n < MAXOPT))
	list[n++] = ch;
    }
  }
  else
  {
    sprintf (p, "%s%cOption", path, ConfigXml->path_sep);
    nopt = xml_count (ConfigXml, p);
    for (i = 0; i < nopt; i++)
    {
      ch = xml_getf (ConfigXml, "%s%cOption%c%d", path,
	ConfigXml->path_sep, ConfigXml->indx_sep, i);
      if (n < MAXOPT)
	list[n++] = ch;
    }
  }
  list[n] = NULL;
  return (n);
}

/*
 * format a prompt based on a (partial) tag suffix
 */
char *config_prompt (char *dst, char *src)
{
  char *p;

  p = dst;
  while (*p = *src++)
  {
    if (!isalnum (*p))
      *p = ' ';
    p++;
  }
  *p = 0;
  return (dst);
}

/*********************** html generation ************************/
/*
 * issue javascript help attributes
 * Note help is added to the child of the object moused over,
 * as a convenience when building MOST of the form's HTML.  In
 * some cases you will notice artificial HTML artifacts are created
 * because of this however.
 */

int config_addhelp (char *help, DBUF *b)
{
  int prevc;
  char *ch;

  if (strempty (ch = help))
    return (0);
  dbuf_printf (b, " OnMouseOver=\"showHelp (this, '");
  prevc = ' ';
  while (*ch)
  {
    if (isspace (*ch))
    {
      if (!isspace (prevc))
        dbuf_putc (b, ' ');
    }
    else if (*ch == '"')
      dbuf_printf (b, "&quot;");
    else if (*ch == '\'')
      dbuf_printf (b, "\\'");
    else
      dbuf_putc (b, *ch);
    prevc = *ch++;
  }
  return (dbuf_printf (b, "')\" onMouseOut=\"hideHelp()\" "));
}

/*
 * check a path for help and add it
 */
int config_help (char *path, DBUF *b)
{
  char *ch;

  if ((ch = xml_getf (ConfigXml, "%s%cHelp", path, ConfigXml->path_sep)) 
    == NULL) return (0);
  return (config_addhelp (ch, b));
}

/*
 * construct a select input from a list of values
 */
int config_select (char *name, char *val, char **list, DBUF *b)
{
  char *ch;

  dbuf_printf (b, "<select size='1' name='%s'>\n", name);
  while (*list != NULL)
  {
    dbuf_printf (b, "<option %svalue='%s'>%s</option>\n",
      strcmp (val, *list) ? "" : "selected ", *list,
      **list ? *list : "(none selected)");
    list++;
  }
  dbuf_printf (b, "</select>\n");
  return (0);
}

/*
 * construct a radio input from a list of values
 */
int config_radio (char *name, char *val, char **list, DBUF *b)
{
  int i;

  for (i = 0; list[i] != NULL; i++)
  {
    dbuf_printf (b, 
      "<input type='radio' name='%s' value='%s' %s/> %s<br>\n",
      name, list[i], strcmp (val, list[i]) ? "" : "checked", list[i]);
  }
  return (0);
}


/*
 * build a form input for this tag
 * xml - xml in edit
 * cpath - path to the configuration item
 * path - path to the xml data
 * name - suffix of data item
 * b - buffer for resulting html
 */
int config_getInput (XML *xml, char *cpath, 
  char *path, char *name, DBUF *b)
{
  int sz, width;
  char *value, 
       prompt[MAX_PATH],
       *list[MAXOPT];

  config_prompt (prompt, name);		/* build a prompt		*/
  					/* and get default value	*/
  if (strstarts (path, xml_root (xml)->key))
    value = xml_get_text (xml, path);
  else if (strcmp (prompt, CONFIG) == 0)
    value = EditConfig;
  else
    value = "";
  if (*path == 0)
    path = name;

  sz = dbuf_size (b);
  dbuf_printf (b, "<tr><td valign='top'>%s</td><td", prompt);
  config_help (cpath, b);
  dbuf_printf (b, ">");
  width = 24;
  switch (config_inputtype (cpath))
  {
    case INPUT_SUBMIT:
      dbuf_setsize (b, sz);
      dbuf_printf (b, "<tr><td colspan='2'");
      config_help (cpath, b);
      dbuf_printf (b, "><button type='button' name='%s' value='%s'"
	"onClick='setRequest (this, \"%s\")'>%s</button>",
        name, prompt, path, prompt);
      break;
    case INPUT_SELECT :
      if (config_list (xml, cpath, list))
        config_select (path, value, list, b);
      break;
    case INPUT_RADIO :
      if (config_list (xml, cpath, list))
        config_radio (path, value, list, b);
      break;
    case INPUT_PASSWD :
      dbuf_printf (b, "<input type='password' name='%s' value='%s' "
        "size='%d'/>", path, value, PASS_WIDTH);
      break;
    case INPUT_FILE :
      width = FILE_WIDTH;
      goto istext;
    case INPUT_DIR :
      width = DIR_WIDTH;
      goto istext;
    case INPUT_NUMBER :
      width = NUM_WIDTH;
      goto istext;
    case INPUT_TEXT :
      width = atoi (xml_getf (ConfigXml, "%s%cWidth", cpath, 
	ConfigXml->path_sep));
      goto istext;
    default :
istext:
      dbuf_printf (b, "<input type='text' name='%s' value='%s' "
        "size='%d'/>", path, value, width);
      break;
  }
  dbuf_printf (b, "</td></tr>\n");
  return (0);
}

/*
 * get inputs for a repeated field and include an "ADD" button
 */
int config_getRepeats (XML *xml, char *cpath,
  char *path, char *name, DBUF *b)
{
  int i,
      repeats,
      path_len,
      name_len;
  char hbuf[124];

  if ((repeats = xml_count (xml, path)) == 0)
    repeats = 1;
  path_len = strlen (path);
  name_len = strlen (name);
  for (i = 0; i < repeats; i++)
  {
    if (path_len)
      sprintf (path + path_len, "[%d]", i);
    sprintf (name + name_len, " %d", i + 1);
    config_getInput (xml, cpath, path, name, b);
  }
  name[name_len] = 0;
  path[path_len] = 0;
  sprintf (hbuf, "Click this to add another %s to the end of the list", 
    name);
  dbuf_printf (b, "<tr><td");
  config_addhelp (hbuf, b);
  dbuf_printf (b, "><button type='button' "
    "name='%s' onClick='addfield(this)'>"
    "Add %s</button></td><td></td></tr>", name, name);
  return (0);
}

/*
 * Build configuration for Tabs
 * A Tab identifies a discreate tabbed "page" of input.
 * All tabs in the current path are built together at the same time.
 */
int config_tabs (XML *xml, char *path, char *prefix, DBUF *buf)
{
  int i, n;
  char id[MAX_PATH],
       pre[MAX_PATH],
       p[MAX_PATH];

  // get a tab count
  if ((n = xml_count (ConfigXml, path)) < 2)
  {
    config_path (xml, pre, path, prefix, 0);
    return (config_build (xml, path, pre, buf));
  }

  config_id (id, path);
  // pre html
  dbuf_printf (buf, "<div id='%s' class='configlayout' >\n"
    "<ul id='%s-nav' class='configlayout' >\n", id, id);
  // build the tabs
  for (i = 0; i < n; i++)
  {
    dbuf_printf (buf, "<li");
    sprintf (p, "%s%c%d", path, ConfigXml->indx_sep, i);
    config_help (p, buf);
    dbuf_printf (buf, "><a href='#%s_%d'>%s</a></li>\n", id, i + 1, 
      xml_getf (ConfigXml, "%s%cName", p, ConfigXml->path_sep, i));
  }
  dbuf_printf (buf, "</ul><div class='tabs-container'>\n");
  // build the tab content
  for (i = 0; i < n; i++)
  {
    dbuf_printf (buf, "<div class='t_%s' id='%s_%d'>\n", id, id, i + 1);
    sprintf (p, "%s%c%d", path, ConfigXml->indx_sep, i);
    config_path (xml, pre, p, prefix, 0);
    config_build (xml, p, pre, buf);
    dbuf_printf (buf, "</div>\n");
  }
  // post html
  dbuf_printf (buf,
    "</div></div>\n<script type='text/javascript'>\n"
    "addTabber(new Yetii ({id:'%s',tabclass:'t_%s',persists:true}));\n"
    "</script>\n", id, id);
  return (0);
}

/*
 * Build configuration for a Set
 * A Set identifies repeated tags within the data xml.  The first
 * input for that set is assumed to be the Key if not defined, and
 * the keys determined that tabbed "pages".
 * Each set is built independently.
 */
int config_set (XML *xml, char *path, char *prefix, DBUF *buf)
{
  int i, n;
  char *ch,			/* for current input of set	*/
       key[MAX_PATH],
       pre[MAX_PATH],
       id[MAX_PATH];		/* id used for this tab set	*/

				/* add name and help...		*/
  ch = xml_getf (ConfigXml, "%s%cName", path, ConfigXml->path_sep);
  if (*ch)
  {
    dbuf_printf (buf, "<div");
    config_help (path, buf);
    dbuf_printf (buf, "><b>%s</b></div>", ch);
  }
  				/* set a new prefix 		*/
  config_path (xml, pre, path, prefix, 0);
  				/* get the ch for tabs 	*/
  ch = xml_getf (ConfigXml, "%s%cKey", path, ConfigXml->path_sep);
  if (*ch == 0) 		/* default uses first input	*/
  {
    ch = xml_getf (ConfigXml, "%s%cInput%cTags", path,
      ConfigXml->path_sep, ConfigXml->path_sep);
  }
  if (*ch == 0)
  {
    error ("Set at %s missing key\n", path);
    return (-1);
  }
  config_fixpath (xml, strcpy (key, ch));
   				
  n = xml_count (xml, pre);	/* get count of repeated items	*/
  config_id (id, path);		/* get an id			*/
  				/* set up tabber		*/
  dbuf_printf (buf, "<div id='%s' class='configlayout' >\n"
    "<ul id='%s-nav' class='configlayout' >\n", id, id);
  				/* build tabs using ch data	*/
  for (i = 0; i < n; i++)
  {
    config_path (xml, pre, path, prefix, i);
    dbuf_printf (buf, "<li");
    dbuf_printf (buf, "><a href='#%s_%d'>%s</a></li>\n", id, i + 1, 
      xml_getf (xml, "%s%c%s", pre, xml->path_sep, key));
  }
  dbuf_printf (buf, "<li><a href='#%s_%d'>NEW</a></li>\n"
    "</ul><div class='tabs-container'>", id, i + 1);
  				/* build the tab content	*/
  for (i = 0; i < n; i++)
  {
    config_path (xml, pre, path, prefix, i);
    dbuf_printf (buf, "<div class='t_%s' id='%s_%d'>\n", id, id, i + 1);
    config_build (xml, path, pre, buf);
    dbuf_printf (buf, "</div>\n");
  }
  config_path (xml, pre, path, prefix, i);
  dbuf_printf (buf, "<div class='t_%s' id='%s_%d'>\n", id, id, i + 1);
  config_build (xml, path, pre, buf);
  dbuf_printf (buf, "</div>\n");
  				/* activate tabber		*/
  dbuf_printf (buf,
    "</div></div>\n<script type='text/javascript'>\n"
    "addTabber(new Yetii({id:'%s',tabclass:'t_%s',persists:true}));\n"
    "</script>\n", id, id);
  return (0);
}

/*
 * build one input for a given path 
 * path should end in Input tag
 */
int config_input (XML *xml, char *cpath, char *prefix, DBUF *buf)
{
  char *name,
       path[MAX_PATH];

  /*
  suffix = xml_getf (ConfigXml, "%s%cSuffix", cpath, ConfigXml->path_sep);
  sprintf (path, "%s%c%s", prefix, xml->path_sep, suffix);
  */
  config_path (xml, path, cpath, prefix, 0);
  name = xml_getf (ConfigXml, "%s%cName", cpath, ConfigXml->path_sep);
  if (*name == 0)
    name = xml_getf (ConfigXml, "%s%cTags", cpath, ConfigXml->path_sep);
  debug ("path=%s name=%s\n", path, name);
  if (!strcmp (xml_getf (ConfigXml, "%s%cRepeats", cpath, xml->path_sep),
     "true"))
  {
    config_getRepeats (xml, cpath, path, name, buf);
  }
  else
  {
    config_getInput (xml, cpath, path, name, buf);
  }
}

/*
 * start and end input tables
 */
int config_endtable (int inputs, DBUF *buf)
{
  if (inputs)
  {
    dbuf_printf (buf, "</table>\n");
  }
  return (0);
}

int config_starttable (int inputs, DBUF *buf)
{
  if (inputs++ == 0)
  {
    dbuf_printf (buf, "<table summary=\"\">\n");
  }
  return (inputs);
}

/*
 * Build configuration inputs for a given path
 * Call recursively for each tabbed "page" of input
 *
 * xml - the data in edit (EditXml)
 * path - the (partial) path to ConfigXml
 * buf - holds the resulting HTML
 */
int config_build (XML *xml, char *path, char *prefix, DBUF *buf)
{
  int tabs = 0;			/* # tabs seen				*/
  int inputs = 0;		/* # consecutive inputs			*/
  char  p[MAX_PATH];

  debug ("build path=%s prefix=%s\n", path, prefix);
  strcpy (p, path);
  if (xml_first (ConfigXml, p) < 1)
    return (0);
  do
  {
    debug ("next path=%s\n", p);
    switch (config_tag (p))
    {
      case TAG_TAB :
	if (tabs++ == 0)	// only once per path level
	{
	  inputs = config_endtable (inputs, buf);
	  config_tabs (xml, p, prefix, buf);
	}
	break;
      case TAG_SET :
	inputs = config_endtable (inputs, buf);
	config_set (xml, p, prefix, buf);
	break;
      case TAG_INPUT :
	inputs = config_starttable (inputs, buf);
	config_input (xml, p, prefix, buf);
	break;
      default :
	break;
    }
  } while (xml_next (ConfigXml, p));
  config_endtable (inputs, buf);
  debug ("finished with %s\n", path);
  return (0);
}

/********************* set up **********************************/

/*
 * remove unwanted items from the configuration
 */
int config_remove (XML *xml, char *path, char *prefix, char *match)
{
  char  p[MAX_PATH],
	pre[MAX_PATH];

  if ((xml == NULL) || (ConfigXml == NULL))
    return (-1);
  debug ("build path=%s prefix=%s\n", path, prefix);
  strcpy (p, path);
  if (xml_first (ConfigXml, p) < 1)
    return (0);
  do
  {
    config_path (xml, pre, path, prefix, 0);
    debug ("next path=%s prefix=%s\n", p, pre);
    if (strstarts (pre, match))
      xml_delete (ConfigXml, path);
    else switch (config_tag (p))
    {
      case TAG_TAB :
      case TAG_SET :
	config_remove (xml, p, pre, match);
	break;
      default :
	break;
    }
  } while (xml_next (ConfigXml, p));
  debug ("finished with %s\n", path);
  return (0);
}

/******************* removal of empty data *********************/
/*
 * Remove empty repeated fields from data xml
 */
int config_cleaninput (XML *xml, char *prefix)
{
  int i, n;
  char p[MAX_PATH];

  n = xml_count (xml, prefix);
  sprintf (p, "%s%c%d", prefix, xml->indx_sep, i = 0);
  while (i < n)
  {
    if (*xml_get_text (xml, p))
    {
      sprintf (p, "%s%c%d", prefix, xml->indx_sep, ++i);
    }
    else
    {
      debug ("deleting input %s\n", p);
      xml_delete (xml, p);
      n--;
    }
  }
  return (0);
}

/*
 * Remove sets with empty keys from data xml
 */
int config_cleanset (XML *xml, char *path, char *prefix)
{
  int i, n;
  char *ch,
       suffix[MAX_PATH],
       pre[MAX_PATH];

				/* get a key suffix		*/
  ch = xml_getf (ConfigXml, "%s%cKey", path, ConfigXml->path_sep);
  if (*ch == 0)
  {
    ch = xml_getf (ConfigXml, "%s%cInput%cTags", path,
      ConfigXml->path_sep, ConfigXml->path_sep);
    if (*ch == 0)
    {
      error ("No Key or Tags found for %s\n", path);
      return (-1);
    }
  }
  config_fixpath (xml, strcpy (suffix, ch));
  /*
   * Loop setting path to data and checking for key.  If we find
   * a value then recurse on that path.  Otherwise, delete it.
   */
  config_path (xml, pre, path, prefix, i = 0);
  n = xml_count (xml, pre);
  while (i < n)
  {
    if (*xml_getf (xml, "%s%c%s", pre, xml->path_sep, suffix))
    {
      config_clean (xml, path, pre);
      config_path (xml, pre, path, prefix, ++i);
    }
    else
    {
      debug ("deleting set at %s for %s\n", pre, path);
      xml_delete (xml, pre);
      n--;
    }
  }
  return (0);
}

/*
 * cleans a tab, removing empty data from xml
 */
int config_cleantab (XML *xml, char *path, char *prefix)
{
  int i, n;
  char p[MAX_PATH],
       pre[MAX_PATH];

  n = xml_count (ConfigXml, path);
  i = 0;
  while (i < n)
  {
    sprintf (p, "%s%c%d", path, ConfigXml->indx_sep, i++);
    config_path (xml, pre, p, prefix, 0);
    config_clean (xml, p, pre);
  }
  return (0);
}

/*
 * remove empty repeated field and/or set with empty "key"
 */
int config_clean (XML *xml, char *path, char *prefix)
{
  char  p[MAX_PATH];

  if ((xml == NULL) || (ConfigXml == NULL))
    return (-1);
  debug ("build path=%s prefix=%s\n", path, prefix);
  strcpy (p, path);
  if (xml_first (ConfigXml, p) < 1)
    return (0);
  do
  {
    debug ("next path=%s\n", p);
    switch (config_tag (p))
    {
      case TAG_TAB :
	config_cleantab (xml, p, prefix);
	break;
      case TAG_SET :
	config_cleanset (xml, p, prefix);
	break;
      case TAG_INPUT :
	if (!strcmp ("true", xml_getf (ConfigXml, "%s%cRepeats", 
	  path, ConfigXml->path_sep)))
	config_cleaninput (xml, prefix);
	break;
      default :
	break;
    }
  } while (xml_next (ConfigXml, p));
  debug ("finished with %s\n", path);
  return (0);
}

/*
 * Set up for configuration entry.
 * Loads the editor configuration, and the Phineas configuration.
 * Removes unused parts from edit configuration.
 * Note there is magic (hard coded paths) here...
 */
int config_setup ()
{
  int i, n;
  char *ch, path[MAX_PATH];

  if (*EditConfig == 0)
    strcpy (EditConfig, ConfigName);
  debug ("current configuraton is %s\n", EditConfig);
  if ((EditXml != NULL) && (ConfigXml != NULL))
    return (0);
  ConfigXml = xml_free (ConfigXml);
  EditXml = xml_free (EditXml);
  if ((EditXml = xml_load (EditConfig)) == NULL)
  {
    error ("Can't load %s\n", EditConfig);
    return (-1);
  }
  pathf (path, "%s", xml_get_text (EditXml, "Phineas.Console.Config"));
  debug ("getting console configuration %s\n", path);
  if ((ConfigXml = xml_load (path)) == NULL)
  {
    error ("Can't load %s\n", path);
    return (-1);
  }
  /* conditionally remove chunks we don't use			*/
  ch = xml_root (ConfigXml)->key;
  config_path (EditXml, path, ch, "", 0);
#ifndef __SENDER__
  config_remove (EditXml, ch, path, "Phineas.Sender");
#endif
#ifndef __RECEIVER__
  config_remove (EditXml, ch, path, "Phineas.Receiver");
#endif
#ifndef __SERVER__
  config_remove (EditXml, ch, path, "Phineas.Server");
#endif
  config_clean (EditXml, ch, path);
  debug ("loaded configuraton %s\n", EditConfig);
  return (0);
}

/*
 * Build the input form in HTML
 * called from the console when the configuration editor is requested
 */
DBUF *config_getConfig ()
{
  DBUF *b;
  int i, n;
  char *r, path[MAX_PATH];

  debug ("getting configuration HTML\n");
  if (config_setup ())
  {
    debug ("setup failed!\n");
    return (NULL);
  }

  b = dbuf_alloc ();
#ifdef UNITTEST
  dbuf_printf (b, "<html><head>"
    "<title>Test Configuration</title>\n"
    "<link rel='stylesheet' type='text/css' href='console.css' />\n"
    "<SCRIPT src='yetii.js' type=text/javascript></SCRIPT>\n"
    "<SCRIPT src='config.js' type=text/javascript></SCRIPT>\n"
    "</head><body><h2>Configuration Edit Test</h2>\n"
    "<div id='console'>\n");
#endif
  dbuf_printf (b, "<form method='POST' action='#' >\n"
    "<input type='hidden' name='%s' id='%s' />", REQUEST, REQUEST);
  r = xml_root (ConfigXml)->key;
  config_path (EditXml, path, r, "", 0);
  config_build (EditXml, r, path, b);
  dbuf_printf (b, "</form><div style=\"height: 100px\"></div>\n");
#ifdef UNITTEST
  dbuf_printf (b, "</div></body></html>");
#endif
  debug ("returning configuration HTML\n");
  return (b);
}

/***************** POST processing *********************************/

/*
 * get a value for a given key
 */
char *config_getValue (char *data, char *key, char *value)
{
  char *ch, *amp, p[MAX_PATH];

  *value = 0;
  urlencode (p, key);
  strcat (p, "=");
  if ((ch = strstr (data, p)) == NULL)
    return (value);
  ch += strlen (p);
  if ((amp = strchr (ch, '&')) == NULL)
    amp = ch + strlen (ch);
  strncpy (value, ch, amp - ch);
  value[amp - ch] = 0;
  return (urldecode (value));
}

/*
 * get the next value, key pair and return new location
 */
char *config_nextValue (char *data, char *key, char *value)
{
  char *v, *amp;

  *value = *key = 0;
  if (data == NULL)
    return (NULL);
  if ((v = strchr (data, '=')) == NULL)
    return (NULL);
  strncpy (key, data, v - data);
  key[v - data] = 0;
  urldecode (key);
  if ((amp = strchr (++v, '&')) == NULL)
    amp = v + strlen (v);
  strncpy (value, v, amp - v);
  value[amp - v] = 0;
  urldecode (value);
  return (*amp ? amp + 1 : amp);
}

/*
 * Get the filename value and return submit type...
 * We have a hidden input for our submit buttons filled in by
 * javascript that has a name:path for the value.  Use this to
 * evaluate how to hand the submit.
 *
 * 0=load, 1=save, 2=export, 3=update -1=error
 */
int config_getPosted (char *data)
{
  int action;
  char key[MAX_PATH],
       value[MAX_PATH];
  char *submit[] =
  {
    LOAD, SAVE, EXPORT, UPDATE
  };

  			/* get hidding input request value	*/
  if (*config_getValue (data, REQUEST, value) == 0)
  {
    error ("Missing %s parameter from POST\n", REQUEST);
    return (-1);
  }
  debug ("request value=%s\n", value);
  			/* determine what action we are taking */
  for (action = 3; action >= 0; action--)
  {
    if (strstarts (value, submit[action]))
      break;
  }
  if (action < 0)
  {
    error ("POST missing request value\n");
    return (-1);
  }
  			/* then note config file name		*/
  if (*config_getValue (data, CONFIG, value) == 0)
  {
    error ("POST missing configuration file name\n");
    return (-1);
  }
  pathf (EditConfig, "%s", value);
  debug ("action=%d EditConfig=%s\n", action, EditConfig);
  return (action);
}

/*
 * Process the post request
 * called from the console when a configuration is submitted from edit
 */
DBUF *config_setConfig (XML *xml, char *req)
{
  int l, action;
  char *r,
       route[MAX_PATH],
       key[MAX_PATH],
       value[MAX_PATH];
  DBUF *console_doGet ();

  if ((EditXml == NULL) || (ConfigXml == NULL))
    return (console_doGet (xml, req));
  /*
   * skip past headers
   */
  if (req == NULL)
  {
    error ("NULL console POST request\n");
    return (NULL);
  }
  debug ("POST skipping headers...\n");
  if ((r = strstr (req, "\n\r\n")) == NULL)
  {
    error ("Malformed request %s\n", req);
    return (NULL);
  }
  r += 3;
  /*
   * determine what action to take
   */
  debug ("getting config name and submit action...\n");
  if ((action = config_getPosted (r)) < 1)
  {
#ifdef DEBUG
    if (action < 0)
      debug ("request:\n%s\n", r);
#endif
    if (action == 0)			/* Load...		*/
      EditXml = xml_free (EditXml);
    return (console_doGet (xml, req));	/* and refresh		*/
  }

  debug ("updating configuration...\n");
  if (EditXml == NULL)
    EditXml = xml_alloc ();
  *route = 0;				/* no route seen yet	*/
  /*
   * Loop through all the post arguments, parsing as we go.
   * The various "input" names in the form are actually paths
   * into our data xml, so that the post data looks like...
   * xml_path=value&xml_path=value&... with urlencoding.
   */
  while ((r = config_nextValue (r, key, value)) != NULL)
  {
    /*
     * if the key is into our configuration
     * then set the new value
     */
    if (strstarts (key, xml_root (EditXml)->key))
    {
      debug ("setting %s=%s\n", key, value);
      xml_set_text (EditXml, key, value);
    }
    else 			/* check "hidden" request	*/
    {				/* for route CPA export		*/
      if ((strcmp (key, REQUEST) == 0) && *value)
      {
	if (strstarts (value, EXPORT))
	{
          strcpy (route, strchr (value, ':') + 1);
	  debug ("route reference=%s\n", route);
	}
      }
    }
  }
  r = xml_root (ConfigXml)->key;
  config_path (EditXml, value, r, "", 0);
  config_clean (EditXml, r, value);
  if (action == 1)			/* save configuration	*/
  {
    debug ("format and save to %s\n", EditConfig);
    xml_beautify (EditXml, 2);
    if (backup (EditConfig))
      error ("Can't backup %s - %s\n", EditConfig, strerror (errno));
    else
      xml_save (EditXml, EditConfig);
  }
  else if (action == 2) 		/* export Cpa		*/
  {
    r = xml_get_text (EditXml, route);
    debug ("export route for %s at %s\n", r, route);
    cpa_create (EditXml, cpa_route (EditXml, r));
  }
  					/* just update		*/
  return (console_doGet (xml, req));
}

/******************** testing crud ******************************/

#ifdef UNITTEST

DBUF *console_doGet (XML *xml, char *req)
{
  DBUF *b = dbuf_alloc ();
  dbuf_printf (b, "The response\n");
  return (b);
}

int main (int argc, char **argv)
{
  DBUF *b;
  XML *xml;

  if ((xml = xml_parse (PhineasConfig)) == NULL)
    fatal ("Failed parsing PhineasConfig\n");
  loadpath (xml_get_text (xml, "Phineas.InstallDirectory"));
  xml_free (xml);
  pathf (ConfigName, "bin/Phineas.xml");
  b = config_getConfig ();
  writefile ("../console/test.html", dbuf_getbuf (b), dbuf_size (b));
  // debug ("%.*s\n", dbuf_size (b), dbuf_getbuf (b));
  dbuf_free (b);
  info ("%s unit test completed\n", argv[0]);
}

#endif /* UNITTEST */
#endif /* __CONSOLE__ */
