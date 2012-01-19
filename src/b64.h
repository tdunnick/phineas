/*
 * b64.h
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
#ifndef __B64__
#define __B64__

/*
 * Encode a buffer. dst should be at least 137% the size of src.
 * If lb > 0, encoding has line breaks every lb characters.
 * Returns the encoded length not including the EOS.
 */
int b64_encode (char *dst, unsigned char *src, int len, int lb);
/*
 * Decode a buffer.  dst should be at least 75% the size of src.
 * Decoding stops on EOS, or a non-white/non-b64 encoding character.
 * Returns the decoded length not including the EOS.
 */
int b64_decode (unsigned char *dst, char *src);

#endif /* __B64__ */
