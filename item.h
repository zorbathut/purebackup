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

#include "util.h"

using namespace std;

enum { MTI_ORIGINAL, MTI_LOCAL, MTI_NONEXISTENT, MTI_END };

class Metadata {
public:
  long long timestamp;
  /* permissions and stuff */

  string toKvd() const;

  Metadata() { };
  Metadata(long long in_timestamp) : timestamp(in_timestamp) { };
};

inline bool operator==(const Metadata &lhs, const Metadata &rhs) {
  return lhs.timestamp == rhs.timestamp;
}

inline bool operator!=(const Metadata &lhs, const Metadata &rhs) {
  return !(lhs.timestamp == rhs.timestamp);
}

class ItemShunt {
public:
  FILE *local_file;

  void seek(long long pos);
  int read(char *buffer, int len);

  ItemShunt();
  ~ItemShunt();

private:
  ItemShunt(const ItemShunt &is); // do not implement
  void operator=(const ItemShunt &is); // do not implement
};

class Item {
public:
  
  long long size() const { return p_size; }
  const Metadata &metadata() const { return p_metadata; }

  ItemShunt *open() const;

  Checksum checksum() const;
  Checksum checksumPart(int len) const;

  bool exists() const { return type != MTI_NONEXISTENT; }

  string toString() const;

  static Item MakeLocal(const string &full_path, long long size, const Metadata &meta);
  static Item MakeOriginal(long long size, const Metadata &meta, const Checksum &checksum);
  
  Item();

private:
  mutable vector<pair<int, Checksum> > css;

  int type;
  long long p_size;
  Metadata p_metadata;

  string local_path;
};

inline bool operator==(const Item &lhs, const Item &rhs) {
  return lhs.size() == rhs.size() && lhs.metadata() == rhs.metadata() && lhs.checksum() == rhs.checksum();
}

#endif
