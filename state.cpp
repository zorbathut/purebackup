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

#include "state.h"

#include "parse.h"
#include "debug.h"

#include <fstream>

using namespace std;

void State::readFile(const string &fil) {
  ifstream ifs(fil.c_str());
  kvData kvd;
  while(getkvData(ifs, kvd)) {
    if(kvd.category == "file") {
      Item item;
      item.type = MTI_ORIGINAL;
      item.size = atoll(kvd.consume("size").c_str());
      item.timestamp = atoll(kvd.consume("timestamp").c_str());
      item.setTotalChecksum(atochecksum(kvd.consume("checksum_sha1").c_str()));
      string itemname = kvd.consume("name");
      CHECK(!items.count(itemname));
      items[itemname] = item;
    } else {
      CHECK(0);
    }
  }
}

const Item *State::findItem(const string &name) const {
  if(items.count(name))
    return &items.find(name)->second;
  return NULL;
}

/*
void State::process(const Instruction &in, const Item *srcdata) {
  if(in.type == SRC_APPEND) {
    CHECK(srcdata);
    CHECK(srcdata->name == in.dst);
    CHECK(items.count(in.dst));
    Item &onto = items[in.dst];
    CHECK(onto.name == in.dst);
    CHECK(onto.size < srcdata->size);
    CHECK(srcdata->checksumPart(onto.size) == onto.checksum());
    onto = *srcdata;
  } else if(in.type == SRC_COPYFROM) {
    CHECK(srcdata);
    CHECK(srcdata->name == in.copyfrom_src);
    CHECK(!items.count(in.dst));
    Item &onto = items[in.dst];
    CHECK(onto.name == in.dst);
    CHECK(onto.size < srcdata->size);
    CHECK(srcdata->checksumPart(onto.size) == onto.checksum());
    onto = *srcdata;
    
    CHECK(!items.count(dst.name));
    CHECK(in.link->checksum() == dst.checksum());
    CHECK(in.link->size == dst.size);
    items[dst.name] = dst;
  } else if(in.type == SRC_NEW) {
    CHECK(!items.count(dst.name));
    items[dst.name] = dst;
  } else if(in.type == SRC_DELETE) {
    CHECK(!items.count(dst.name));
    items[dst.name] = dst;
  } else {
    CHECK(0);
  }
}
*/

void State::writeOut(const string &fn) const {
  FILE *fil = fopen(fn.c_str(), "w");
  for(map<string, Item>::const_iterator itr = items.begin(); itr != items.end(); itr++) {
    fprintf(fil, "file {\n");
    fprintf(fil, "  name=%s\n", itr->first.c_str());
    fprintf(fil, "  size=%lld\n", itr->second.size);
    fprintf(fil, "  timestamp=%lld\n", itr->second.timestamp);
    fprintf(fil, "  checksum_sha1=%s\n", itr->second.checksum().toString().c_str());
    fprintf(fil, "}\n");
    fprintf(fil, "\n");
  }
  fclose(fil);
}

