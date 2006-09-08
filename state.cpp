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
  while(getkvDataInline(ifs, kvd)) {
    if(kvd.category == "file") {
      string itemname = kvd.consume("name");
      vector<int> depend = sti(tokenize(kvd.consume("dependencies"), " "));
      CHECK(!items.count(itemname));
      items[itemname] = Item::MakeOriginal(atoll(kvd.consume("size").c_str()), atoll(kvd.consume("timestamp").c_str()), atochecksum(kvd.consume("checksum").c_str()), set<int>(depend.begin(), depend.end()));
    } else {
      CHECK(0);
    }
  }
  CHECK(kvd.isDone());
}

const map<string, Item> &State::getItemDb() const {
  return items;
}

const Item *State::findItem(const string &name) const {
  if(items.count(name))
    return &items.find(name)->second;
  return NULL;
}


void State::process(const Instruction &in, int tversion) {
  if(in.type == TYPE_CREATE) {
    CHECK(0);
  } else if(in.type == TYPE_ROTATE) {
    vector<Item> srcs;
    for(int i = 0; i < in.rotate_paths.size(); i++) {
      CHECK(items.count(in.rotate_paths[i].first));
      srcs.push_back(Item::MakeOriginal(items[in.rotate_paths[i].first].size(), in.rotate_paths[i].second, items[in.rotate_paths[i].first].checksum(), items[in.rotate_paths[i].first].getVersions()));
      srcs.back().addVersion(tversion);
      //printf("%lld %s\n", srcs.back().metadata.timestamp, srcs.back().checksum().toString().c_str());
    }
    for(int i = 0; i < in.rotate_paths.size(); i++)
      items[in.rotate_paths[(i + 1) % in.rotate_paths.size()].first] = srcs[i];
    for(int i = 0; i < in.rotate_paths.size(); i++)
      items[in.rotate_paths[i].first].addVersion(tversion);
  } else if(in.type == TYPE_DELETE) {
    CHECK(items.count(in.delete_path));
    items.erase(items.find(in.delete_path));
  } else if(in.type == TYPE_COPY) {
    //dprintf("Copying from %s\n", in.copy_source.c_str());
    //dprintf("%d\n", items.count(in.copy_source));
    CHECK(items.count(in.copy_source));
    items[in.copy_dest] = Item::MakeOriginal(items[in.copy_source].size(), in.copy_dest_meta, items[in.copy_source].checksum(), items[in.copy_source].getVersions());
    items[in.copy_dest].addVersion(tversion);
  } else if(in.type == TYPE_APPEND) {
    CHECK(items.count(in.append_path));
    items[in.append_path] = Item::MakeOriginal(in.append_size, in.append_meta, in.append_checksum, items[in.append_path].getVersions());
    items[in.append_path].addVersion(tversion);
  } else if(in.type == TYPE_STORE) {
    if(items.count(in.store_path))
      items.erase(items.find(in.store_path));
    items[in.store_path] = Item::MakeOriginal(in.store_size, in.store_meta, in.store_source->checksumPart(in.store_size), set<int>());
    items[in.store_path].addVersion(tversion);
  } else if(in.type == TYPE_TOUCH) {
    CHECK(items.count(in.touch_path));
    items[in.touch_path] = Item::MakeOriginal(items[in.touch_path].size(), in.touch_meta, items[in.touch_path].checksum(), items[in.touch_path].getVersions());
    // We're not going to add a version entry because the client can pull that data straight out of the information file
  } else {
    CHECK(0);
  }
}

void State::writeOut(const string &fn) const {
  ofstream ofs(fn.c_str());
  for(map<string, Item>::const_iterator itr = items.begin(); itr != items.end(); itr++) {
    //dprintf("Processing %s\n", itr->first.c_str());
    kvData kvd;
    kvd.category = "file";
    kvd.kv["name"] = itr->first;
    kvd.kv["size"] = StringPrintf("%lld", itr->second.size());
    kvd.kv["timestamp"] = StringPrintf("%lld", itr->second.metadata().timestamp);
    kvd.kv["checksum"] = itr->second.checksum().toString();
    {
      const set<int> &vers = itr->second.getVersions();
      string vs;
      for(set<int>::const_iterator itr = vers.begin(); itr != vers.end(); itr++) {
        if(itr != vers.begin())
          vs += " ";
        vs += StringPrintf("%d", *itr);
      }
      kvd.kv["dependencies"] = vs;
    }
    putkvDataInline(ofs, kvd, "name");
  }
}


void dumpa(string *str, const string &txt, const vector<pair<bool, string> > &vek) {
  *str += "  " + txt + "\n";
  for(int i = 0; i < vek.size(); i++)
    *str += StringPrintf("    %d:%s\n", vek[i].first, vek[i].second.c_str());
}

string Instruction::textout() const {
  string rvx;
  CHECK(type >= 0 && type < TYPE_END);
  rvx = type_strs[type] + "\n";
  dumpa(&rvx, "Depends:", depends);
  dumpa(&rvx, "Removes:", removes);
  dumpa(&rvx, "Creates:", creates);
  return rvx;
}

string Instruction::processString() const {
  kvData kvd;
  if(type == TYPE_CREATE) {
    CHECK(0);
  } else if(type == TYPE_ROTATE) {
    kvd.category = "rotate";
    for(int i = 0; i < rotate_paths.size(); i++) {
      kvd.kv[StringPrintf("src%02ddst%02d", i, (i + rotate_paths.size() - 1) % rotate_paths.size())] = rotate_paths[i].first;
      kvd.kv[StringPrintf("meta%02d", i)] = rotate_paths[i].second.toKvd();
    }
  } else if(type == TYPE_DELETE) {
    kvd.category = "delete";
    kvd.kv["path"] = delete_path;
  } else if(type == TYPE_COPY) {
    kvd.category = "copy";
    kvd.kv["source"] = copy_source;
    kvd.kv["dest"] = copy_dest;
    kvd.kv["dest_meta"] = copy_dest_meta.toKvd();
  } else if(type == TYPE_APPEND) {
    kvd.category = "touch";
    kvd.kv["path"] = append_path;
    kvd.kv["meta"] = append_meta.toKvd();
    // TODO: Checksum?
  } else if(type == TYPE_STORE) {
    kvd.category = "touch";
    kvd.kv["path"] = store_path;
    kvd.kv["meta"] = store_meta.toKvd();
    // TODO: Checksum?
  } else if(type == TYPE_TOUCH) {
    kvd.category = "touch";
    kvd.kv["path"] = touch_path;
    kvd.kv["meta"] = touch_meta.toKvd();
  } else {
    CHECK(0);
  }
  return putkvDataInlineString(kvd);
}

const long long Instruction::size() const {
  if(type == TYPE_STORE) {
    return usedperitem + store_size;
  } else if(type == TYPE_APPEND) {
    return usedperitem + append_size - append_begin;
  } else {
    return usedperitem;
  }
}
