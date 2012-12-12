/*
 * b64.c
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
 *
 * base 64 encoding table.  From RFC 1113...
 * The encoding process represents 24-bit groups of input bits as 
 * output strings of 4 encoded characters. Proceeding from left to 
 * right across a 24-bit input group extracted from the output of 
 * step 3, each 6-bit group is used as an index into an array of 
 * 64 printable characters. 
 */

#ifdef UNITTEST
#include "unittest.h"
#endif

#include <stdio.h>
#include <string.h>
#include "log.h"
#include "b64.h"

#ifndef debug
#define debug(fmt...)
#endif

/*
 * Note for some encoding variants the last two characters are different.
 * We'll ignore that here and use typical coding.
 */
const char b64_tab[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

#define B64_PAD '='		/* the pad character			*/
#define B64_QUOTE '*'		/* we'll ignore this...			*/

/*
 * Encode a buffer. dst should be at least 137% the size of src.
 * If lb > 0, encoding has line breaks every lb characters.
 * Returns the encoded length not including the EOS.
 */
int b64_encode (char *dst, unsigned char *src, int len, int lb)
{
  int padding, v;
  char *d = dst;
  int bit6 = 0;

  if (padding = len % 3)
    padding = 3 - padding;
  while (len--)
  {
    switch (bit6++)
    {
      case 0:
	v = *src >> 2;
	len++;
	break;
      case 1:
	v = (*src++ & 0x3) << 4;
	v |= (*src & 0xf0) >> 4;
	break;
      case 2:
	v = (*src++ & 0xf) << 2;
	v |= (*src & 0xc0) >> 6;
	break;
      case 3:
	v = *src++ & 0x3f;
	bit6 = 0;
	break;
    }
    *d++ = b64_tab[v];
    if (lb && (((d - dst + 2) % (lb + 2)) == 0))
    {
      *d++ = '\r';
      *d++ = '\n';
    }
  }
  while (padding--)
    *d++ = B64_PAD;
  *d = 0;
  return (d - dst);
}

/*
 * Decode a buffer.  dst should be at least 75% the size of src,
 * and can be the same buffer as src.
 * Decoding stops on EOS, or a non-white/non-b64 encoding character.
 * Returns the decoded length not including the EOS.
 * 
 * Decoding is not optimized, but not used all that much so just
 * a simple implementation suffices.  Optimization would have to 
 * assume character set and codings (e.g. lookup table). We also don't
 * bother to check the padding...
 *
 * Note dst and src can be the same buffer.
 */
int b64_decode (unsigned char *dst, char *src)
{
  unsigned char *d;
  char *ch;
  int bit6 = 0;
  int v, c;

  d = dst;
  while (c = *src++)
  {
    if (isspace (c))
      continue;
    if ((ch = strchr (b64_tab, c)) == NULL)
      break;
    v = ch - b64_tab;
    switch (bit6++)
    {
      case 0 :
	*d = v << 2;
	break;
      case 1 :
	*d++ |= (v >> 4) & 0x03;
	*d = (v << 4) & 0xf0;
	break;
      case 2 :
	*d++ |= (v >> 2) &0x0f;
	*d = (v << 6) & 0xc0;
	break;
      case 3 :
	*d++ |= v & 0x3f;
	bit6 = 0;
	break;
    }
  }
  *d = 0;
  return (d - dst);
}

#ifdef UNITTEST
#include "unittest.h"

char Plain[] =
"Man is distinguished, not only by his reason, but by this singular "
"passion from other animals, which is a lust of the mind, that by a "
"perseverance of delight in the continued and indefatigable generation "
"of knowledge, exceeds the short vehemence of any carnal pleasure.";

char Coded[] =
"TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24sIGJ1dCBieSB0aGlz\r\n"
"IHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGljaCBpcyBhIGx1c3Qgb2Yg\r\n"
"dGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGVsaWdodCBpbiB0aGUgY29udGlu\r\n"
"dWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yga25vd2xlZGdlLCBleGNlZWRzIHRo\r\n"
"ZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hbCBwbGVhc3VyZS4=";

int main (int argc, char **argv)
{
  char buf[512], buf2[512];
  int i;

  i = b64_encode (buf, Plain, strlen (Plain), 76);
  if (i != strlen (Coded))
    error ("encoded size doesn't match\n");
  if (strcmp (buf, Coded))
    error ("encoding didn't match\n");
  i = b64_decode (buf2, buf);
  if (i != strlen (Plain))
    error ("decoded size doesn't match\n");
  if (strcmp (buf2, Plain))
    error ("decoding didn't match\n");
  info ("%s %s\n", argv[0], Errors?"failed":"passed");
  exit (Errors);
}

#endif
