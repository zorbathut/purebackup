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

#ifndef PUREBACKUP_TREE
#define PUREBACKUP_TREE

#include <string>
#include <map>
#include <vector>

#include "item.h"

using namespace std;

enum { MTT_VIRTUAL, MTT_IMPLIED, MTT_MASKED, MTT_FILE, MTT_SSH, MTT_ITEM, MTT_END, MTT_UNINITTED };

class MountTree {
public:
  int type;
  
  map<string, MountTree> links;

  string file_source;
  bool file_scanned;

  string ssh_user;
  string ssh_pass;
  string ssh_host;
  string ssh_source;
  bool ssh_scanned;

  Item item;

  bool checkSanity() const;
  
  void print(int indent) const;
  
  void scan();

  void dumpItems(map<string, Item> *items, string cpath) const;

  MountTree() {
    type = MTT_UNINITTED;
  }
};

MountTree *getRoot();

#endif
