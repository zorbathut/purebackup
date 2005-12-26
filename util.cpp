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

long long atoll(const char *in) {
  long long foo;
  sscanf(in, "%lld", &foo);
  return foo;
}

Checksum atochecksum(const char *in) {
  Checksum cs;
  for(int i = 0; i < 20; i++) {
    int k;
    sscanf(string(in + i * 2, in + i * 2 + i).c_str(), "%x", &k);
    cs.bytes[i] = k;
  }
  return cs;
}