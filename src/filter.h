/*
 * filter.h
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
 * Functions used to run data through programs that act as filters.
 * These independent programs can be configured and selectively run
 * creating data processing extensions for PHINEAS senders and receivers.
 */

#ifndef __FILTER__
#define __FILTER__

#include "dbuf.h"

/*
 * Run a filter.
 * cmd - command string where $in=input filie and $out=output file
 * fin - input file name
 * in - input buffer
 * fout - output file name
 * out - output buffer
 * err - malloc'ed stderr output
 * timeout - milliseconds to wait for filter
 *
 * return filter exit code
 */
int filter_run (char *cmd, char *fin, DBUF *in, char *fout, DBUF *out, 
    char **err, int timeout);

#endif /* __FILTER__ */
