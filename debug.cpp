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

#include "debug.h"

#include <cstdio>
#include <vector>
#include <stdarg.h>
#include <assert.h>
using namespace std;

int frameNumber = -1;

void CrashHandler() { };

void crash() {
  *(int*)0 = 0;
  while(1);
}

void outputDebugString(const char *in) {
  if(in[strlen(in) - 1] == '\n')
    printf("%s", in);
  else
    printf("%s\n", in);
}

int dprintf( const char *bort, ... ) {

  // this is duplicated code with StringPrintf - I should figure out a way of combining these
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

  assert( done < (int)buf.size() );

  outputDebugString( &(buf[ 0 ]) );

  return 0;

};
