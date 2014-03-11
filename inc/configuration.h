#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <map>
#include <string>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>
#include <sstream>

using namespace std;

class Configuration {
    private:
        map<string,string> data;
        static string Trim(const string& str);
    public:
        void Clear();
        bool Load(const string& file);
        bool Contains(const string& key) const;
        bool Get(const string& key, string& value) const;
        bool Get(const string& key, int&    value) const;
        bool Get(const string& key, long&   value) const;
        bool Get(const string& key, double& value) const;
        bool Get(const string& key, bool&   value) const;
        string Stripe(const string& str);
        string Ltrim(string& s);
        string Rtrim(string& s);
        string Trimm(string& s);
        string &ToString(char *c);
        int WriteToCmdfile(const string& file,char *cmd);
        string OpenFile(const string& file);
        string ReplaceString(string subject, const string& search, const string& replace);
        string GetStdoutFromCommand(string cmd);
};

#endif