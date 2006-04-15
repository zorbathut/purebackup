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

#ifndef PUREBACKUP_ITEM
#define PUREBACKUP_ITEM

#include <string>
#include <vector>
#include <set>

#ifdef WIN32API
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define NOMINMAX
#include <windows.h>
#endif

#include "util.h"

using namespace std;

enum { MTI_ORIGINAL, MTI_LOCAL, MTI_SSH, MTI_NONEXISTENT, MTI_END };

class kvData;

class Metadata {
public:
  long long timestamp;
  /* permissions and stuff */

  string toKvd() const;

  Metadata() { };
  Metadata(long long in_timestamp) : timestamp(in_timestamp) { };
};

Metadata metaParseFromKvd(kvData kvd);

inline bool operator==(const Metadata &lhs, const Metadata &rhs) {
  return lhs.timestamp == rhs.timestamp;
}

inline bool operator!=(const Metadata &lhs, const Metadata &rhs) {
  return !(lhs.timestamp == rhs.timestamp);
}

class ItemShunt {
public:
  void seek(long long pos);
  int read(char *buffer, int len);

  ~ItemShunt();

  static ItemShunt* LocalFile(const string &fname);

private:  

#ifdef WIN32API
  HANDLE local_file;
#else
  FILE *local_file;
#endif

  ItemShunt();
  ItemShunt(const ItemShunt &is); // do not implement
  void operator=(const ItemShunt &is); // do not implement
};

class Item {
public:
  
  long long size() const { return p_size; }
  const Metadata &metadata() const { return p_metadata; }

  ItemShunt *open() const;

  Checksum checksum() const;
  Checksum checksumPart(long long len) const;
  
  Checksum signature() const;
  Checksum signaturePart(long long len) const;  // Same as a checksum, but with the checksum part 0'ed.
  
  void addVersion(int x);
  const set<int> &getVersions() const;

  bool exists() const { return type != MTI_NONEXISTENT; }
  bool isReadable() const;
  bool isChecksummable() const;

  string toString() const;

  static Item MakeLocal(const string &full_path, long long size, const Metadata &meta);
  static Item MakeSsh(const string &user, const string &pass, const string &host, const string &full_path, long long size, const Metadata &meta);
  static Item MakeOriginal(long long size, const Metadata &meta, const Checksum &checksum, const set<int> &versions);
  
  Item();

private:
  mutable vector<pair<int, Checksum> > css;
  mutable vector<pair<int, Checksum> > sss;

  int type;
  long long p_size;
  Metadata p_metadata;

  mutable int readable;  // 0 for not readable, 1 for readable, -1 for unknown

  string local_path;

  string ssh_user;
  string ssh_pass;
  string ssh_host;
  string ssh_path;

  set<int> needed_versions;

};

inline bool operator==(const Item &lhs, const Item &rhs) {
  return lhs.size() == rhs.size() && lhs.metadata() == rhs.metadata() && lhs.checksum() == rhs.checksum();
}

// Various optimizations possible
bool identicalFile(const Item &lhs, const Item &rhs, long long bytes = -1);

#endif
