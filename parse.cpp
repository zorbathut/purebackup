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

using namespace std;

vector< string > tokenize( const string &in, const string &kar ) {
  string::const_iterator cp = in.begin();
  vector< string > oot;
  while( cp != in.end() ) {
    while( cp != in.end() && count( kar.begin(), kar.end(), *cp ) )
      cp++;
    if( cp != in.end() )
      oot.push_back( string( cp, find_first_of( cp, in.end(), kar.begin(), kar.end() ) ) );
    cp = find_first_of( cp, in.end(), kar.begin(), kar.end() );
  };
  return oot;
};

vector< int > sti( const vector< string > &foo ) {
  int i;
  vector< int > bar;
  for( i = 0; i < foo.size(); i++ ) {
    bar.push_back( atoi( foo[ i ].c_str() ) );
  }
  return bar;
};

string kvData::debugOutput() const {
  string otp;
  otp = "\"";
  otp += category;
  otp += "\" : ";
  for(map<string, string>::const_iterator itr = kv.begin(); itr != kv.end(); itr++) {
    otp += "{ \"" + itr->first + "\" -> \"" + itr->second + "\" }, ";
  }
  return otp;
}

string kvData::consume(string key) {
  if(kv.count(key) != 1) {
    dprintf("Failed to read key \"%s\" in object \"%s\"\n", key.c_str(), category.c_str());
    CHECK(kv.count(key) == 1);
  }
  string out = kv[key];
  kv.erase(kv.find(key));
  return out;
}

bool kvData::isDone() const {
  return kv.size() == 0;
}
void kvData::shouldBeDone() const {
  CHECK(isDone());
}

istream &getLineStripped(istream &ifs, string &out) {
  while(getline(ifs, out)) {
    out = string(out.begin(), find(out.begin(), out.end(), '#'));
    while(out.size() && isspace(*out.begin()))
      out.erase(out.begin());
    while(out.size() && isspace(out[out.size()-1]))
      out.erase(out.end() - 1);
    if(out.size())
      return ifs;
  }
  return ifs;
}

istream &getkvData(istream &ifs, kvData &out) {
  out.kv.clear();
  out.category.clear();
  {
    string line;
    getLineStripped(ifs, line);
    if(!ifs)
      return ifs; // only way to return failure without an assert
    vector<string> tok = tokenize(line, " ");
    CHECK(tok.size() == 2);
    CHECK(tok[1] == "{");
    out.category = tok[0];
  }
  {
    string line;
    while(getLineStripped(ifs, line)) {
      if(line == "}")
        return ifs;
      vector<string> tok = tokenize(line, "=");
      CHECK(tok.size() == 1 || tok.size() == 2);
      string dat;
      if(tok.size() == 1)
        dat = "";
      else
        dat = tok[1];
      if(!out.kv.count(tok[0]))
        out.kv[tok[0]] = dat;
      else
        out.kv[tok[0]] += "\n" + dat;
    }
  }
  CHECK(0);
  return ifs; // this will be failure
}

void putkvDataInline(ostream &ofs, const kvData &in, const string &mostimportant) {
  ofs << putkvDataInlineString(in, mostimportant) << '\n';
}

// Data format:
// keyword: key="value" key="value"
// Keyword and key are alphanumeric or underscore strictly
// Value is pure binary, escaped as necessary.

bool isAlphanumeric(const string &str) {
  for(int i = 0; i < str.size(); i++)
    if(!isalpha(str[i]) && !isdigit(str[i]) && str[i] != '_')
      return false;
  return true;
}

char toHexChar(int in) {
  CHECK(in >= 0 && in < 16);
  if(in < 10)
    return in + '0';
  else
    return in - 10 + 'A';
}

void appendEscapedStr(string *stt, const string &esc) {
  (*stt) += '"';
  for(int i = 0; i < esc.size(); i++) {
    if(esc[i] == '"') {
      (*stt) += "\\\"";
    } else if(esc[i] == '\n') {
      (*stt) += "\\n";
    } else if(esc[i] == '\\') {
      (*stt) += "\\\\";
    } else if(esc[i] >= 32 && esc[i] < 127) {
      (*stt) += esc[i];
    } else {
      (*stt) += string("\\x") + toHexChar(esc[i] / 16) + toHexChar(esc[i] % 16);
    }
  }
  (*stt) += '"';
}

string putkvDataInlineString(const kvData &in, const string &mostimportant) {
  string stt;
  CHECK(isAlphanumeric(in.category));
  stt += in.category + ":";
  if(mostimportant != "") {
    CHECK(in.kv.count(mostimportant));
    CHECK(isAlphanumeric(mostimportant));
    stt += " " + mostimportant + "=";
    appendEscapedStr(&stt, in.kv.find(mostimportant)->second);
  }
  for(map<string, string>::const_iterator itr = in.kv.begin(); itr != in.kv.end(); itr++) {
    CHECK(itr->first.size());
    if(itr->first == mostimportant)
      continue;
    CHECK(isAlphanumeric(itr->first));
    stt += " " + itr->first + "=";
    appendEscapedStr(&stt, itr->second);
  }
  return stt;
}

int hexDigit(char pt) {
  if(isdigit(pt))
    return pt - '0';
  if(pt >= 'A' && pt <= 'F')
    return pt - 'A' + 10;
  if(pt >= 'a' && pt <= 'f')
    return pt - 'a' + 10;
  return -1;
}

bool isHex(char pt) {
  return hexDigit(pt) != -1;
}

int parseHex(const char **pt) {
  CHECK(isHex(**pt));
  if(isHex(*(*pt + 1))) {
    int rv = hexDigit(**pt) * 16 + isHex(*(*pt + 1));
    (*pt) += 2;
    return rv;
  } else {
    int rv = hexDigit(**pt);
    (*pt) += 1;
    return rv;
  }
}

string parseQuotedWord(const char **pt) {
  string oot;
  CHECK(**pt == '"');
  (*pt)++;
  while(**pt && **pt != '"') {
    if(**pt == '\\') {
      // escape character
      (*pt)++;
      if(**pt == '\\') {
        oot += '\\';
      } else if(**pt == 'n') {
        oot += '\n';
      } else if(**pt == '"') {
        oot += '"';
      } else if(**pt == 'x') {
        (*pt)++;
        oot += parseHex(pt);
      } else {
        CHECK(0);
      }
    } else if(**pt >= 32 && **pt < 127) {
      // standard ascii
      oot += **pt;
    } else {
      printf("Character %d, aka %c\n", **pt, **pt);
      CHECK(0);
    }
    (*pt)++;
  }
  CHECK(**pt == '"');
  (*pt)++;
  return oot;
}

string parseWord(const char **pt, char endchar) {
  string oot;
  while(**pt && **pt != endchar) {
    oot += **pt;
    (*pt)++;
  }
  CHECK(**pt == endchar);
  (*pt)++;
  return oot;
}

istream &getkvDataInline(istream &ifs, kvData &out) {
  out.kv.clear();
  out.category.clear();
  string line;
  getline(ifs, line);
  if(!ifs)
    return ifs; // failure, end of universe, sanity halted!
  out = getkvDataInlineString(line);
  return ifs; // this may be failure, if we're at the end
}

kvData getkvDataInlineString(const string &line) {
  kvData out;
  const char *pt = line.c_str();
  out.category = parseWord(&pt, ':');
  while(*pt) {
    CHECK(*pt == ' ');
    pt++;
    string key = parseWord(&pt, '=');
    string value = parseQuotedWord(&pt);
    CHECK(!out.kv.count(key));
    out.kv[key] = value;
  }
  return out;
}
