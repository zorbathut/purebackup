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

#ifndef PUREBACKUP_STATE
#define PUREBACKUP_STATE

#include "item.h"

#include <map>

using namespace std;

enum { TYPE_CREATE, TYPE_ROTATE, TYPE_DELETE, TYPE_COPY, TYPE_APPEND, TYPE_STORE, TYPE_TOUCH, TYPE_END };
const string type_strs[] = { "CREATE", "ROTATE", "DELETE", "COPY", "APPEND", "STORE", "TOUCH" };

class Instruction {
public:
  vector<pair<bool, string> > depends;
  vector<pair<bool, string> > removes;
  vector<pair<bool, string> > creates;

  int type;

  string create_path;
  Metadata create_meta;

  vector<pair<string, Metadata> > rotate_paths; // [0].first becomes [n-1], [1].first becomes [0], [2].first becomes [1]

  string delete_path;

  string copy_source;
  string copy_dest;
  Metadata copy_dest_meta;

  string append_path;
  long long append_size;
  Metadata append_meta;
  Checksum append_checksum;

  string store_path;
  long long store_size;
  Metadata store_meta;
  Checksum store_checksum;
  
  string touch_path;
  Metadata touch_meta;

  string textout() const;
};

class State {
private:
  
  map<string, Item> items;

public:
  
  void readFile(const string &fil);

  void process(const Instruction &inst);

  const map<string, Item> &getItemDb() const;
  const Item *findItem(const string &name) const;

  void writeOut(const string &fil) const;

};

#endif
