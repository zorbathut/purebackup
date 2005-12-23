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

#include <string>
#include <fstream>

using namespace std;

enum { MTT_VIRTUAL, MTT_FILE, MTT_END, MTT_UNINITTED };

class MountTree {
public:
  int type;
  
  map<string, MountTree> virtual_links;

  string file_source;

  bool checkSanity() const {
    CHECK(type >= 0 && type <= MTT_END);
    
    bool checked = false;
    
    if(type == MTT_VIRTUAL) {
      checked = true;
      for(map<string, MountTree>::const_iterator itr = virtual_links.begin(); itr != virtual_links.end(); itr++) {
        if(itr->first == "." || itr->first == "..")
          return false;
        if(!itr->second.checkSanity())
          return false;
      }
    } else {
      CHECK(virtual_links.size() == 0);
    }
    
    if(type == MTT_FILE) {
      checked = true;
      CHECK(file_source.size());
    } else {
      CHECK(file_source.size() == 0);
    }
    
    CHECK(checked);
    
    return true;
  }

  MountTree() {
    type = MTT_UNINITTED;
  }
};

MountTree mt_root;

// Makes sure everything up to this is virtual
MountTree *getMountpointLocation(const string &loc) {
  const char *tpt = loc.c_str();
  MountTree *cpos = &mt_root;
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
  } else {
    CHECK(0);
  }
  
}

void readConfig(const string &conffile) {
  // First we init root
  {
    mt_root.type = MTT_UNINITTED;
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
  
  CHECK(mt_root.checkSanity());
  printf("Sanity checked\n");
}

int main() {
  readConfig("purebackup.conf");
}
