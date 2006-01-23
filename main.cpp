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

#include "minizip/zip.h"

#include <string>
#include <fstream>
#include <set>
#include <sys/types.h>
#include <sys/stat.h>

using namespace std;

// Makes sure everything up to this is virtual
MountTree *getMountpointLocation(const string &loc) {
  const char *tpt = loc.c_str();
  MountTree *cpos = getRoot();
  bool inRealTree = false;
  while(*tpt) {
    CHECK(*tpt == '/');
    tpt++;
    CHECK(*tpt);
  
    if(cpos->type == MTT_UNINITTED) {
      if(!inRealTree) {
        cpos->type = MTT_VIRTUAL;
      } else {
        cpos->type = MTT_IMPLIED;
      }
    } else {
      inRealTree = true;
    }
    
    // isolate the next path component
    const char *tpn = strchr(tpt, '/');
    if(!tpn)
      tpn = tpt + strlen(tpt);
    
    string linkage = string(tpt, tpn);
    cpos = &cpos->links[linkage];
    
    tpt = tpn;
  }
  return cpos;
}

void createMountpoint(const string &loc, const string &type, const string &source) {
  MountTree *dpt = getMountpointLocation(loc);
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

void createMask(const string &loc) {
  MountTree *dpt = getMountpointLocation(loc);
  CHECK(dpt);
  CHECK(dpt->type == MTT_UNINITTED);
  
  dpt->type = MTT_MASKED;
  
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
    } else if(kvd.category == "mask") {
      createMask(kvd.consume("remove"));
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
      vector<Instruction> buckupd;
      for(int j = 0; j < buckets[i].size(); j++) {
        bool good = true;
        for(int k = 0; good && k < buckets[i][j].depends.size(); k++)
          if(!live.count(buckets[i][j].depends[k]))
            good = false;
        for(int k = 0; good && k < buckets[i][j].removes.size(); k++)
          if(!(leftToUse[buckets[i][j].removes[k]] == 0 || leftToUse[buckets[i][j].removes[k]] == 1 && count(buckets[i][j].depends.begin(), buckets[i][j].depends.end(), buckets[i][j].removes[k])))
            good = false;
        if(!good) {
          buckupd.push_back(buckets[i][j]);
          continue;
        }
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
        if(i != TYPE_CREATE)  // these don't get output
          kinstr.push_back(buckets[i][j]);
      }
      buckets[i].swap(buckupd);
    }
    for(int i = 0; i < TYPE_END; i++)
      tcl -= buckets[i].size();
    CHECK(tcl != 0);
  }

  return kinstr;
}

void writeToZip(const Item *source, long long start, long long end, zipFile dest) {
  //printf("%lld, %lld\n", start, end);
  ItemShunt *shunt = source->open();
  shunt->seek(start);
  char buffer[65536];
  while(start != end) {
    int desired = (int)min((long long)sizeof(buffer), end - start);
    int dt = shunt->read(buffer, desired);
    CHECK(dt == desired);
    zipWriteInFileInZip(dest, buffer, dt);
    start += dt;
  }
  CHECK(start == end);
  delete shunt;
}

long long filesize(const string &fsz) {
  struct stat stt;
  CHECK(!stat(fsz.c_str(), &stt));
  return stt.st_size;
}

class ArchiveState {
public:
  
  void doInst(const Instruction &inst);

  long long getCSize() const;

  ArchiveState(const string &origstate, State *newstate, const string &destpath);
  ~ArchiveState();

private:
  
  FILE *proc;
  int archivemode;
  zipFile archivefile;
  string fname;

  State *newstate;
  string destpath;

  long long used;
  int archives;

  string origstate;
};

const int usedperitem = 500;

void ArchiveState::doInst(const Instruction &inst) {

  used += usedperitem;
  
  if(archivemode != -1 && archivemode != inst.type) {
    // We're not appending to this archive anymore, so close it
    CHECK(!zipClose(archivefile, NULL));
    archivemode = -1;
    archivefile = NULL;
    used += filesize(fname);
  }
  
  if(inst.type == TYPE_APPEND || inst.type == TYPE_STORE) {
    
    if(archivemode == -1) {
      
      // Open archive, write appropriate record, then rewind one item so we don't duplicate code
      fname = StringPrintf("%s/%02d%s.zip", destpath.c_str(), archives++, (inst.type == TYPE_APPEND) ? "append" : "store");
      CHECK(archivefile = zipOpen(fname.c_str(), APPEND_STATUS_CREATE));
      archivemode = inst.type;
      
      if(inst.type == TYPE_APPEND) {
        kvData kvd;
        kvd.category = "append";
        kvd.kv["source"] = fname;
        fprintf(proc, "%s\n", getkvDataInlineString(kvd).c_str());
      } else {
        kvData kvd;
        kvd.category = "store";
        kvd.kv["source"] = fname;
        fprintf(proc, "%s\n", getkvDataInlineString(kvd).c_str());
      }
  
    }
    
    // Add data to archive, add an appropriate touch record
    zip_fileinfo zfi;
    zfi.dosDate = time(NULL);
    zfi.internal_fa = 0;
    zfi.external_fa = 0;
    if(inst.type == TYPE_APPEND) {
      CHECK(!zipOpenNewFileInZip(archivefile, inst.append_path.c_str() + 1, &zfi, NULL, 0, NULL, 0, NULL, Z_DEFLATED, Z_DEFAULT_COMPRESSION));
      writeToZip(inst.append_source, newstate->findItem(inst.append_path)->size, inst.append_size, archivefile);
      CHECK(!zipCloseFileInZip(archivefile));
    } else {
      CHECK(inst.type == TYPE_STORE);
      CHECK(!zipOpenNewFileInZip(archivefile, inst.store_path.c_str() + 1, &zfi, NULL, 0, NULL, 0, NULL, Z_DEFLATED, Z_DEFAULT_COMPRESSION));
      writeToZip(inst.store_source, 0, inst.store_size, archivefile);
      CHECK(!zipCloseFileInZip(archivefile));
    }
    
    // this should be a touch record
    fprintf(proc, "%s\n", inst.processString().c_str());
    //printf("%s\n", inst.textout().c_str());
    newstate->process(inst);
    
  } else {
    // Write instruction as normal, it's not an append or a store
    fprintf(proc, "%s\n", inst.processString().c_str());
  }

}

long long ArchiveState::getCSize() const {
  long long tused = used;
  if(archivefile)
    tused += filesize(fname) + (1<<20); // Account for some buffer on the zip writing functions
  return tused;
}

ArchiveState::ArchiveState(const string &in_origstate, State *in_newstate, const string &in_destpath) {
  proc = fopen(StringPrintf("%s/process", in_destpath.c_str()).c_str(), "w");
  CHECK(proc);
  
  newstate = in_newstate;
  destpath = in_destpath;
  
  archivemode = -1;
  archivefile = NULL;
  
  used = 0;
  archives = 0;
  
  origstate = in_origstate;
}

ArchiveState::~ArchiveState() {
  if(archivefile) {
    CHECK(!zipClose(archivefile, NULL));
    used += filesize(fname);
  }
  
  printf("Closing archive. Expected size: %lld\n", used);
  
  fclose(proc);
  
  newstate->writeOut(StringPrintf("%s/newstate", destpath.c_str()));
  system(StringPrintf("diff %s %s/newstate > %s/statediff", origstate.c_str(), destpath.c_str(), destpath.c_str()).c_str());
  unlink(StringPrintf("%s/newstate", destpath.c_str()).c_str());
}
  
// Things we generate:
// * Process file, consisting of every step
// * Some number of archive files
// * Some number of other compressed datafiles, possibly
// * State diff
void generateArchive(const vector<Instruction> &inst, State *newstate, const string &origstate, long long size, const string &destpath) {
  
  ArchiveState ars(origstate, newstate, destpath);
  
  for(int i = 0; i < inst.size(); i++) {
    long long tused = ars.getCSize() + usedperitem;
    printf("%lld of %lld written (%.1f%%)\r", tused, size, (double)tused / size * 100);
    fflush(stdout);
    long long nextitem;
    if(inst[i].type == TYPE_APPEND) {
      nextitem = inst[i].append_size - newstate->findItem(inst[i].append_path)->size;
    } else if(inst[i].type == TYPE_STORE) {
      nextitem = inst[i].store_size;
    }
    if(tused + nextitem > size && size - tused > (1<<20) && (inst[i].type == TYPE_APPEND || inst[i].type == TYPE_STORE)) {
      // We've got some space left, so let's see what we can do with it
      // This code kind of relies on the "fact" that State won't have been changed yet with an Append or a Store
      // This whole thing's really kind of hacky, I suppose.
      dprintf("Doing nasty half-instruction, woooo\n");
      if(inst[i].type == TYPE_APPEND) {
        Instruction ninst = inst[i];
        ninst.append_size = newstate->findItem(ninst.append_path)->size + (size - tused);
        CHECK(ninst.append_size < inst[i].append_size);
        ninst.append_checksum = ninst.append_source->checksumPart(ninst.append_size);
        ars.doInst(ninst);
      } else {
        CHECK(inst[i].type == TYPE_STORE);
        Instruction ninst = inst[i];
        ninst.store_size = size - tused;
        CHECK(ninst.store_size < inst[i].store_size);
        ars.doInst(ninst);
      }
      dprintf("Done nasty half-instruction\n");
      break;
    } else if(tused + nextitem > size) {
      break;
    } else {
      ars.doInst(inst[i]);
    }
  }

}

long long getTotalSizeUsed(const string &path) {
  vector<DirListOut> dlo = getDirList(path);
  long long tsize = 0;
  for(int i = 0; i < dlo.size(); i++) {
    if(dlo[i].directory) {
      tsize += getTotalSizeUsed(dlo[i].full_path);
    } else {
      tsize += dlo[i].size;
    }
  }
  return tsize;
}

pair<int, int> inferDiscInfo() {
  // For one thing, we don't know how much data we can actually hold
  // For another thing, we don't know anything about our various overheads
  // And for a third thing, we basically, essentially, don't know anything
  // Why the hell do these tools suck so much?
  
  const string drive = "/cygdrive/d";
  const long long drivesize = 20*1024*1024;
  
  long long usedsize = getTotalSizeUsed(drive);
  printf("%lld bytes used\n", usedsize);
  
  vector<DirListOut> dlo = getDirList(drive);
  
  if(dlo.size() == 0)
    return make_pair(-1, drivesize - usedsize);
  
  // We need a manifest and at least one directory for this to be a valid archive.
  // If there is more than one directory, they'll need to be consecutively numbered.
  // Technically, we should make sure the directories contain appropriate data,
  // and that they are consecutive after the manifest.
  bool foundManifest = false;
  vector<int> dirnames;
  
  for(int i = 0; i < dlo.size(); i++) {
    if(dlo[i].itemname == "manifest") {
      CHECK(!dlo[i].directory);
      CHECK(!foundManifest);
      foundManifest = true;
      continue;
    }
    
    if(atoi(dlo[i].itemname.c_str()) != 0) {
      for(int j = 0; j < dlo[i].itemname.size(); j++)
        CHECK(isdigit(dlo[i].itemname[j]));
      CHECK(dlo[i].directory);
      dirnames.push_back(atoi(dlo[i].itemname.c_str()));
    }
    
    CHECK(0);
  }
  
  sort(dirnames.begin(), dirnames.end());
  
  for(int i = 0; i < dirnames.size() - 1; i++)
    CHECK(dirnames[i] + 1 == dirnames[i + 1]);
  
  return make_pair(dirnames.back(), drivesize - usedsize);
};

int main() {
  
  pair<int, int> inf = inferDiscInfo();
  if(inf.second < 1048576) {
    printf("New disc, fucker!\n");
    return 0;
  }
  
  printf("Reading config\n");
  readConfig("purebackup.conf");
  
  printf("Scanning items\n");
  scanPaths();
  
  printf("Dumping items\n");
  map<string, Item> realitems;
  getRoot()->dumpItems(&realitems, "");
  dprintf("%d items found\n", realitems.size());
  
  const string curstate = "state0";
  const int curstateid = 0;
  
  State origstate;
  origstate.readFile(curstate);
  
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
  
  int itpos = 0;
  // FTC is the union of the files in realitems and origstate
  // citem is the items that we can look at
  // citemsizemap is the same, only organized by size
  for(set<string>::iterator itr = ftc.begin(); itr != ftc.end(); itr++) {
    printf("%d/%d files examined\r", itpos++, ftc.size());
    fflush(stdout);
    if(realitems.count(*itr)) {
      const Item &ite = realitems.find(*itr)->second;
      bool got = false;
      
      // First, we check to see if it's the same file as existed before
      if(!got && citem.count(make_pair(false, *itr))) {
        const Item &pite = citem.find(make_pair(false, *itr))->second;
        if(ite.size == pite.size && ite.metadata == pite.metadata) {
          // It's identical!
          //printf("Preserve file %s\n", itr->c_str());
          fi.creates.push_back(make_pair(true, *itr));
          got = true;
        } else if(ite.size == pite.size && ite.checksum() == pite.checksum()) {
          // It's touched!
          CHECK(ite.metadata != pite.metadata);
          //printf("Touching file %s\n", itr->c_str());
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
          //printf("Appendination on %s, dude!\n", itr->c_str());
          Instruction ti;
          ti.type = TYPE_APPEND;
          ti.creates.push_back(make_pair(true, *itr));
          ti.depends.push_back(make_pair(false, *itr));
          ti.removes.push_back(make_pair(false, *itr));
          ti.append_path = *itr;
          ti.append_size = ite.size;
          ti.append_meta = ite.metadata;
          ti.append_checksum = ite.checksum();
          ti.append_source = &ite;
          inst.push_back(ti);
          got = true;
        }
      }
      
      // Okay, now we see if it's been copied from somewhere
      if(!got) {
        const vector<pair<bool, string> > &sli = citemsizemap[ite.size];
        //printf("Trying %d originals\n", sli.size());
        for(int k = 0; k < sli.size(); k++) {
          CHECK(ite.size == citem[sli[k]].size);
          //printf("Comparing with %d:%s\n", sli[k].first, sli[k].second.c_str());
          //printf("%s vs %s\n", ite.checksum().toString().c_str(), citem[sli[k]].checksum().toString().c_str());
          if(ite.checksum() == citem[sli[k]].checksum()) {
            //printf("Holy crapcock! Copying %s from %s:%d! MADNESS\n", itr->c_str(), sli[k].second.c_str(), sli[k].first);
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
        //printf("Storing %s from GALACTIC ETHER\n", itr->c_str());
        Instruction ti;
        ti.type = TYPE_STORE;
        ti.creates.push_back(make_pair(true, *itr));
        if(citem.count(make_pair(false, *itr)))
          ti.removes.push_back(make_pair(false, *itr));
        ti.store_path = *itr;
        ti.store_size = ite.size;
        ti.store_meta = ite.metadata;
        ti.store_source = &ite;
        inst.push_back(ti);
        got = true;
      }
       
      CHECK(got);
      
      citem[make_pair(true, *itr)] = ite;
      citemsizemap[ite.size].push_back(make_pair(true, *itr));
      
    } else {
      CHECK(citem.count(make_pair(false, *itr)));
      //printf("Delete file %s\n", itr->c_str());
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
  
  printf("Genarch\n");
  
  system("rm -rf temp");  // this is obviously dangerous, dur
  system("mkdir temp");
  
  if(inf.first == -1) {
    // We need to copy our original state to the root, then create our first patch
    system(StringPrintf("cp %s %s", curstate.c_str(), "temp/manifest").c_str()); // note: this is buggy and probably a security hole
    string destpath = StringPrintf("temp/%08d", curstateid + 1);
    system(StringPrintf("mkdir %s", destpath.c_str()).c_str());
    
    generateArchive(inst, &newstate, curstate, inf.second, destpath);
  } else {
    // We don't. (Duh.)
    CHECK(inf.first == curstateid);
    string destpath = StringPrintf("temp/%08d", curstateid + 1);
    system(StringPrintf("mkdir %s", destpath.c_str()).c_str());
    
    generateArchive(inst, &newstate, curstate, inf.second, destpath);
  }
  
  printf("Done genarch\n");

/*
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
*/
  
  newstate.writeOut("state1");

}
