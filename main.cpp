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
#include "minizip/unzip.h"

#include <string>
#include <fstream>
#include <set>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <openssl/sha.h>

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
  
    if(cpos->type == MTT_UNINITTED || cpos->type == MTT_VIRTUAL) {
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

bool isNulled(const string &loc) {

  const char *tpt = loc.c_str();
  MountTree *cpos = getRoot();
  while(*tpt) {
    CHECK(*tpt == '/');
    tpt++;
    CHECK(*tpt);
  
    if(cpos->type == MTT_NULL) {
      printf("Null %s\n", loc.c_str());
      return true;
    }
    
    // isolate the next path component
    const char *tpn = strchr(tpt, '/');
    if(!tpn)
      tpn = tpt + strlen(tpt);
    
    string linkage = string(tpt, tpn);
    if(!cpos->links.count(linkage))
      return false; // It can't be null because it doesn't exist!
    cpos = &cpos->links[linkage];
    
    tpt = tpn;
  }
  return false;
}

void createMountpoint(const string &loc, const string &type, const string &source) {
  MountTree *dpt = getMountpointLocation(loc);
  CHECK(dpt);
  CHECK(dpt->type == MTT_UNINITTED);
  
  if(type == "file") {
    dpt->type = MTT_FILE;
    dpt->file_source = source;
    dpt->file_scanned = false;
  } else if(type == "ssh") {
    dpt->type = MTT_SSH;
    vector<string> spa = tokenize(source, "@");
    CHECK(spa.size() == 2);
    vector<string> spaa = tokenize(spa[0], ":");
    vector<string> spab = tokenize(spa[1], ":");
    CHECK(spaa.size() == 2);
    CHECK(spab.size() == 2);
    dpt->ssh_user = spaa[0];
    dpt->ssh_pass = spaa[1];
    dpt->ssh_host = spab[0];
    dpt->ssh_source = spab[1];
    dpt->ssh_scanned = false;
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
    bool didinexpensive = false;
    for(int i = 0; i < TYPE_END; i++) {
      if(didinexpensive && type_expensive[i]) {
        printf("Skipping mode due to expensive commands, will return\n");
        continue;
      }
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
        if(i != TYPE_CREATE) {  // these don't get output
          kinstr.push_back(buckets[i][j]);
          if(!type_expensive[i])
            didinexpensive = true;
        }
      }
      buckets[i].swap(buckupd);
    }
    for(int i = 0; i < TYPE_END; i++)
      tcl -= buckets[i].size();
    CHECK(tcl != 0);
  }

  return kinstr;
}

Checksum writeToZip(const Item *source, long long start, long long end, zipFile dest, const string &outfname) {
  // One problem here - we have to read the entire file just to get the right checksum. This is something that should be fixed in the future, but isn't yet, and I'm not quite sure how.
  //printf("%lld, %lld\n", start, end);
  SHA_CTX c;
  SHA1_Init(&c);
  ItemShunt *shunt = source->open();
  CHECK(shunt);
  long long pos = 0;
  while(pos != start) {
    char buf[1024*128];
    int desired = (int)min((long long)sizeof(buf), start - pos);
    int rv = shunt->read(buf, desired);
    CHECK(rv == desired);
    pos += rv;
    SHA1_Update(&c, buf, rv);
  }
  
  while(pos != end) {
    char buf[1024*128];
    int desired = (int)min((long long)sizeof(buf), end - pos);
    int rv = shunt->read(buf, desired);
    if(rv != desired) {
      printf("rv isn't desired! %d, %d\n", rv, desired);
      printf("in the middle %s\n", outfname.c_str());
      CHECK(0);
    }
    pos += rv;
    zipWriteInFileInZip(dest, buf, rv);
    SHA1_Update(&c, buf, rv);
  }
  
  delete shunt;
  
  Checksum csr = source->signaturePart(end); // because I'm lazy
  SHA1_Final(csr.bytes, &c);
  return csr;
}

long long filesize(const string &fsz) {
  struct stat stt;
  if(lstat(fsz.c_str(), &stt)) {
    printf("Couldn't stat %s\n", fsz.c_str());
    CHECK(0);
  }
  return stt.st_size;
}

class ArchiveState {
public:
  
  void doInst(const Instruction &inst, int tversion);

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
  long long data;
  int entries;

  int archives;

  string origstate;
};

void ArchiveState::doInst(const Instruction &inst, int tversion) {
  
  used += usedperitem;
  
  CHECK(inst.size() < 1900 * 1024 * 1024);  // fix this.
  
  if(archivemode != -1 && (archivemode != inst.type || (filesize(fname) + inst.size() > 1900 * 1024 * 1024))) {
    // We're not appending to this archive anymore, so close it
    CHECK(!zipClose(archivefile, NULL));
    archivemode = -1;
    archivefile = NULL;
    used += filesize(fname);
  }

  if(inst.type == TYPE_APPEND || inst.type == TYPE_STORE) {
    
    if(archivemode == -1) {
      
      // Open archive, write appropriate record, then rewind one item so we don't duplicate code
      string pfname = StringPrintf("%02d%s.zip", archives++, (inst.type == TYPE_APPEND) ? "append" : "store");
      fname = StringPrintf("%s/%s", destpath.c_str(), pfname.c_str());
      CHECK(archivefile = zipOpen(fname.c_str(), APPEND_STATUS_CREATE));
      archivemode = inst.type;
      
      if(inst.type == TYPE_APPEND) {
        kvData kvd;
        kvd.category = "append";
        kvd.kv["source"] = pfname;
        fprintf(proc, "%s\n", putkvDataInlineString(kvd).c_str());
      } else {
        kvData kvd;
        kvd.category = "store";
        kvd.kv["source"] = pfname;
        fprintf(proc, "%s\n", putkvDataInlineString(kvd).c_str());
      }
      
      entries++;
  
    }
    
    // Add data to archive, add an appropriate touch record
    zip_fileinfo zfi;
    zfi.dosDate = time(NULL);
    zfi.internal_fa = 0;
    zfi.external_fa = 0;
    if(inst.type == TYPE_APPEND) {
      CHECK(!zipOpenNewFileInZip(archivefile, inst.append_path.c_str() + 1, &zfi, NULL, 0, NULL, 0, NULL, Z_DEFLATED, Z_DEFAULT_COMPRESSION));
      data += inst.append_size - newstate->findItem(inst.append_path)->size();
      Checksum rvx = writeToZip(inst.append_source, newstate->findItem(inst.append_path)->size(), inst.append_size, archivefile, inst.append_path.c_str());
      CHECK(rvx == inst.append_checksum);
      CHECK(!zipCloseFileInZip(archivefile));
    } else {
      CHECK(inst.type == TYPE_STORE);
      CHECK(!zipOpenNewFileInZip(archivefile, inst.store_path.c_str() + 1, &zfi, NULL, 0, NULL, 0, NULL, Z_DEFLATED, Z_DEFAULT_COMPRESSION));
      data += inst.store_size;
      Checksum rvx = writeToZip(inst.store_source, 0, inst.store_size, archivefile, inst.store_path.c_str());
      if(rvx != inst.store_source->checksumPart(inst.store_size)) { // since this is where the "checksum" comes from in the file
        printf("%s checksum mismatch\n", inst.store_path.c_str());
        CHECK(0);
      }
      CHECK(!zipCloseFileInZip(archivefile));
    }
    
    // this should be a touch record
    fprintf(proc, "%s\n", inst.processString().c_str());
    //printf("%s\n", inst.textout().c_str());
    
  } else {
    // Write instruction as normal, it's not an append or a store
    fprintf(proc, "%s\n", inst.processString().c_str());
  }
  
  newstate->process(inst, tversion);
  entries++;

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
  data = 0;
  entries = 0;
  
  archives = 0;
  
  origstate = in_origstate;
}

ArchiveState::~ArchiveState() {
  if(archivefile) {
    CHECK(!zipClose(archivefile, NULL));
    used += filesize(fname);
  }
  
  printf("Closing archive. Expected size: %lld. Data stored: %lld (plus %d entries of overhead). Compression: %f%%\n", used, data, entries, (1.0 - (double)used / data) * 100);
  
  fclose(proc);
  
  printf("Writing state\n");
  newstate->writeOut(StringPrintf("%s/newstate", destpath.c_str()));
  printf("Writing state\n");
  
  system(StringPrintf("diff %s %s/newstate > %s/statediff", origstate.c_str(), destpath.c_str(), destpath.c_str()).c_str());
  unlink(StringPrintf("%s/newstate", destpath.c_str()).c_str());
}
  
// Things we generate:
// * Process file, consisting of every step
// * Some number of archive files
// * Some number of other compressed datafiles, possibly
// * State diff
void generateArchive(const vector<Instruction> &inst, State *newstate, const string &origstate, long long size, const string &destpath, bool *spaceleft, int tversion) {
  
  dprintf("Starting archive - %d instructions\n", inst.size());
  
  ArchiveState ars(origstate, newstate, destpath);
  
  for(int i = 0; i < inst.size(); i++) {
    long long tused = ars.getCSize() + usedperitem;
    printf("%lld of %lld written (%.1f%%)\r", tused, size, (double)tused / size * 100);
    fflush(stdout);
    if(tused + inst[i].size() > size && size - tused > (1<<20) && (inst[i].type == TYPE_APPEND || inst[i].type == TYPE_STORE)) {
      // We've got some space left, so let's see what we can do with it
      // This code kind of relies on the "fact" that State won't have been changed yet with an Append or a Store
      // This whole thing's really kind of hacky, I suppose.
      dprintf("Doing nasty half-instruction, woooo\n");
      if(inst[i].type == TYPE_APPEND) {
        Instruction ninst = inst[i];
        ninst.append_size = newstate->findItem(ninst.append_path)->size() + (size - tused);
        ninst.append_begin = newstate->findItem(ninst.append_path)->size();
        CHECK(ninst.append_size < inst[i].append_size);
        ninst.append_checksum = ninst.append_source->checksumPart(ninst.append_size);
        ars.doInst(ninst, tversion);
      } else {
        CHECK(inst[i].type == TYPE_STORE);
        Instruction ninst = inst[i];
        ninst.store_size = size - tused;
        CHECK(ninst.store_size < inst[i].store_size);
        ars.doInst(ninst, tversion);
      }
      dprintf("Done nasty half-instruction\n");
      break;
    } else if(tused + inst[i].size() > size) {
      break;
    } else {
      ars.doInst(inst[i], tversion);
    }
  }
  
  *spaceleft = ars.getCSize() + 10*1024*1024 < size;

}

long long getTotalSizeUsed(const string &path) {
  pair<bool, vector<DirListOut> > dlo = getDirList(path);
  if(dlo.first)
    return 0;
  long long tsize = 0;
  for(int i = 0; i < dlo.second.size(); i++) {
    if(dlo.second[i].directory) {
      tsize += getTotalSizeUsed(dlo.second[i].full_path);
    } else {
      tsize += dlo.second[i].size;
    }
  }
  return tsize;
}

pair<int, long long> inferDiscInfo() {
  // For one thing, we don't know how much data we can actually hold
  // For another thing, we don't know anything about our various overheads
  // And for a third thing, we basically, essentially, don't know anything
  // Why the hell do these tools suck so much?
  
  const string drive = "/cygdrive/m";
  //const string drive = "/cygdrive/c/werk/sea/purebackup/temp";
  //const long long drivesize = 40*1024*1024 + getTotalSizeUsed(drive);
  const long long drivesize = 4482ll*1024*1024;
  
  long long usedsize = getTotalSizeUsed(drive);
  printf("%lld bytes used\n", usedsize);
  
  // We ignore whether there's a disc or not - this code works even if there's an empty disc.
  vector<DirListOut> dlo = getDirList(drive).second;
  
  if(dlo.size() == 0)
    return make_pair(-1, drivesize - usedsize);
  
  // We need a manifest and at least one directory for this to be a valid archive.
  // If there is more than one directory, they'll need to be consecutively numbered.
  // Technically, we should make sure the directories contain appropriate data,
  // and that they are consecutive after the manifest.
  bool foundManifest = false;
  vector<int> dirnames;
  
  for(int i = 0; i < dlo.size(); i++) {
    if(dlo[i].itemname == "manifest.gz") {
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
      continue;
    }
    
    CHECK(0);
  }
  
  CHECK(foundManifest || !dirnames.size());
  
  sort(dirnames.begin(), dirnames.end());
  
  for(int i = 0; i < dirnames.size() - 1; i++)
    CHECK(dirnames[i] + 1 == dirnames[i + 1]);
  
  return make_pair(dirnames.back(), drivesize - usedsize);
};

void createDirectoryTree(const string &tree) {
  if(mkdir(tree.c_str(), 0755)) {
    createDirectoryTree(string(tree.c_str(), (const char *)strrchr(tree.c_str(), '/')));
    CHECK(!mkdir(tree.c_str(), 0755));
  }
}

FILE *openAndCreatePath(const string &path) {
  printf("open %s\n", path.c_str());
  FILE *fil = fopen(path.c_str(), "wb");
  if(!fil) {
    createDirectoryTree(string(path.c_str(), (const char *)strrchr(path.c_str(), '/')));
    fil = fopen(path.c_str(), "wb");
  }
  CHECK(fil);
  return fil;
}

void applyMetadata(const string &path, const Metadata &meta) {
  timeval tv[2];
  memset(tv, 0, sizeof(tv));
  tv[0].tv_sec = meta.timestamp;
  tv[1].tv_sec = meta.timestamp;
  
  CHECK(!utimes(path.c_str(), tv));
}

void copyFile(const string &src, const string &dst, const Metadata &meta) {
  FILE *fsrc = fopen(src.c_str(), "rb");
  FILE *fdst = openAndCreatePath(dst);
  
  CHECK(fsrc);
  CHECK(fdst);
  
  while(1) {
    char buf[65536];
    int rv = fread(buf, 1, sizeof(buf), fsrc);
    
    if(!rv)
      break;
    
    fwrite(buf, 1, rv, fdst);
  };
  
  fclose(fsrc);
  fclose(fdst);
  
  applyMetadata(dst, meta);
}

void restore(const string &src, const string &dst) {
  ifstream fil(StringPrintf("%s/process", src.c_str()).c_str());
  
  kvData kvd;
  while(getkvDataInline(fil, kvd)) {
    if(kvd.category == "store") {
      string start = kvd.consume("source");
      
      unzFile unzf = unzOpen((src + "/" + start).c_str());
      CHECK(unzf);
      
      CHECK(unzGoToFirstFile(unzf) == UNZ_OK);
      
      do {
        char filename[1024];
        
        CHECK(unzGetCurrentFileInfo(unzf, NULL, filename, sizeof(filename), NULL, 0, NULL, 0) == UNZ_OK);
        
        string realfile = dst + "/" + filename;
        printf("Creatinating file %s\n", realfile.c_str());
        
        CHECK(unzOpenCurrentFile(unzf) == UNZ_OK);
        
        FILE *fil = openAndCreatePath(realfile.c_str());
        do {
          char buf[65536];
          
          int byter = unzReadCurrentFile(unzf, buf, sizeof(buf));
          if(!byter)
            break;
          
          fwrite(buf, 1, byter, fil);
        } while(1);
        
        fclose(fil);
        
        unzCloseCurrentFile(unzf);
          
      } while(unzGoToNextFile(unzf) == UNZ_OK);
      
      unzClose(unzf);
      
    } else if(kvd.category == "touch") {
      
      string path = kvd.consume("path");
      Metadata meta = metaParseFromKvd(getkvDataInlineString(kvd.consume("meta")));
      
      applyMetadata(dst + path, meta);
      
    } else if(kvd.category == "copy") {
      
      copyFile(dst + kvd.consume("source"), dst + kvd.consume("dest"), metaParseFromKvd(getkvDataInlineString(kvd.consume("dest_meta"))));

    } else if(kvd.category == "delete") {
      
      unlink((dst + kvd.consume("path")).c_str());
    
    } else if(kvd.category == "rotate") {
      
      int ct = 0;
      for(ct = 0; kvd.kv.count(StringPrintf("meta%02d", ct)); ct++);
      
      CHECK(ct >= 2);
      
      vector<pair<string, Metadata> > path;
      for(int i = 0; i < ct; i++)
        path.push_back(make_pair(kvd.consume(StringPrintf("src%02ddst%02d", (i + 1) % ct, i)), metaParseFromKvd(getkvDataInlineString(kvd.consume(StringPrintf("meta%02d", i))))));
      
      reverse(path.begin(), path.end());
      
      path.insert(path.begin(), make_pair("/tmp1230478cvhoiuw", Metadata())); // wheeeeeeee
      path.push_back(make_pair("/tmp1230478cvhoiuw", Metadata()));
      
      for(int i = 0; i < path.size() - 1; i++)
        copyFile(dst + path[i + 1].first, dst + path[i].first, path[i].second);
      
      unlink((dst + "/tmp1230478cvhoiuw").c_str());
      
    } else if(kvd.category == "append") {
      string start = kvd.consume("source");
      
      unzFile unzf = unzOpen((src + "/" + start).c_str());
      CHECK(unzf);
      
      CHECK(unzGoToFirstFile(unzf) == UNZ_OK);
      
      do {
        char filename[1024];
        
        CHECK(unzGetCurrentFileInfo(unzf, NULL, filename, sizeof(filename), NULL, 0, NULL, 0) == UNZ_OK);
        
        string realfile = dst + "/" + filename;
        printf("Creatinating file %s\n", realfile.c_str());
        
        CHECK(unzOpenCurrentFile(unzf) == UNZ_OK);
        
        FILE *fil = fopen(realfile.c_str(), "a");
        do {
          char buf[65536];
          
          int byter = unzReadCurrentFile(unzf, buf, sizeof(buf));
          if(!byter)
            break;
          
          fwrite(buf, 1, byter, fil);
        } while(1);
        fclose(fil);
        
        unzCloseCurrentFile(unzf);
          
      } while(unzGoToNextFile(unzf) == UNZ_OK);
      
      unzClose(unzf);
      
    } else {
      CHECK(0);
    }
    
    CHECK(kvd.isDone());
    
  }
}

long long cssi = 0;
extern int if_presig;
extern int if_mid;
extern int if_full;

int main(int argc, char **argv) {
  
  if(argc != 2) {
    printf("purebackup backup or purebackup restore - and seriously, you really want to email zorba-purebackup@pavlovian.net if you want to do anything serious with this program.");
    return 0;
  }
  
  string command = argv[1];
  if(command == "backup") {
  
    pair<int, long long> inf = inferDiscInfo();
    if(inf.second < 1048576) {
      printf("New disc, fucker!\n");
      return 0;
    }
    
    printf("Reading config\n");
    readConfig("purebackup.conf");
    
    int curstateid;
    string curstate;
    string nextstate;
    
    {
      FILE *vidi = fopen("states/current", "r");
      curstateid = 0;
      if(vidi) {
        fscanf(vidi, "%d", &curstateid);
        fclose(vidi);
      } else {
        system("touch states/00000000");
      }
      
      curstate = StringPrintf("states/%08d", curstateid);
      nextstate = StringPrintf("states/%08d", curstateid + 1);
    }
    
    CHECK(inf.first == -1 || inf.first == curstateid);
    
    printf("Scanning items\n");
    scanPaths();
    
    printf("Dumping items\n");
    map<string, Item> realitems;
    getRoot()->dumpItems(&realitems, "");
    dprintf("%d items found\n", realitems.size());
    
    State origstate;
    origstate.readFile(curstate);
    
    map<pair<bool, string>, Item> citem;
    map<long long, vector<pair<bool, string> > > citemsizemap;
    set<string> ftc;
    
    Instruction fi;
    fi.type = TYPE_CREATE;
    
    vector<Instruction> inst;
    
    //map<long long, int> sizefreq;
    
    for(map<string, Item>::iterator itr = realitems.begin(); itr != realitems.end(); itr++) {
      CHECK(itr->second.size() >= 0);
      CHECK(itr->second.metadata().timestamp >= 0);
      ftc.insert(itr->first);
      //sizefreq[itr->second.size()]++;
    }
    
    /*
    {
      FILE *sfq = fopen("sizefreq.txt", "w");
      for(map<long long, int>::iterator itr = sizefreq.begin(); itr != sizefreq.end(); itr++)
        if(itr->second > 1)
          fprintf(sfq, "%lld: %d\n", itr->first, itr->second);
      fclose(sfq);
    }
    
    return 0;*/
    
    for(map<string, Item>::const_iterator itr = origstate.getItemDb().begin(); itr != origstate.getItemDb().end(); itr++) {
      CHECK(itr->second.size() >= 0);
      CHECK(itr->second.metadata().timestamp >= 0);
      CHECK(citem.count(make_pair(false, itr->first)) == 0);
      citem[make_pair(false, itr->first)] = itr->second;
      citemsizemap[itr->second.size()].push_back(make_pair(false, itr->first));
      fi.creates.push_back(make_pair(false, itr->first));
      ftc.insert(itr->first);
    }
    
    printf("Starting examining\n");
    
    long long totcomsize = 0;
    bool earlyterm = false;
    
    int ltime = 0;
    
    int itpos = 0;
    // FTC is the union of the files in realitems and origstate
    // citem is the items that we can look at
    // citemsizemap is the same, only organized by size
    for(set<string>::iterator itr = ftc.begin(); itr != ftc.end(); itr++) {
      if(ltime != time(NULL)) {
        printf("%d/%d files, %d/%d/%d, %lld read, %lld filled, now %s\r", itpos, ftc.size(), if_presig, if_mid, if_full, cssi, totcomsize, itr->c_str());
        ltime = time(NULL);
      }
      itpos++;
      fflush(stdout);
      
      /*
      if(totcomsize > inf.second * 2) {
        printf("Archive is getting too big, splitting\n");
        earlyterm = true;
        break;
      }
      */
  
      // If it's null, it doesn't exist in the real items because we couldn't scan it. However, if we're iterating over it, it *must* exist.
      // Therefore, it must exist in the original items.
      CHECK(!(isNulled(*itr) && !citem.count(make_pair(false, *itr))));
      
      //dprintf("Processing %s", itr->c_str());
      
      // If it's null, we pretend it exists and is identical to what we currently have, which involves going through this section.
      if(realitems.count(*itr) || isNulled(*itr)) {
        const Item &ite = realitems.find(*itr)->second;
        bool got = false;
        
        // First, we check to see if it's the same file as existed before
        if(!got && citem.count(make_pair(false, *itr))) {
          const Item &pite = citem.find(make_pair(false, *itr))->second;
          if(isNulled(*itr) || ite.size() == pite.size() && ite.metadata() == pite.metadata()) {
            // It's identical!
            //printf("Preserve file %s\n", itr->c_str());
            fi.creates.push_back(make_pair(true, *itr));
            got = true;
          } else if(ite.size() == pite.size() && ite.isChecksummable() && identicalFile(ite, pite)) {
            // It's touched!
            CHECK(ite.metadata() != pite.metadata());
            //printf("Touching file %s\n", itr->c_str());
            Instruction ti;
            ti.type = TYPE_TOUCH;
            ti.creates.push_back(make_pair(true, *itr));
            ti.depends.push_back(make_pair(false, *itr)); // if this matters, something is hideously wrong
            ti.touch_path = *itr;
            ti.touch_meta = ite.metadata();
            totcomsize += ti.size();
            inst.push_back(ti);
            got = true;
          } else if(ite.size() > pite.size() && pite.size() > 0 && ite.isChecksummable() && identicalFile(ite, pite, pite.size())) {
            // It's appended!
            // The pite.size() check is so we don't claim a file going from 0 bytes to more is "appended"
            // Technically that's valid, but it's a bit ugly and so I decided to make it not happen. :)
            //printf("Appendination on %s, dude!\n", itr->c_str());
            Instruction ti;
            ti.type = TYPE_APPEND;
            ti.creates.push_back(make_pair(true, *itr));
            ti.depends.push_back(make_pair(false, *itr));
            ti.removes.push_back(make_pair(false, *itr));
            ti.append_path = *itr;
            ti.append_size = ite.size();
            ti.append_begin = pite.size();
            ti.append_meta = ite.metadata();
            ti.append_checksum = ite.checksum();
            ti.append_source = &ite;
            totcomsize += ti.size();
            inst.push_back(ti);
            got = true;
          }
        }
        
        // If either of these are true, we don't have adequate data - if it's null we're saving it from deletion,
        // if it's merely unreadable we're simply ignoring it
        if(!got) {
          if(isNulled(*itr) || !ite.isReadable()) {
            if(citem.count(make_pair(false, *itr))) {
              fi.creates.push_back(make_pair(true, *itr));
              got = true;
            } else {
              continue;
            }
          }
        }
        
        // Okay, now we see if it's been copied from somewhere
        if(!got) {
          CHECK(ite.isChecksummable());
          const vector<pair<bool, string> > &sli = citemsizemap[ite.size()];
          for(int k = 0; k < sli.size(); k++) {
            CHECK(ite.size() == citem[sli[k]].size());
            
            // It's possible for a nulled or unreadable item to get pushed into the citem map. If so, we can't necessarily compare it.
            // There may be a better way to do this.
            if(citem[sli[k]].isChecksummable() && identicalFile(ite, citem[sli[k]])) {
              //printf("Holy crapcock! Copying %s from %s:%d! MADNESS\n", itr->c_str(), sli[k].second.c_str(), sli[k].first);
              Instruction ti;
              ti.type = TYPE_COPY;
              ti.creates.push_back(make_pair(true, *itr));
              ti.depends.push_back(sli[k]);
              if(citem.count(make_pair(false, *itr)))
                ti.removes.push_back(make_pair(false, *itr));
              ti.copy_source = sli[k].second;
              ti.copy_dest = *itr;
              ti.copy_dest_meta = ite.metadata();
              totcomsize += ti.size();
              inst.push_back(ti);
              got = true;
              break;
            }
          }
        }
        
        // And now we give up and just store it
        if(!got) {
          CHECK(ite.isReadable());
          //printf("Storing %s from GALACTIC ETHER\n", itr->c_str());
          Instruction ti;
          ti.type = TYPE_STORE;
          ti.creates.push_back(make_pair(true, *itr));
          if(citem.count(make_pair(false, *itr)))
            ti.removes.push_back(make_pair(false, *itr));
          ti.store_path = *itr;
          ti.store_size = ite.size();
          ti.store_meta = ite.metadata();
          ti.store_source = &ite;
          totcomsize += ti.size();
          inst.push_back(ti);
          got = true;
        }
         
        CHECK(got);
        
        citem[make_pair(true, *itr)] = ite;
        citemsizemap[ite.size()].push_back(make_pair(true, *itr));
        
      } else {
        CHECK(citem.count(make_pair(false, *itr)));
        //printf("Delete file %s\n", itr->c_str());
        Instruction ti;
        ti.type = TYPE_DELETE;
        ti.delete_path = *itr;
        ti.removes.push_back(make_pair(false, *itr));
        totcomsize += ti.size();
        inst.push_back(ti);
      }
    }
    
    inst.push_back(fi);
    
    inst = sortInst(inst);
    
    State newstate = origstate;
    
    printf("Genarch\n");
    
    if(inst.size() == 0) {
      printf("No changes!\n");
      return 0;
    }
    
    {
      long long archsize = 0;
      for(int i = 0; i < inst.size(); i++) {
        archsize += usedperitem;
        if(inst[i].type == TYPE_APPEND) {
          archsize += inst[i].append_size - newstate.findItem(inst[i].append_path)->size();
        } else if(inst[i].type == TYPE_STORE) {
          archsize += inst[i].store_size;
        }
      }
      printf("Total of %lld bytes left! (%lldmb)\n", archsize, archsize >> 20);
    }
    
    system("rm -rf temp");  // this is obviously dangerous, dur
    system("mkdir temp");
    
    printf("Generating archive of at most %lld bytes\n", inf.second);
    
    bool spaceleft;
    
    if(inf.first == -1) {
      // We need to copy our original state to the root, then create our first patch
      system(StringPrintf("cp %s %s", curstate.c_str(), "temp/manifest").c_str()); // note: this is buggy and probably a security hole
      system("gzip temp/manifest");
      string destpath = StringPrintf("temp/%08d", curstateid + 1);
      system(StringPrintf("mkdir %s", destpath.c_str()).c_str());
      
      generateArchive(inst, &newstate, curstate, inf.second - filesize("temp/manifest.gz"), destpath, &spaceleft, curstateid + 1);
    } else {
      // We don't. (Duh.)
      CHECK(inf.first == curstateid);
      string destpath = StringPrintf("temp/%08d", curstateid + 1);
      system(StringPrintf("mkdir %s", destpath.c_str()).c_str());
      
      generateArchive(inst, &newstate, curstate, inf.second, destpath, &spaceleft, curstateid + 1);
    }
    
    if(earlyterm)
      CHECK(!spaceleft);
    
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
    
    /*
    if(inf.first == -1) {
      // We're not continuing a multisession CD
      system("mkisofs -J -r -o image.iso temp");
    } else {
      // We are continuing a multisession CD
      system("mkisofs -J -r -C `dvdrecord dev=1,0,0 -msinfo`-o image.iso temp");
    }
    
    spaceleft = true;
    if(spaceleft) {
      // We're leaving multisession space open
      system("dvdrecord dev=1,0,0 -v -eject speed=40 fs=16m -multi image.iso");
    } else {
      // We're closing the CD
      system("dvdrecord dev=1,0,0 -v -eject speed=40 fs=16m image.iso");
    }*/
    
    if(spaceleft) {
      printf("Burn it now, and leave multisession open!\n");
    } else {
      printf("Burn it now, and close the CD!\n");
    }
    
    
    newstate.writeOut(nextstate);
    
    FILE *curv = fopen("states/current", "w");
    CHECK(curv);
    fprintf(curv, "%d\n", curstateid + 1);
    fclose(curv);
    
  } else if(command == "restore") {
    
    string source = "/cygdrive/c/werk/sea/purebackup/temp";
    string dest = "/cygdrive/c/werk/sea/purebackup/restore";
    
    system(StringPrintf("rm -rf %s", dest.c_str()).c_str());
    
    for(int i = 1; i <= 5; i++) {
      printf("Restoring %d\n", i);
      restore(StringPrintf("%s/%08d", source.c_str(), i), dest);
    }
    
  } else {
    printf("just \"purebackup\" for help\n");
  }

}
