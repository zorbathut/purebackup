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

#include "parse.h"
#include "debug.h"
#include "tree.h"
#include "state.h"

#include <string>
#include <fstream>

using namespace std;

// Makes sure everything up to this is virtual
MountTree *getMountpointLocation(const string &loc) {
  const char *tpt = loc.c_str();
  MountTree *cpos = getRoot();
  while(*tpt) {
    CHECK(*tpt == '/');
    tpt++;
    CHECK(*tpt);
    
    CHECK(cpos->type == MTT_UNINITTED || cpos->type == MTT_VIRTUAL);
    cpos->type = MTT_VIRTUAL;
    
    // isolate the next path component
    const char *tpn = strchr(tpt, '/');
    if(!tpn)
      tpn = tpt + strlen(tpt);
    
    string linkage = string(tpt, tpn);
    cpos = &cpos->virtual_links[linkage];
    
    tpt = tpn;
  }
  return cpos;
}

void createMountpoint(const string &loc, const string &type, const string &source) {
  MountTree *dpt =getMountpointLocation(loc);
  CHECK(dpt);
  CHECK(dpt->type == MTT_UNINITTED);
  
  if(type == "file") {
    dpt->type = MTT_FILE;
    dpt->file_source = source;
    dpt->file_scanned = false;
  } else {
    CHECK(0);
  }
  
}

void printAll() {
  printf("ROOT\n");
  getRoot()->print(2);
}

void readConfig(const string &conffile) {
  // First we init root
  {
    getRoot()->type = MTT_UNINITTED;
  }
  
  ifstream ifs(conffile.c_str());
  kvData kvd;
  while(getkvData(ifs, kvd)) {
    if(kvd.category == "mountpoint") {
      createMountpoint(kvd.consume("mount"), kvd.consume("type"), kvd.consume("source"));
    } else {
      CHECK(0);
    }
  }
  
  CHECK(getRoot()->checkSanity());
  printAll();
}

void scanPaths() {
  getRoot()->scan();
  CHECK(getRoot()->checkSanity());
  //printAll();
}

int main() {
  readConfig("purebackup.conf");
  scanPaths();
  
  map<string, Item> realitems;
  getRoot()->dumpItems(&realitems, "");
  dprintf("%d items found\n", realitems.size());
  
  for(map<string, Item>::iterator itr = realitems.begin(); itr != realitems.end(); itr++) {
    CHECK(itr->second.size >= 0);
    CHECK(itr->second.timestamp >= 0);
  }
  
  /*
  State origstate;
  origstate.readFile("state0");
  
  // Here we read in the file of existing data
  vector<Item> origitems;
  
  map<long long, vector<Item *> > sizelinks;
  
  //for(int i = 0; i < origitems.size(); i++)
    //orignamelinks[origitems[i].name] = &origitems[i];
  
  vector<Instruction> sources;

  for(int i = 0; i < realitems.size(); i++) {
    bool got = false;
    
    // First, we check to see if it's the same file as is in origitems
    if(!got) {
      const Item *it = origstate.findItem(realitems[i].name);
      if(it && it->size == realitems[i].size && it->timestamp == realitems[i].timestamp) {
        Source src;
        src.type = SRC_PRESERVE;
        sources.push_back(src);
        got = true;
      }
    }
    
    // Then, we check to see if it's the same file only appended to
    if(!got) {
      const Item *it = origstate.findItem(realitems[i].name);
      if(it && it->size < realitems[i].size && realitems[i].checksumPart(it->size) == it->checksum()) {
        Source src;
        src.type = SRC_APPEND;
        sources.push_back(src);
        got = true;
      }
    }
    
    // Next we see if we can copy it from any existing place
    if(!got) {
      const vector<Item *> &sli = sizelinks[realitems[i].size];
      for(int k = 0; k < sli.size(); k++) {
        CHECK(realitems[i].size == sli[k]->size);
        if(realitems[i].checksum() == sli[k]->checksum()) {
          Source src;
          src.type = SRC_COPYFROM;
          src.link = sli[k];
          sources.push_back(src);
          got = true;
          break;
        }
      }
    }
    
    // Then, as a last resort, we simply copy it from its current location.
    if(!got) {
      Source src;
      src.type = SRC_NEW;
      sources.push_back(src);
      got = true;
    }
    
    CHECK(got);
    sizelinks[realitems[i].size].push_back(&realitems[i]);
  }
  
  // Here is where we would search for deleted files

  {
    int same = 0;
    int append = 0;
    int copy = 0;
    int create = 0;
    int del = 0;
    for(int i = 0; i < sources.size(); i++) {
      if(sources[i].type == SRC_PRESERVE)
        same++;
      if(sources[i].type == SRC_APPEND)
        append++;
      if(sources[i].type == SRC_COPYFROM) {
        printf("Copying %s from %s\n", realitems[i].name.c_str(), sources[i].link->name.c_str());
        copy++;
      }
      if(sources[i].type == SRC_NEW) {
        printf("Creating %s\n", realitems[i].name.c_str());
        create++;
      }
    }
    printf("%d preserved, %d appended, %d copied, %d new, %d deleted\n", same, append, copy, create, del);
  }
  
  State newstate = origstate;
  for(int i = 0; i < sources.size(); i++)
    newstate.process(realitems[i], sources[i]);
  
  /*State stt;
  for(int i = 0; i < sources.size(); i++)
    stt.process(realitems[i], sources[i]);
  
  stt.writeOut("state1");*/

}
