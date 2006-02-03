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
#include "parse.h"

#include <openssl/sha.h>

string Metadata::toKvd() const {
  kvData kvd;
  kvd.category = "metadata";
  kvd.kv["timestamp"] = StringPrintf("%lld", timestamp);
  return putkvDataInlineString(kvd);
}

Metadata metaParseFromKvd(kvData kvd) {
  CHECK(kvd.category == "metadata");
  Metadata mtd;
  sscanf(kvd.consume("timestamp").c_str(), "%lld", &mtd.timestamp);
  return mtd;
}

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

void ItemShunt::seek(long long pos) {
  fseeko(local_file, pos, SEEK_SET);
}
int ItemShunt::read(char *buffer, int len) {
  return fread(buffer, 1, len, local_file);
}

ItemShunt::ItemShunt() {
  local_file = NULL;
}
ItemShunt::~ItemShunt() {
  fclose(local_file);
}

ItemShunt *Item::open() const {
  CHECK(type == MTI_LOCAL);
  ItemShunt *is = new ItemShunt;
  is->local_file = fopen(local_path.c_str(), "rb");
  CHECK(is->local_file);
  return is;
}

Checksum Item::checksum() const {
  return checksumPart(size());
}

extern long long cssi;

Checksum Item::checksumPart(int len) const {
  CHECK(isReadable());

  for(int i = 0; i < css.size(); i++) {
    if(css[i].first == len)
      return css[i].second;
  }
  
  CHECK(type == MTI_LOCAL);
  
  int bytu = 0;
  
  SHA_CTX c;
  SHA1_Init(&c);
  FILE *phil = fopen(local_path.c_str(), "rb");
  if(!phil) {
    printf("Couldn't open %s during checksum\n", local_path.c_str());
    CHECK(isReadable());
    CHECK(0);
  }
  while(1) {
    char buf[1024*512];
    int rv = fread(buf, 1, sizeof(buf), phil);
    
    if(bytu + rv >= len) {
      SHA1_Update(&c, buf, len - bytu);
      Checksum tcs;
      SHA1_Final(tcs.bytes, &c);
      fclose(phil);
      cssi += len;
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

void Item::addVersion(int x) {
  needed_versions.insert(x);
}
const set<int> &Item::getVersions() const {
  return needed_versions;
}

bool Item::isReadable() const {
  if(readable == -1) {
    if(type == MTI_LOCAL) {
      readable = 1;
      FILE *fil = fopen(local_path.c_str(), "rb");
      if(!fil) {
        printf("Cannot read %s\n", local_path.c_str());
        readable = 0;
      } else {
        fclose(fil);
      }
    } else if(type == MTI_ORIGINAL) {
      readable = 1; // for certain definitions of readable, I suppose
    } else {
      CHECK(0);
    }
  }
  return readable == 1;
};

string Item::toString() const {
  return StringPrintf("%lld %lld %s", size(), metadata().timestamp, checksum().toString().c_str());
}

Item Item::MakeLocal(const string &full_path, long long size, const Metadata &meta) {
  Item item;
  item.type = MTI_LOCAL;
  item.local_path = full_path;
  item.p_size = size;
  item.p_metadata = meta;
  return item;
}

Item Item::MakeOriginal(long long size, const Metadata &meta, const Checksum &checksum, const set<int> &versions) {
  Item item;
  item.type = MTI_ORIGINAL;
  item.p_size = size;
  item.p_metadata = meta;  
  item.css.push_back(make_pair(size, checksum));
  item.needed_versions = versions;
  return item;
}

Item Item::MakeSsh(const string &user, const string &pass, const string &host, const string &full_path, long long size, const Metadata &meta) {
  Item item;
  item.type = MTI_SSH;
  item.p_size = size;
  item.p_metadata = meta;  
  item.ssh_user = user;
  item.ssh_pass = pass;
  item.ssh_host = host;
  item.ssh_path = full_path;
  return item;
}

Item::Item() {
  type = MTI_NONEXISTENT;
  readable = -1;
}

