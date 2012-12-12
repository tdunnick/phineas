/*
 * cfg.c
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

/*
 * support for the Phineas running configuration use
 * see config.c for edit support
 */

#ifdef UNITTEST
#include "unittest.h"
#endif

#include <stdio.h>
#include <signal.h>

#include "util.h"
#include "log.h"
#include "xml.h"
#include "xcrypt.h"

#ifndef debug
#define debug(fmt...)
#endif

char ConfigName[MAX_PATH];	/* running file	 		*/
char ConfigPName[MAX_PATH];	/* formatted (display) file	*/
XML *Config;			/* running configuration	*/

/* used for configuration encryption				*/
char *ConfigCert =  NULL;	/* encryption certificate	*/
char *ConfigPass =  NULL;	/* and/or password		*/
int ConfigAlgorithm = TRIPLEDES;/* method for password		*/

/*
 * clean up configuration file artifacts
 */
void cfg_clean (char *path)
{
  if (*ConfigPName && !strcmp (path, ConfigName))
  {
    unlink (ConfigPName);
    *ConfigPName = 0;
  }
}

/*
 * free up configuration and remove formatted config
 */
void cfg_free ()
{
  if (Config != NULL)
    Config = xml_free (Config);
  cfg_clean (ConfigName);
}

/*
 * load a configuration, decrypting it if credentials found
 */
XML *cfg_load (char *path)
{
  XML *xml;
  
  cfg_clean (path);
  if ((xml = xml_load (path)) == NULL)
    return (NULL);
  if (strcmp (xml_root (xml), "Phineas"))
  {
    unsigned char *p;
    xcrypt_decrypt (xml, &p, ConfigCert, NULL, ConfigPass);
    xml_free (xml);
    xml = xml_parse (p);
    free (p);
  }
  return (xml);
}

/*
 * save a configuration, encrypting it if credentials found
 */
int cfg_save (XML *xml, char *path)
{
  int r;
  XML *x;

  cfg_clean (path);
  x = xml;
  if ((ConfigCert != NULL) || (ConfigPass != NULL))
  {
    unsigned char *p;

    p = xml_format (xml);
    x = xcrypt_encrypt (p, strlen (p) + 1, ConfigCert, NULL, 
      ConfigPass, AES256);
    free (p);
  }
  r = xml_save (x, path);
  if (x != xml)
    xml_free (x);
  return (r);
}

/*
 * Create a censored configuration for display purposes, and return
 * it's name.  This has the side effect of beautifying the Config XML.
 */
char *cfg_format ()
{
  char *buf, *p, *pe;

  if ((Config == NULL) || (*ConfigName == 0))
    return (NULL);
  if (*ConfigPName != 0)
    return (ConfigPName);
  strcpy (ConfigPName, ConfigName);
  if ((p = strrchr (ConfigPName, '.')) == NULL)
    p = ConfigName + strlen (ConfigPName);
  strcpy (p, "Configuration.xml");
  xml_beautify (Config, 2);
  buf = xml_format (Config);
  pe = buf;
  while ((p = strstr (pe, "<Password>")) != NULL)
  {
    p += 10;
    pe = strstr (p, "</Password>");
    if (pe == NULL)
      break;
    if (p != pe)
    {
      *p++ = '*';
      strcpy (p, pe);
    }
  }
  writefile (ConfigPName, buf, strlen (buf));
  free (buf);
  return (ConfigPName);
}

/********************** access ************************************/

/*
 * find the index for a repeated configuration item
 */
cfg_index (XML *xml, char *path, char *name)
{
  int i, n;
  char *ch;

  n = xml_count (xml, path);
  for (i = 0; i < n; i++)
  {
    if (!strcmp (name, xml_getf (xml, "%s[%d].Name", path, i)))
      return (i);
  }
  error ("Can't find name matching %s for %s\n", name, path);
  return (-1);
}

#ifdef UNITTEST
#undef UNITTEST
#undef debug
#include "util.c"
#include "b64.c"
#include "dbuf.c"
#include "xmln.c"
#include "xml.c"
#include "crypt.c"
#include "xcrypt.c"

int main (int argc, char **argv)
{
  struct stat st;

  loadpath ("..");
  pathf (ConfigName, "templates/Phineas.xml");
  Config = cfg_load (ConfigName);
  if (Config == NULL)
    fatal ("Configuration failed to load\n");
  if (cfg_format () == NULL)
    fatal ("Configuration failed to format\n");
  if (stat (ConfigPName, &st))
    fatal ("Configuration not created\n");
  cfg_free ();
  if (!stat (ConfigPName, &st))
    fatal ("Configuration not removed\n");
  info ("%s %s\n", argv[0], Errors ? "failed" : "passed");
  exit (Errors);
}

#endif /* UNITTEST */
