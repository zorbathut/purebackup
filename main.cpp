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

enum { SRC_PRESERVE, SRC_APPEND, SRC_COPYFROM, SRC_NEW };

class Source {
public:
  int type;
  
  Item *link; // what to append from for SRC_APPEND, what to copy from for SRC_COPYFROM
};

int main() {
  readConfig("purebackup.conf");
  scanPaths();
  
  vector<Item> realitems;
  getRoot()->dumpItems(&realitems, "");
  dprintf("%d items found\n", realitems.size());
  
  for(int i = 0; i < realitems.size(); i++) {
    CHECK(realitems[i].size >= 0);
    CHECK(realitems[i].timestamp >= 0);
  }
  
  //for(int i = 0; i < realitems.size(); i++)
    //printf("%s == %s, %lld\n", realitems[i].name.c_str(), realitems[i].local_path.c_str(), realitems[i].size);
  
  // Here we read in the file of existing data
  vector<Item> origitems;
  
  map<long long, vector<Item *> > sizelinks;
  //map<string, Item *> orignamelinks;
  
  //for(int i = 0; i < origitems.size(); i++)
    //orignamelinks[origitems[i].name] = &origitems[i];
  
  vector<Source> sources; // This parallels realitems

  for(int i = 0; i < realitems.size(); i++) {
    bool got = false;
    
    // First, we check to see if it's the same file as is in origitems
    if(!got) {
    }
    
    // Then, we check to see if it's the same file only appended to
    if(!got) {
    }
    
    // Next we see if we can copy it from any existing place
    if(!got) {
      const vector<Item *> &sli = sizelinks[realitems[i].size];
      for(int k = 0; k < sli.size(); k++) {
        CHECK(realitems[i].size == sli[k]->size);
        printf("Comparing %s and %s\n", realitems[i].local_path.c_str(), sli[k]->local_path.c_str());
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
  
  
  
  /*
  for(int i = 0; i < sources.size(); i++) {
    if(sources[i].type == 2)
      printf("%s equals %s\n", realitems[i].local_path.c_str(), sources[i].link->local_path.c_str());
  }*/

}
