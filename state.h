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

enum { SRC_PRESERVE, SRC_APPEND, SRC_COPYFROM, SRC_NEW };

class Source {
public:
  int type;
  
  Item *link; // what to append from for SRC_APPEND, what to copy from for SRC_COPYFROM
};

class State {
private:
  
  map<string, Item> items;

public:
  
  void readFile(const string &fil);

  const Item *findItem(const string &name) const;
  void process(const Item &dst, const Source &in);

  void writeOut(const string &fil) const;

};

#endif
