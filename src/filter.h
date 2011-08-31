/*
 * filter.h
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
 *
 * Functions used to run data through programs that act as filters.
 * These independent programs can be configured and selectively run
 * creating data processing extensions for PHINEAS senders and receivers.
 */

#ifndef __FILTER__
#define __FILTER__

#include "dbuf.h"

/*
 * Read file fname using filter cmd to buffer in
 */
int filter_read (char *cmd, char *fname, DBUF *in);

/*
 * write file fname using filter cmd from buffer out
 */
int filter_write (char *cmd, char *fname, DBUF *out);

/*
 * filter read/write fully buffered
 */
int filter_buf (char *cmd, DBUF *in, DBUF *out);
/*
 * filter using file system
 */
int filter_file (char *cmd, char *iname, char *oname)

#endif /* __FILTER__ */
