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
#include <set>

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

void loopprocess(int pos, vector<int> *loop, const vector<Instruction> &inst,
    const map<pair<bool, string>, vector<int> > &depon, 
    const map<pair<bool, string>, int> &creates,
    const set<pair<bool, string> > &removed,
    vector<bool> *noprob) {
      
  //printf("Entering loopprocess %d\n", pos);
  //printf("%s\n", inst[pos].textout().c_str());
      
  if((*noprob)[pos])
    return;
  
  if(loop->size() && loop->back() == pos)
    return;
  
  //printf("Pushback %d\n", pos);
  
  loop->push_back(pos);
  
  if(set<int>(loop->begin(), loop->end()).size() != loop->size()) {
    //printf("LOOP!\n");
    //for(int i = 0; i < loop->size(); i++)
      //printf("%d\n", (*loop)[i]);
    return;
  }
  
  //printf("Starting depends %d\n", pos);
  vector<int> bku = *loop;
  // Anything this depends on must exist
  for(int i = 0; i < inst[pos].depends.size(); i++) {
    CHECK(creates.count(inst[pos].depends[i]));
    loopprocess(creates.find(inst[pos].depends[i])->second, loop, inst, depon, creates, removed, noprob);
    if(*loop != bku)
      return;
  }
  
  //printf("Starting removes %d\n", pos);
  // Anything this removes must have its dependencies complete
  for(int i = 0; i < inst[pos].removes.size(); i++) {
    if(!depon.count(inst[pos].removes[i]))
      continue;
    const vector<int> &dpa = depon.find(inst[pos].removes[i])->second;
    for(int j = 0; j < dpa.size(); j++) {
      loopprocess(dpa[j], loop, inst, depon, creates, removed, noprob);
      if(*loop != bku)
        return;
    }
  }
  //printf("Done %d\n", pos);
  
  loop->pop_back();

  (*noprob)[pos] = true;
}

vector<Instruction> deloop(const vector<Instruction> &inst) {
  map<pair<bool, string>, vector<int> > depon;
  map<pair<bool, string>, int> creates;
  set<pair<bool, string> > removed;
  vector<bool> noprob(inst.size());
  for(int i = 0; i < inst.size(); i++) {
    for(int j = 0; j < inst[i].depends.size(); j++)
      depon[inst[i].depends[j]].push_back(i);
    for(int j = 0; j < inst[i].creates.size(); j++) {
      CHECK(!creates.count(inst[i].creates[j]));
      creates[inst[i].creates[j]] = i;
    }
    for(int j = 0; j < inst[i].removes.size(); j++) {
      CHECK(!removed.count(inst[i].removes[j]));
      removed.insert(inst[i].removes[j]);
    }
  }
  for(int i = 0; i < inst.size(); i++) {
    vector<int> loop;
    loopprocess(i, &loop, inst, depon, creates, removed, &noprob);
    if(loop.size()) {
      printf("Loop! %d\n", loop.size());
      CHECK(loop.size() >= 3);
      CHECK(loop.front() == loop.back());
      CHECK(set<int>(loop.begin(), loop.end()).size() == loop.size() - 1);
      
      loop.pop_back();
      
      Instruction rotinstr;
      rotinstr.type = TYPE_ROTATE;
      for(int i = 0; i < loop.size(); i++) {
        const Instruction &insta = inst[loop[i]];
        const Instruction &instb = inst[loop[(i+1) % loop.size()]];
        
        CHECK(insta.type == TYPE_COPY);
        CHECK(instb.type == TYPE_COPY);
        
        CHECK(instb.copy_source == insta.copy_dest);
        
        rotinstr.depends.insert(rotinstr.depends.end(), insta.depends.begin(), insta.depends.end());
        rotinstr.removes.insert(rotinstr.removes.end(), insta.removes.begin(), insta.removes.end());
        rotinstr.creates.insert(rotinstr.creates.end(), insta.creates.begin(), insta.creates.end());
        
        rotinstr.rotate_paths.push_back(make_pair(insta.copy_source, insta.copy_dest_meta));
      }
      CHECK((set<pair<bool, string> >(rotinstr.depends.begin(), rotinstr.depends.end()).size() == rotinstr.depends.size()));
      CHECK((set<pair<bool, string> >(rotinstr.removes.begin(), rotinstr.removes.end()).size() == rotinstr.removes.size()));
      CHECK((set<pair<bool, string> >(rotinstr.creates.begin(), rotinstr.creates.end()).size() == rotinstr.creates.size()));
      
      vector<Instruction> nistr;
      for(int i = 0; i < inst.size(); i++)
        if(!count(loop.begin(), loop.end(), i))
          nistr.push_back(inst[i]);
      nistr.push_back(rotinstr);
      return deloop(nistr);
    }
  }
  return inst;
}

vector<Instruction> sortInst(vector<Instruction> oinst) {
  oinst = deloop(oinst);
  
  vector<Instruction> buckets[TYPE_END];
  map<pair<bool, string>, int> leftToUse;
  for(int i = 0; i < oinst.size(); i++) {
    CHECK(oinst[i].type >= 0 && oinst[i].type < TYPE_END);
    buckets[oinst[i].type].push_back(oinst[i]);
    for(int j = 0; j < oinst[i].depends.size(); j++)
      leftToUse[oinst[i].depends[j]]++;
  }
  CHECK(buckets[0].size() == 1);
  
  set<pair<bool, string> > live;

  vector<Instruction> kinstr;
  
  while(1) {
    int tcl = 0;
    for(int i = 0; i < TYPE_END; i++)
      tcl += buckets[i].size();
    if(!tcl)
      break;
    printf("Starting pass, %d instructions left\n", tcl);
    for(int i = 0; i < TYPE_END; i++) {
      for(int j = 0; j < buckets[i].size(); j++) {
        bool good = true;
        for(int k = 0; good && k < buckets[i][j].depends.size(); k++)
          if(!live.count(buckets[i][j].depends[k]))
            good = false;
        for(int k = 0; good && k < buckets[i][j].removes.size(); k++)
          if(!(leftToUse[buckets[i][j].removes[k]] == 0 || leftToUse[buckets[i][j].removes[k]] == 1 && count(buckets[i][j].depends.begin(), buckets[i][j].depends.end(), buckets[i][j].removes[k])))
            good = false;
        if(!good)
          continue;
        //dprintf("%s\n", buckets[i][j].textout().c_str());
        for(int k = 0; k < buckets[i][j].depends.size(); k++)
          leftToUse[buckets[i][j].depends[k]]--;
        for(int k = 0; k < buckets[i][j].removes.size(); k++) {
          CHECK(live.count(buckets[i][j].removes[k]));
          live.erase(buckets[i][j].removes[k]);
        }
        for(int k = 0; k < buckets[i][j].creates.size(); k++) {
          pair<bool, string> ite = buckets[i][j].creates[k];
          CHECK(!live.count(ite));
          live.insert(buckets[i][j].creates[k]);
        }
        kinstr.push_back(buckets[i][j]);
        buckets[i].erase(buckets[i].begin() + j);
        j--;
      }
    }
    for(int i = 0; i < TYPE_END; i++)
      tcl -= buckets[i].size();
    CHECK(tcl != 0);
  }

  return kinstr;
}

int main() {
  readConfig("purebackup.conf");
  scanPaths();
  
  map<string, Item> realitems;
  getRoot()->dumpItems(&realitems, "");
  dprintf("%d items found\n", realitems.size());
  
  State origstate;
  origstate.readFile("state0");
  
  map<pair<bool, string>, Item> citem;
  map<long long, vector<pair<bool, string> > > citemsizemap;
  set<string> ftc;
  
  Instruction fi;
  fi.type = TYPE_CREATE;
  
  vector<Instruction> inst;
  
  for(map<string, Item>::iterator itr = realitems.begin(); itr != realitems.end(); itr++) {
    CHECK(itr->second.size >= 0);
    CHECK(itr->second.metadata.timestamp >= 0);
    ftc.insert(itr->first);
  }
  
  for(map<string, Item>::const_iterator itr = origstate.getItemDb().begin(); itr != origstate.getItemDb().end(); itr++) {
    CHECK(itr->second.size >= 0);
    CHECK(itr->second.metadata.timestamp >= 0);
    CHECK(citem.count(make_pair(false, itr->first)) == 0);
    citem[make_pair(false, itr->first)] = itr->second;
    citemsizemap[itr->second.size].push_back(make_pair(false, itr->first));
    fi.creates.push_back(make_pair(false, itr->first));
    ftc.insert(itr->first);
  }
  
  // FTC is the union of the files in realitems and origstate
  // citem is the items that we can look at
  // citemsizemap is the same, only organized by size
  for(set<string>::iterator itr = ftc.begin(); itr != ftc.end(); itr++) {
    if(realitems.count(*itr)) {
      const Item &ite = realitems.find(*itr)->second;
      bool got = false;
      
      // First, we check to see if it's the same file as existed before
      if(!got && citem.count(make_pair(false, *itr))) {
        const Item &pite = citem.find(make_pair(false, *itr))->second;
        if(ite.size == pite.size && ite.metadata == pite.metadata) {
          // It's identical!
          printf("Preserve file %s\n", itr->c_str());
          fi.creates.push_back(make_pair(true, *itr));
          got = true;
        } else if(ite.size == pite.size && ite.checksum() == pite.checksum()) {
          // It's touched!
          CHECK(ite.metadata != pite.metadata);
          printf("Touching file %s\n", itr->c_str());
          Instruction ti;
          ti.type = TYPE_TOUCH;
          ti.creates.push_back(make_pair(true, *itr));
          ti.depends.push_back(make_pair(false, *itr)); // if this matters, something is hideously wrong
          ti.touch_path = *itr;
          ti.touch_meta = ite.metadata;
          inst.push_back(ti);
          got = true;
        } else if(ite.size > pite.size && ite.checksumPart(pite.size) == pite.checksum()) {
          // It's appended!
          printf("Appendination on %s, dude!\n", itr->c_str());
          Instruction ti;
          ti.type = TYPE_APPEND;
          ti.creates.push_back(make_pair(true, *itr));
          ti.depends.push_back(make_pair(false, *itr));
          ti.removes.push_back(make_pair(false, *itr));
          ti.append_path = *itr;
          ti.append_size = ite.size;
          ti.append_meta = ite.metadata;
          ti.append_checksum = ite.checksum();
          inst.push_back(ti);
          got = true;
        }
      }
      
      // Okay, now we see if it's been copied from somewhere
      if(!got) {
        const vector<pair<bool, string> > &sli = citemsizemap[ite.size];
        printf("Trying %d originals\n", sli.size());
        for(int k = 0; k < sli.size(); k++) {
          CHECK(ite.size == citem[sli[k]].size);
          printf("Comparing with %d:%s\n", sli[k].first, sli[k].second.c_str());
          printf("%s vs %s\n", ite.checksum().toString().c_str(), citem[sli[k]].checksum().toString().c_str());
          if(ite.checksum() == citem[sli[k]].checksum()) {
            printf("Holy crapcock! Copying %s from %s:%d! MADNESS\n", itr->c_str(), sli[k].second.c_str(), sli[k].first);
            Instruction ti;
            ti.type = TYPE_COPY;
            ti.creates.push_back(make_pair(true, *itr));
            ti.depends.push_back(sli[k]);
            if(citem.count(make_pair(false, *itr)))
              ti.removes.push_back(make_pair(false, *itr));
            ti.copy_source = sli[k].second;
            ti.copy_dest = *itr;
            ti.copy_dest_meta = ite.metadata;
            inst.push_back(ti);
            got = true;
            break;
          }
        }
      }
      
      // And now we give up and just store it
      if(!got) {
        printf("Storing %s from GALACTIC ETHER\n", itr->c_str());
        Instruction ti;
        ti.type = TYPE_STORE;
        ti.creates.push_back(make_pair(true, *itr));
        if(citem.count(make_pair(false, *itr)))
          ti.removes.push_back(make_pair(false, *itr));
        ti.store_path = *itr;
        ti.store_size = ite.size;
        ti.store_meta = ite.metadata;
        ti.store_checksum = ite.checksum();
        inst.push_back(ti);
        got = true;
      }
      
      CHECK(got);
      
      citem[make_pair(true, *itr)] = ite;
      citemsizemap[ite.size].push_back(make_pair(true, *itr));
      
    } else {
      CHECK(citem.count(make_pair(false, *itr)));
      printf("Delete file %s\n", itr->c_str());
      Instruction ti;
      ti.type = TYPE_DELETE;
      ti.delete_path = *itr;
      ti.removes.push_back(make_pair(false, *itr));
      inst.push_back(ti);
    }
  }
  
  inst.push_back(fi);
  
  inst = sortInst(inst);
  
  State newstate = origstate;
  
  for(int i = 0; i < inst.size(); i++) {
    printf("%s\n", inst[i].textout().c_str());
    newstate.process(inst[i]);
  }

  {
    const map<string, Item> &lhs = newstate.getItemDb();
    const map<string, Item> &rhs = realitems;
    CHECK(lhs.size() == rhs.size());
    for(map<string, Item>::const_iterator lhsi = lhs.begin(), rhsi = rhs.begin(); lhsi != lhs.end(); lhsi++, rhsi++) {
      printf("Comparing %s and %s\n", lhsi->first.c_str(), rhsi->first.c_str());
      printf("%s\n", lhsi->second.toString().c_str());
      printf("%s\n", rhsi->second.toString().c_str());
      CHECK(lhsi->first == rhsi->first);
      CHECK(lhsi->second == rhsi->second);
    }
  }
  
  newstate.writeOut("state1");

}
