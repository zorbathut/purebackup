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

#ifndef PUREBACKUP_DEBUG
#define PUREBACKUP_DEBUG

/*************
 * CHECK/TEST macros
 */
 
int dprintf( const char *bort, ... ) __attribute__((format(printf,1,2)));

void CrashHandler();

void crash() __attribute__((__noreturn__));
#define CHECK(x) while(1) { if(!(x)) { dprintf("Error at %s:%d - %s\n", __FILE__, __LINE__, #x); CrashHandler(); crash(); } break; }
#define TEST(x) CHECK(x)

#endif
