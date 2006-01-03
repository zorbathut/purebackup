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

class Item {
public:
  int type;
  long long size;
  long long timestamp;

  string local_path;

  Checksum checksum() const;
  Checksum checksumPart(int len) const;

  void setTotalChecksum(const Checksum &chs);

private:
  mutable vector<pair<int, Checksum> > css;
};

#endif
