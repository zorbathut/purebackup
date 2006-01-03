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

#include "tree.h"
#include "debug.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <vector>
#include <set>

using namespace std;

bool MountTree::checkSanity() const {
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
    if(!file_scanned)
      CHECK(file_links.size() == 0);
    CHECK(file_source.size());
    set<string> nlk;
    for(int i = 0; i < file_links.size(); i++) {
      nlk.insert(file_links[i].first);
      if(!file_links[i].second.checkSanity())
        return false;
    }
    CHECK(nlk.size() == file_links.size());
  } else {
    CHECK(file_source.size() == 0);
  }
  
  if(type == MTT_ITEM) {
    checked = true;
    CHECK(item_fullpath.size());
  } else {
    CHECK(item_fullpath.size() == 0);
  }
  
  CHECK(checked);
  
  return true;
}

void MountTree::print(int indent) const {
  string spacing(indent, ' ');
  if(type == MTT_VIRTUAL) {
    for(map<string, MountTree>::const_iterator itr = virtual_links.begin(); itr != virtual_links.end(); itr++) {
      printf("%s%s\n", spacing.c_str(), itr->first.c_str());
      itr->second.print(indent + 2);
    }
  } else if(type == MTT_FILE) {
    if(file_scanned) {
      for(int i = 0; i < file_links.size(); i++) {
        printf("%s%s\n", spacing.c_str(), file_links[i].first.c_str());
        file_links[i].second.print(indent + 2);
      }
    }
  } else if(type == MTT_ITEM) {
  } else {
    CHECK(0);
  }
}

struct DirListOut {
public:
  bool directory;
  string full_path;
  string itemname;
  long long size; // -1 if unknown
  long long timestamp; // -1 if unknown
};

vector<DirListOut> getDirList(const string &path) {
  vector<DirListOut> rv;
  DIR *od = opendir(path.c_str());
  CHECK(od);
  dirent *dire;
  while(dire = readdir(od)) {
    if(strcmp(dire->d_name, ".") == 0 || strcmp(dire->d_name, "..") == 0)
      continue;
    struct stat stt;
    CHECK(!stat((path + "/" + dire->d_name).c_str(), &stt));
    DirListOut dlo;
    dlo.directory = stt.st_mode & S_IFDIR;
    dlo.full_path = path + "/" + dire->d_name;
    dlo.itemname = dire->d_name;
    dlo.size = stt.st_size;
    dlo.timestamp = stt.st_mtime;
    rv.push_back(dlo);
  }
  return rv;
}

void MountTree::scan() {
  if(type == MTT_VIRTUAL) {
    for(map<string, MountTree>::iterator itr = virtual_links.begin(); itr != virtual_links.end(); itr++) {
      itr->second.scan();
    }
  } else if(type == MTT_FILE) {
    if(!file_scanned) {
      file_scanned = true;
      vector<DirListOut> fils = getDirList(file_source);
      for(int i = 0; i < fils.size(); i++) {
        if(fils[i].directory) {
          MountTree mt;
          mt.type = MTT_FILE;
          mt.file_source = fils[i].full_path;
          mt.file_scanned = false;
          file_links.push_back(make_pair(fils[i].itemname, mt));
          file_links.back().second.scan();
        } else {
          MountTree mt;
          mt.type = MTT_ITEM;
          mt.item_fullpath = fils[i].full_path;
          mt.item_size = fils[i].size;
          mt.item_timestamp = fils[i].timestamp;
          file_links.push_back(make_pair(fils[i].itemname, mt));
        }
      }
    }
  } else {
    CHECK(0);
  }
}

void MountTree::dumpItems(map<string, Item> *items, string cpath) const {
  if(type == MTT_VIRTUAL) {
    for(map<string, MountTree>::const_iterator itr = virtual_links.begin(); itr != virtual_links.end(); itr++) {
      itr->second.dumpItems(items, cpath + "/" + itr->first);
    }
  } else if(type == MTT_FILE) {
    if(file_scanned) {
      for(int i = 0; i < file_links.size(); i++) {
        file_links[i].second.dumpItems(items, cpath + "/" + file_links[i].first);
      }
    }
  } else if(type == MTT_ITEM) {
    Item nit;
    nit.type = MTI_LOCAL;
    nit.size = item_size;
    nit.timestamp = item_timestamp;
    nit.local_path = item_fullpath;
    CHECK(!items->count(cpath));
    (*items)[cpath] = nit;
  } else {
    CHECK(0);
  }
}

static MountTree mt_root;

MountTree *getRoot() {
  return &mt_root;
}
