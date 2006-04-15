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

#ifndef PUREBACKUP_UTIL
#define PUREBACKUP_UTIL

#include <string>
#include <vector>

using namespace std;

class Checksum {
public:
  unsigned char bytes[20];

  unsigned char signature[32];   // A chunk in the middle, rounded down, or the entire file with 0's appended.

  string toString() const;
};

bool operator==(const Checksum &lhs, const Checksum &rhs);
inline bool operator!=(const Checksum &lhs, const Checksum &rhs) { return !(lhs == rhs); }

long long atoll(const char *);
Checksum atochecksum(const char *);

string StringPrintf( const char *bort, ... ) __attribute__((format(printf,1,2)));

struct DirListOut {
public:
  bool directory;
  bool null;
  string full_path;
  string itemname;
  long long size;
  long long timestamp;
};

pair<bool, vector<DirListOut> > getDirList(const string &path);

#endif
