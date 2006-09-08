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

#ifdef WIN32API
void ItemShunt::seek(long long pos) {
  LONG low = pos;
  LONG high = pos >> 32;
  SetFilePointer(local_file, low, &high, FILE_BEGIN);
  CHECK(GetLastError() == NO_ERROR);
}
int ItemShunt::read(char *buffer, int len) {
  DWORD out;
  if(!ReadFile(local_file, buffer, len, &out, NULL)) {
    printf("Problem with reading from %s\n", fname.c_str());
    CHECK(0);
  }
  return out;
}

ItemShunt* ItemShunt::LocalFile(const string &local_fname) {
  HANDLE local_file = CreateFile(local_fname.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
  if(local_file == INVALID_HANDLE_VALUE) {
    printf("Failure at opening %s!\n", local_fname.c_str());
    return NULL;
  }
  
  ItemShunt *isr = new ItemShunt;
  isr->local_file = local_file;
  isr->fname = local_fname;
  return isr;
}
ItemShunt::ItemShunt() {
  local_file = NULL;
}
ItemShunt::~ItemShunt() {
  CloseHandle(local_file);
}
#else
void ItemShunt::seek(long long pos) {
  fseeko(local_file, pos, SEEK_SET);
}
int ItemShunt::read(char *buffer, int len) {
  return fread(buffer, 1, len, local_file);
}

ItemShunt::ItemShunt(const string &local_fname) {
  local_file = fopen(local_fname.c_str(), "rb");
  CHECK(local_file);
}
ItemShunt::~ItemShunt() {
  fclose(local_file);
}
#endif

ItemShunt *Item::open() const {
  CHECK(type == MTI_LOCAL);
  return ItemShunt::LocalFile(local_path);
}

Checksum Item::signature() const {
  return signaturePart(size());
}

Checksum Item::signaturePart(long long len) const {
  if(!isReadable()) {
    printf("Isn't readable: %s\n", local_path.c_str());
    CHECK(0);
  }
  
  for(int i = 0; i < sss.size(); i++) {
    if(sss[i].first == len)
      return sss[i].second;
  }
  
  for(int i = 0; i < css.size(); i++) {
    if(css[i].first == len) {
      Checksum cst = css[i].second;
      memset(cst.bytes, 0, sizeof(cst.bytes));
      sss.push_back(make_pair(len, cst));
      return cst;
    }
  }
  
  CHECK(type == MTI_LOCAL);
  
  Checksum tcs;
  memset(tcs.bytes, 0, sizeof(tcs.bytes));
  memset(tcs.signature, 0, sizeof(tcs.signature));
  
  long long poss = (len - sizeof(tcs.signature)) / 2;
  if(poss < 0)
    poss = 0;
  long long pose = poss + 32;
  if(pose > size())
    pose = size();
  
  ItemShunt *snt = open();
  snt->seek(poss);
  snt->read((char*)tcs.signature, pose - poss);
  delete snt;
  
  sss.push_back(make_pair(len, tcs));
  
  return tcs;
}

Checksum Item::checksum() const {
  return checksumPart(size());
}

extern long long cssi;

Checksum Item::checksumPart(long long len) const {
  if(!isReadable()) {
    printf("Isn't readable: %s", local_path.c_str());
    CHECK(0);
  }

  for(int i = 0; i < css.size(); i++) {
    if(css[i].first == len)
      return css[i].second;
  }
  
  if(type != MTI_LOCAL) {
    printf("Invalid type %d, size %lld\n", type, size());
    for(int i = 0; i < css.size(); i++)
      printf("Css %lld\n", css[i].first);
    CHECK(0);
  }
  //printf("Doing full checksum of %s\n", local_path.c_str());
  
  long long bytu = 0;
  
  SHA_CTX c;
  SHA1_Init(&c);
  ItemShunt *phil = open();
  if(!phil) {
    printf("Couldn't open %s during checksum\n", local_path.c_str());
    CHECK(isReadable());
    CHECK(0);
  }
  while(1) {
    char buf[1024*512];
    int rv = phil->read(buf, sizeof(buf));
    
    if(bytu + rv >= len) {
      SHA1_Update(&c, buf, len - bytu);
      Checksum tcs = signaturePart(len);
      SHA1_Final(tcs.bytes, &c);
      delete phil;
      cssi += len;
      css.push_back(make_pair(len, tcs));
      return tcs;
    } else {
      SHA1_Update(&c, buf, rv);
    }
    bytu += rv;
    if(rv != sizeof(buf)) {
      printf("Trying to read %lld from %s, only picked up %lld, last value %d!\n", len, local_path.c_str(), bytu + rv, rv);
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
      ItemShunt *fil = open();
      if(!fil) {
        printf("Cannot read %s\n", local_path.c_str());
        readable = 0;
      } else {
        delete fil;
      }
    } else if(type == MTI_ORIGINAL) {
      readable = 1; // for certain definitions of readable, I suppose
    } else {
      CHECK(0);
    }
  }
  return readable == 1;
};

bool Item::isChecksummable() const {
  return css.size() || isReadable();
}

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

/*
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
}*/

Item::Item() {
  type = MTI_NONEXISTENT;
  readable = -1;
}

int if_presig = 0;
int if_mid = 0;
int if_full = 0;

bool identicalFile(const Item &lhs, const Item &rhs, long long bytes) {
  if(bytes == -1) {
    if(lhs.size() != rhs.size())
      return false; // this shouldn't actually happen
    if_presig++;
    if(!(lhs.signature() == rhs.signature()))
      return false;
    if_mid++;
    if(!(lhs.checksum() == rhs.checksum())) {
      //Checksum cs = lhs.signature();
      //printf("%s\n", cs.toString().c_str());
      //printf("%s and %s\n", lhs.local_path.c_str(), rhs.local_path.c_str());
      return false;
    }
    if_full++;
    return true;
  }
  return lhs.signaturePart(bytes) == rhs.signaturePart(bytes) && lhs.checksumPart(bytes) == rhs.checksumPart(bytes);
}
