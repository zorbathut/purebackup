/*
  PureBackup - human-readable backup output
  Copyright (C) 2005 Ben Wilhelm

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the license only.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 
*/

#include "item.h"

#include <openssl/sha.h>

bool operator==(const Checksum &lhs, const Checksum &rhs) {
  return !memcmp(lhs.bytes, rhs.bytes, sizeof(lhs.bytes));
}

Checksum Item::checksum() {
  if(cs_valid)
    return cs;
  
  printf("Checksumming %s\n", local_path.c_str());
  
  SHA_CTX c;
  SHA1_Init(&c);
  FILE *phil = fopen(local_path.c_str(), "rb");
  while(1) {
    char buf[1024*128];
    int rv = fread(buf, 1, sizeof(buf), phil);
    printf("%d bytes\n", rv);
    SHA1_Update(&c, buf, rv);
    if(rv != sizeof(buf))
      break;
  }
  SHA1_Final(cs.bytes, &c);
  fclose(phil);
  
  cs_valid = true;
  return cs;
}