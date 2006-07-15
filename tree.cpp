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

#include <vector>
#include <set>

using namespace std;

bool MountTree::checkSanity() const {
  CHECK(type >= 0 && type <= MTT_END);
  
  bool checked = false;
  
  if(type == MTT_VIRTUAL || type == MTT_FILE || type == MTT_IMPLIED) {
    checked = true;
    for(map<string, MountTree>::const_iterator itr = links.begin(); itr != links.end(); itr++) {
      if(itr->first == "." || itr->first == "..")
        return false;
      if(!itr->second.checkSanity())
        return false;
      if(type == MTT_VIRTUAL) {
        CHECK(itr->second.type == MTT_VIRTUAL || itr->second.type == MTT_FILE || itr->second.type == MTT_SSH || itr->second.type == MTT_NULL);
      } else if(type == MTT_FILE) {
        CHECK(itr->second.type == MTT_FILE || itr->second.type == MTT_ITEM || itr->second.type == MTT_MASKED || itr->second.type == MTT_IMPLIED || itr->second.type == MTT_NULL);
      } else if(type == MTT_IMPLIED) {
        CHECK(itr->second.type == MTT_MASKED || itr->second.type == MTT_IMPLIED);
      } else {
        CHECK(0);
      }
    }
  } else {
    CHECK(links.size() == 0);
  }
  
  if(type == MTT_MASKED) {
    checked = true;
  } else {
  }
  
  if(type == MTT_ITEM) {
    checked = true;
    CHECK(item.exists());
  } else {
    CHECK(!item.exists());
  }

  if(type == MTT_SSH) {
    checked = true;
    CHECK(ssh_user.size());
    CHECK(ssh_pass.size());
    CHECK(ssh_host.size());
    CHECK(ssh_source.size());
  } else {
    CHECK(!ssh_user.size());
    CHECK(!ssh_pass.size());
    CHECK(!ssh_host.size());
    CHECK(!ssh_source.size());
  }
  
  if(type == MTT_NULL) {
    checked = true;
  } else {
  }
  
  CHECK(checked);
  
  return true;
}

void MountTree::print(int indent) const {
  string spacing(indent, ' ');
  if(type == MTT_VIRTUAL || type == MTT_FILE || type == MTT_SSH) {
    for(map<string, MountTree>::const_iterator itr = links.begin(); itr != links.end(); itr++) {
      printf("%s%s\n", spacing.c_str(), itr->first.c_str());
      itr->second.print(indent + 2);
    }
  } else if(type == MTT_ITEM) {
  } else if(type == MTT_NULL) {
    printf("%sNULL\n", spacing.c_str());
  } else if(type == MTT_IMPLIED) {
    for(map<string, MountTree>::const_iterator itr = links.begin(); itr != links.end(); itr++) {
      printf("%s%s\n", spacing.c_str(), itr->first.c_str());
      itr->second.print(indent + 2);
    }
  } else if(type == MTT_MASKED) {
    printf("%sCULLED\n", spacing.c_str());
  } else {
    CHECK(0);
  }
}

int scanned = 0;

void MountTree::scan() {
  if(type == MTT_VIRTUAL) {
    for(map<string, MountTree>::iterator itr = links.begin(); itr != links.end(); itr++) {
      itr->second.scan();
    }
  } else if(type == MTT_FILE) {
    CHECK(!file_scanned);
  
    file_scanned = true;
    pair<bool, vector<DirListOut> > tfils = getDirList(file_source);
    if(tfils.first) {
      *this = MountTree();
      type = MTT_NULL;
      return;
    }
    vector<DirListOut> fils = tfils.second;
    for(int i = 0; i < fils.size(); i++) {
      if(links[fils[i].itemname].type == MTT_MASKED)
        continue;
      scanned++;
      if(scanned % 1000 == 0) {
        printf("%d scanned\r", scanned);
        fflush(stdout);
      }
      if(links[fils[i].itemname].type != MTT_UNINITTED && links[fils[i].itemname].type != MTT_IMPLIED) {
        printf("Type is %d at %s which is WRONG\n", links[fils[i].itemname].type, fils[i].full_path.c_str());
        CHECK(0);
      }
      if(fils[i].null) {
        links[fils[i].itemname] = MountTree();    // we need to obliterate this entirely
        links[fils[i].itemname].type = MTT_NULL;  // TODO: Maybe we don't want to, if there's masked stuff beneath it?
      } else if(fils[i].directory) {      
        links[fils[i].itemname].type = MTT_FILE;
        links[fils[i].itemname].file_source = fils[i].full_path;
        links[fils[i].itemname].file_scanned = false;
        links[fils[i].itemname].scan();
      } else {
        links[fils[i].itemname].type = MTT_ITEM;
        links[fils[i].itemname].item = Item::MakeLocal(fils[i].full_path, fils[i].size, Metadata(fils[i].timestamp));
      }
    }
  } else if(type == MTT_SSH) {
    CHECK(0);
  } else {
    CHECK(0);
  }
}

void MountTree::dumpItems(map<string, Item> *items, string cpath) const {
  if(type == MTT_VIRTUAL || type == MTT_FILE) {
    for(map<string, MountTree>::const_iterator itr = links.begin(); itr != links.end(); itr++) {
      itr->second.dumpItems(items, cpath + "/" + itr->first);
    }
  } else if(type == MTT_ITEM) {
    CHECK(!items->count(cpath));
    (*items)[cpath] = item;
  } else if(type == MTT_MASKED || type == MTT_NULL) {
  } else {
    printf("Type is %d at %s\n", type, cpath.c_str());
    CHECK(0);
  }
}

static MountTree mt_root;

MountTree *getRoot() {
  return &mt_root;
}
