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
#include "debug.h"

#include <openssl/sha.h>

string Checksum::toString() const {
  string ostr;
  for(int i = 0; i < 20; i++) {
    char bf[8];
    sprintf(bf, "%02x", bytes[i]);
    ostr += bf;
  }
  return ostr;
}

bool operator==(const Checksum &lhs, const Checksum &rhs) {
  return !memcmp(lhs.bytes, rhs.bytes, sizeof(lhs.bytes));
}

Checksum Item::checksum() const {
  return checksumPart(size);
}

Checksum Item::checksumPart(int len) const {
  bool gottotal = false;
  for(int i = 0; i < css.size(); i++) {
    if(css[i].first == len)
      return css[i].second;
    if(css[i].first == size)
      gottotal = true;
  }
  
  if(len == size)
    gottotal = true;
  
  int bytu = 0;
  
  SHA_CTX c;
  SHA1_Init(&c);
  FILE *phil = fopen(local_path.c_str(), "rb");
  while(1) {
    char buf[1024*128];
    int rv = fread(buf, 1, sizeof(buf), phil);
    
    if(bytu + rv >= len) {
      SHA1_Update(&c, buf, len - bytu);
      Checksum tcs;
      SHA1_Final(tcs.bytes, &c);
      css.push_back(make_pair(len, tcs));
      return tcs;
    } else {
      SHA1_Update(&c, buf, rv);
    }
    bytu += rv;
    if(rv != sizeof(buf)) {
      CHECK(0);
    }
  }
  
  CHECK(0);
}

void Item::setTotalChecksum(const Checksum &chs) {
  CHECK(type == MTI_ORIGINAL);
  CHECK(css.size() == 0);
  css.push_back(make_pair(size, chs));
}
