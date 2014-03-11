#ifndef ID2SC_H
#define ID2SC_H

#include <iostream>
#include <fstream>
#include <assert.h>
#include <zdb.h>
#include <vector>
#include <string>
#include <ctime>
#include <sys/types.h>
#include <sys/stat.h>

#include "../nagios/config.h"
#include "../nagios/objects.h"
#include "../nagios/nagios.h"
#include "../nagios/nebstructs.h"
#include "../nagios/neberrors.h"
#include "../nagios/broker.h"
#include "../nagios/nebmodules.h"
#include "../nagios/nebcallbacks.h"
#include "../nagios/protoapi.h"

using namespace std;

/* specify event broker API version (required) */
NEB_API_VERSION(CURRENT_NEB_API_VERSION);

#define MAX_BUFLEN	49152
#define MAX_TEXT_LEN	32768

void *id2sc_module_handle = NULL;
int id2sc_handle_data(int, void *);
ofstream debugfile;
int process_module_args(char *);
Configuration config;
string dbgfile;
string commandfile;
string configfile;
string debug;
char temp_buffer[8192];
string idname;
string identifier;
string pgurl;
string indpath;
string binpath;
ConnectionPool_T pool;
URL_T url_t;
Connection_T con;
int instid;
string temp[1010];
int cc=0;
int dd=0;
extern "C" char *escape_buffer(char *);
int MonitoringTask();
int UpdateConfiguration();
int CheckConfigMtime();
int UpdateConfigFile(int, char *);
time_t lastts = time(NULL);
int kk=0;

#endif