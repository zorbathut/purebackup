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

#ifndef PUREBACKUP_PARSE
#define PUREBACKUP_PARSE

#include <iostream>
#include <string>
#include <map>
#include <vector>

using namespace std;

vector< string > tokenize( const string &in, const string &kar );
vector< int > sti( const vector< string > &foo );
  
class kvData {
public:
  
  string category;
  map<string, string> kv;

  string debugOutput() const;

  string consume(string key);
  bool isDone() const;
  void shouldBeDone() const;

};

istream &getLineStripped(istream &ifs, string &out);
istream &getkvData(istream &ifs, kvData &out);

void putkvDataInline(ostream &ofs, const kvData &in, const string &mostimportant = "");
string putkvDataInlineString(const kvData &ifs, const string &mostimportant = "");
istream &getkvDataInline(istream &ifs, kvData &out);
kvData getkvDataInlineString(const string &dat);

#endif
