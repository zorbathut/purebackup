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

#include "util.h"
#include "debug.h"

#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <vector>

using namespace std;

long long atoll(const char *in) {
  long long foo;
  sscanf(in, "%lld", &foo);
  return foo;
}

Checksum atochecksum(const char *in) {
  Checksum cs;
  for(int i = 0; i < 20; i++) {
    int k;
    sscanf(string(in + i * 2, in + i * 2 + 2).c_str(), "%x", &k);
    cs.bytes[i] = k;
  }
  CHECK(strcmp(in, cs.toString().c_str()) == 0);
  return cs;
}

string StringPrintf( const char *bort, ... ) {

  static vector< char > buf(2);
  va_list args;

  int done = 0;
  do {
    if( done )
      buf.resize( buf.size() * 2 );
    va_start( args, bort );
    done = vsnprintf( &(buf[ 0 ]), buf.size() - 1,  bort, args );
    va_end( args );
  } while( done >= buf.size() - 1 || done == -1);

  CHECK( done < (int)buf.size() );

  return string(buf.begin(), buf.begin() + done);

};

vector<DirListOut> getDirList(const string &path) {
  vector<DirListOut> rv;
  DIR *od = opendir(path.c_str());
  if(!od)
    return vector<DirListOut>();
  CHECK(od);
  dirent *dire;
  while(dire = readdir(od)) {
    if(strcmp(dire->d_name, ".") == 0 || strcmp(dire->d_name, "..") == 0)
      continue;
    struct stat stt;
    //printf("%s\n", (path + "/" + dire->d_name).c_str());
    if(stat((path + "/" + dire->d_name).c_str(), &stt)) {
      printf("Error reading %s\n", (path + "/" + dire->d_name).c_str());
      CHECK(0);
    }
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

