/*
 * xmlb.c
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

#include "dbuf.c"
#include "log.c"
#include "xml.c"

void xmlb (FILE *fp, char *fname, int indent)
{
  XML *xml;
  char *ch;

  if ((xml = xml_load (fname)) == NULL)
    return;
  xml_beautify (xml, indent);
  ch = xml_format (xml);
  xml_free (xml);
  if (ch != NULL)
  {
    if (fp == NULL)
      fp = stdout;
    fprintf (fp, "%s", ch);
    free (ch);
  }
}

int main (int argc, char **argv)
{
  FILE *fp = NULL;
  int i, indent = 2;

  for (i = 1; i < argc; i++)
  {
    if (argv[i][0] == '-')
    {
      if (argv[i][1] == 'o')
      {
	if (fp != NULL)
	{
	  fclose (fp);
	  fp = NULL;
	}
	if (++i < argc)
	  fp = fopen (argv[i], "w");
      }
      else if (isdigit (argv[i][1]))
        indent = atoi (argv[i] + 1);
      else
      {
	fprintf (stderr, "usage: %s [-o outfile] [infile]...\n", argv[0]);
	exit (1);
      }
    }
    else
    {
      xmlb (fp, argv[i], indent);
    }
  }
  if (fp != NULL)
    fclose (fp);
  exit (0);
}
