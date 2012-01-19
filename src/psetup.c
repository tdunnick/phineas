/*
 * psetup - simple Phineas configuration program
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
#include <string.h>
#include <unistd.h>

#ifndef CMDLINE
#define CMDLINE
#endif

#ifdef UNITTEST
#undef UNITTEST
#define __TEST__
#endif
#include "util.c"
#include "dbuf.c"
#include "log.c"
#include "xml.c"

#define FATAL(code,msg...) fatal(msg),fexit(code)

#define NEXTARG (--argc ? *++argv : usagerr (*argv))
#define sperror() strerror (errno)

#define SENDER 1
#define RECEIVER 2
#define TRANSCEIVER 3

char *usagerr (char *arg)
{
  fprintf (stderr, "Invalid argument at '%s'\n"
    "usage: psetup [-iporst] [<config>]\n"
    "  -i <path>    set the install path\n"
    "  -P <port>    set port offsets\n"
    "  -p <partyid> set the Party ID\n"
    "  -o <org>     set the Organization\n"
    "  -t <file>    template file name\n"
    "  -r           receiver only\n"
    "  -s           sender only\n"
    "  <config>     configurate file name\n", 
    arg);
  exit (3);
  return (NULL);
}

int main (int argc, char **argv)
{
  char *org = NULL,
       *partyid = NULL,
       *installpath = NULL,
       *template = "templates/Phineas.xml",
       *config = "bin/Phineas.xml";
  int service = TRANSCEIVER,
      port = -1;
  char path[MAX_PATH];
  XML *xml;

  installpath = loadpath (argv[0]);
  if ((org = strstr (installpath, "\\bin\\")) != NULL)
  {
    org[1] = 0;
    org = NULL;
  }
  pathf (path, "logs/psetup.log");
  LOGFILE = log_open (path);
  while (--argc)
  {
    if (**++argv == '-')
    {
      switch (argv[0][1])
      {
	case 'i' : // install path
	  installpath = loadpath (NEXTARG);
	  break;
	case 'P' : // port offsets
	  port = atoi (NEXTARG);
	  break;
	case 'p' : // party Id
	  partyid = NEXTARG;
	  break;
	case 'o' : // organization
	  org = NEXTARG;
	  break;
	case 'c' : // config name
	  config = NEXTARG;
	  break;
	case 't' : // template name
	  template = NEXTARG;
	  break;
	case 'r' : // receiver only
	  service = RECEIVER;
	  break;
	case 's' : // sender only
	  service = SENDER;
	  break;
	default :
	  usagerr (*argv);
	  break;
      }
    }
    else
    {
      config = *argv;
    }
  }

#ifdef __TEST__
  info ("setting up test environment\n");
  chdir ("..");
  config = "Test.xml";
  unlink (config);
  org = "Phineas Health Test";
  partyid = "Test.Party.ID";
#endif

  pathf (path, config);
  info ("Configuration %s\n", path);
  if ((xml = xml_load (path)) == NULL)
  {
    pathf (path, template);
    if ((xml = xml_load (path)) == NULL)
      FATAL (1, "Couldn't load %s - %s\n", path, sperror ());
  }
  /*
   * make updates to the config
   */
  xml_set_text (xml, "Phineas.InstallDirectory", installpath);
  if (partyid != NULL)
    xml_set_text (xml, "Phineas.PartyId", partyid);
  if (org != NULL)
    xml_set_text (xml, "Phineas.Organization", org);

  setupService (xml, service);
  setupQueues (xml, service);
  setupPorts (xml, port < 0 ? service : port);
 
  xml_beautify (xml, 2);
  pathf (path, config);
  if (xml_save (xml, path))
    FATAL (2, "Couldn't save %s - %s\n", path, sperror ());
  info ("Setup completed\n");
  exit (0);
}

int fexit (int code)
{
  fprintf (stderr, "Press ENTER to continue... ");
  fflush (stderr);
  fgetc (stdin);
  return (exit (code));
}

int setupService (XML *xml, int service)
{
  char *p;

  // remove any un-needed queues
  switch (service)
  {
    case TRANSCEIVER : return (0);
    case RECEIVER : p = "Phineas.Sender"; break;
    case SENDER : p = "Phineas.Receiver"; break;
    default : return (0);
  }
  info ("Deleting service configuration %s\n", p);
  xml_delete (xml, p);
  return (0);
}

int setupQueues (XML *xml, int service)
{
  struct stat st;
  int i, n;
  char *t, *p, path[MAX_PATH];

  // setup default for access if dsn found
  pathf (path, "queues/phineas.dsn");
  if (stat (path, &st) == 0)
  {
    info ("configuring ACCESS queues...\n");
    n = xml_count (xml, p = "Phineas.QueueInfo.Connection");
    for (i = 0; i < n; i++)
    {
      if (strcmp (xml_getf (xml, "%s[%d].Name", p, i), "StdConn") == 0)
      {
        sprintf (path, "%s[%d].Type", p, i);
        xml_set_text (xml, path, "odbc");
        sprintf (path, "%s[%d].Unc", p, i);
        xml_set_text (xml, path, "queues/phineas.dsn");
        break;
      }
    }
    if (i == n)
    {
      FATAL (4, "Couldn't find 'StdConn' (standard connection)\n");
      return (-1);
    }
  }
  // remove any un-needed queues
  switch (service)
  {
    case TRANSCEIVER : return (0);
    case RECEIVER : t = "EbXmlRcvQ"; break;
    case SENDER : t = "EbXmlSndQ"; break;
    default : return (0);
  }
  n = xml_count (xml, p = "Phineas.QueueInfo.Queue");
  for (i = n; i; i--)
  {
    if (strcmp (xml_getf (xml, "%s[%d].Type", p, i-1), t))
      continue;
    sprintf (path, "%s[%d].Type", p, i-1);
    info ("Deleting %s from configuration\n", path);
    xml_delete (xml, path);
  }
  return (0);
}

/*
 * set the port numbers based on the service
 */
int setupPorts (XML *xml, int service)
{
  int offset;
  char buf[10];

  switch (service)
  {
    case RECEIVER : offset = 5000; break;
    case SENDER: offset = 6000; break;
    case TRANSCEIVER : offset = 4000; break;
    default : offset = service; break;
  }
  info ("Setting ports to %d/%d\n", offset + 80, offset + 443);
  sprintf (buf, "%d", offset + 80);
  xml_set_text (xml, "Phineas.Server.Port", buf);
  sprintf (buf, "%d", offset + 443);
  xml_set_text (xml, "Phineas.Server.SSL.Port", buf);
  return (0);
}

