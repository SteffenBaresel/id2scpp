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

#include "../inc/configuration.h"

using namespace std;

string s;

void Configuration::Clear()
{
    data.clear();
}

bool Configuration::Load(const string& file)
{
    ifstream inFile(file.c_str());

    if (!inFile.good())
    {
        cout << "Cannot read configuration file " << file << endl;
        return false;
    }

    while (inFile.good() && ! inFile.eof())
    {
        string line;
        getline(inFile, line);

        // filter out comments
        if (!line.empty())
        {
            unsigned int pos = line.find('#');

            if (pos != string::npos)
            {
                line = line.substr(0, pos);
            }
        }

        // split line into key and value
        if (!line.empty())
        {
            unsigned int pos = line.find('=');

            if (pos != string::npos)
            {
                string key     = Trim(line.substr(0, pos));
                string value   = Trim(line.substr(pos + 1));

            if (!key.empty() && !value.empty())
                {
                    data[key] = value;
                }
            }
        }
    }

    return true;
}

bool Configuration::Contains(const string& key) const
{
    return data.find(key) != data.end();
}

bool Configuration::Get(const string& key, string& value) const
{
    map<string,string>::const_iterator iter = data.find(key);

    if (iter != data.end())
    {
        value = iter->second;
        return true;
    }
    else
    {
        return false;
    }
}

bool Configuration::Get(const string& key, int& value) const
{
    string str;

    if (Get(key, str))
    {
        value = atoi(str.c_str());
        return true;
    }
    else
    {
        return false;
    }
}

bool Configuration::Get(const string& key, long& value) const
{
    string str;

    if (Get(key, str))
    {
        value = atol(str.c_str());
        return true;
    }
    else
    {
        return false;
    }
}

bool Configuration::Get(const string& key, double& value) const
{
    string str;

    if (Get(key, str))
    {
        value = atof(str.c_str());
        return true;
    }
    else
    {
        return false;
    }
}

bool Configuration::Get(const string& key, bool& value) const
{
    string str;

    if (Get(key, str))
    {
        value = (str == "true");
        return true;
    }
    else
    {
        return false;
    }
}

string Configuration::Trim(const string& str)
{
    unsigned int first = str.find_first_not_of(" \t");

    if (first != string::npos)
    {
        unsigned int last = str.find_last_not_of(" \t");

        return str.substr(first, last - first + 1);
    }
    else
    {
        return "";
    }
}

string Configuration::Stripe(const string& str)
{
    unsigned int first = str.find_first_not_of(" \t");

    if (first != string::npos)
    {
        unsigned int last = str.find_last_not_of(" \t");

        return str.substr(first, last - first + 1);
    }
    else
    {
        return "";
    }
}

string Configuration::Ltrim(string& s)
{
    s.erase(s.begin(), find_if(s.begin(), s.end(), not1(ptr_fun<int, int>(isspace))));
    return s;
}

string Configuration::Rtrim(string& s)
{
    s.erase(find_if(s.rbegin(), s.rend(), not1(ptr_fun<int, int>(isspace))).base(), s.end());
    return s;
}

string Configuration::Trimm(string& s)
{
    string c = Rtrim(s);
    return Ltrim(c);
}

string &Configuration::ToString(char *c)
{
    stringstream ss;
    ss << c;
    ss >> s;
    return s;
}

int Configuration::WriteToCmdfile(const string& file,char *cmd) {
    ofstream cmdfile;
    cmdfile.open(file.c_str(), fstream::out);
    cmdfile << cmd;
    cmdfile.close();
    return 0;
}
