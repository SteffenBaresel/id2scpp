/*
 * ID2SC.cpp
 *
 * C++ Icinga Broker Module to push Icinga Data to System Control Data Source
 *
 * 01.2014 - Steffen Baresel :: initiale Version
 *
 */

#define NSCORE

#include "../inc/configuration.h"
#include "../inc/id2sc.h"

extern "C" int nebmodule_init(int flags, char *args, nebmodule *handle) {
    /* save our handle */
    id2sc_module_handle = handle;

    write_to_all_logs("id2sc: Steffen Baresel SIV.AG 2013 Mail: kvasysystemcontrol@siv.de", NSLOG_INFO_MESSAGE);
    write_to_all_logs("id2sc: startup ...", NSLOG_INFO_MESSAGE);

    /* process module args */
    if (process_module_args(args) == 1) {
	write_to_all_logs("id2sc: Load configuration failed.", NSLOG_INFO_MESSAGE);
	return -1;
    } else {
	write_to_all_logs("id2sc: Load configuration done.", NSLOG_INFO_MESSAGE);
    }

    snprintf(temp_buffer, sizeof(temp_buffer) - 1, "id2sc: Parameter :: id.name -> %s\n[%d] id2sc: Parameter :: id.idtf -> %s\n[%d] id2sc: Parameter :: lg.file -> %s\n[%d] id2sc: Parameter :: debug -> %s\n[%d] id2sc: Parameter :: pg.url -> %s\n[%d] id2sc: Parameter :: command.file -> %s", idname.c_str(), (int)time(NULL), identifier.c_str(), (int)time(NULL), dbgfile.c_str(), (int)time(NULL), debug.c_str(), (int)time(NULL), pgurl.c_str(), (int)time(NULL), commandfile.c_str());
    temp_buffer[sizeof(temp_buffer)-1] = '\x0';
    write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);

    /* Initialize Connection Pool */
    url_t = URL_new(pgurl.c_str());
    pool = ConnectionPool_new(url_t);
    ConnectionPool_setInitialConnections(pool, 10);
    ConnectionPool_setMaxConnections(pool, 50);
    ConnectionPool_setConnectionTimeout(pool, 4);
    ConnectionPool_setReaper(pool, 4);
    ConnectionPool_start(pool);

    write_to_all_logs("id2sc: Connection Pool initialized.", NSLOG_INFO_MESSAGE);

    /* Register Instance */
    con = ConnectionPool_getConnection(pool);
    TRY {
        PreparedStatement_T pre = Connection_prepareStatement(con, "SELECT instid FROM monitoring_info_instance WHERE instna=? AND identifier=?");
        PreparedStatement_setString(pre, 1, idname.c_str());
        PreparedStatement_setString(pre, 2, identifier.c_str());
        ResultSet_T instance = PreparedStatement_executeQuery(pre);
        if (ResultSet_next(instance)) {
	    instid = ResultSet_getIntByName(instance, "instid");
        } else {
	    PreparedStatement_T ins = Connection_prepareStatement(con, "INSERT INTO monitoring_info_instance(INSTNA,IDENTIFIER) VALUES (?,?)");
	    PreparedStatement_setString(ins, 1, idname.c_str());
	    PreparedStatement_setString(ins, 2, identifier.c_str());
	    PreparedStatement_execute(ins);
	    /* select instid */
	    PreparedStatement_T pre2 = Connection_prepareStatement(con, "SELECT instid FROM monitoring_info_instance WHERE instna=? AND identifier=?");
    	    PreparedStatement_setString(pre2, 1, idname.c_str());
    	    PreparedStatement_setString(pre2, 2, identifier.c_str());
    	    ResultSet_T instance2 = PreparedStatement_executeQuery(pre2);
    	    if (ResultSet_next(instance2)) {
    		instid = ResultSet_getIntByName(instance2, "instid");
    	    }
        }
        snprintf(temp_buffer, sizeof(temp_buffer) - 1, "id2sc: Monitoring Instance registered. INSTID: %d\n", instid);
        temp_buffer[sizeof(temp_buffer)-1] = '\x0';
        write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);
    } CATCH(SQLException) {
        snprintf(temp_buffer, sizeof(temp_buffer) - 1, "id2sc: SQLException - %s\n", Exception_frame.message);
        temp_buffer[sizeof(temp_buffer)-1] = '\x0';
        write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);
        if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] INSTANCE: REGISTER_INSTANCE SQLException - " << Exception_frame.message << endl; }
    } FINALLY {
        Connection_close(con);
    } END_TRY;

    /* open debugfile */

    debugfile.open(dbgfile.c_str(), fstream::out | fstream::app);

    if (debug.compare("on") == 0) {
	debugfile << "[" << time(NULL) << "] INFO: Steffen Baresel SIV.AG 2013 Mail: kvasysystemcontrol@siv.de" << endl;
	debugfile << "[" << time(NULL) << "] DEAMON: Startup success." << endl;
	debugfile << "[" << time(NULL) << "] LOGFILE: Open '" << dbgfile << "' success." << endl;
	debugfile << "[" << time(NULL) << "] CALLBACKS: Register ..." << endl;
    }

    /* register callbacks */
    neb_register_callback(NEBCALLBACK_SERVICE_STATUS_DATA, id2sc_module_handle, 0, id2sc_handle_data);
    neb_register_callback(NEBCALLBACK_HOST_STATUS_DATA, id2sc_module_handle, 0, id2sc_handle_data);
    neb_register_callback(NEBCALLBACK_PROGRAM_STATUS_DATA, id2sc_module_handle, 0, id2sc_handle_data);
    neb_register_callback(NEBCALLBACK_DOWNTIME_DATA, id2sc_module_handle, 0, id2sc_handle_data);

    if (debug.compare("on") == 0) {
	debugfile << "[" << time(NULL) << "] CALLBACKS: ... done!" << endl;
	debugfile << "[" << time(NULL) << "] ########################################################################### " << endl;
	debugfile << "[" << time(NULL) << "] #                                                                         # " << endl;
	debugfile << "[" << time(NULL) << "] #                               OUTPUT                                    # " << endl;
	debugfile << "[" << time(NULL) << "] #                                                                         # " << endl;
	debugfile << "[" << time(NULL) << "] ########################################################################### " << endl;
    }

    return 0;
}

extern "C" int nebmodule_deinit(int flags, int reason) {

    if (debug.compare("on") == 0) {
	debugfile << "[" << time(NULL) << "] ########################################################################### " << endl;
	debugfile << "[" << time(NULL) << "] #                                                                         # " << endl;
	debugfile << "[" << time(NULL) << "] #                                 END                                     # " << endl;
	debugfile << "[" << time(NULL) << "] #                                                                         # " << endl;
	debugfile << "[" << time(NULL) << "] ########################################################################### " << endl;
	debugfile << "[" << time(NULL) << "] CALLBACKS: DeRegister ..." << endl;
    }

    /* deregister callbacks */
    neb_deregister_callback(NEBCALLBACK_SERVICE_STATUS_DATA, id2sc_handle_data);
    neb_deregister_callback(NEBCALLBACK_HOST_STATUS_DATA, id2sc_handle_data);
    neb_deregister_callback(NEBCALLBACK_PROGRAM_STATUS_DATA, id2sc_handle_data);
    neb_deregister_callback(NEBCALLBACK_DOWNTIME_DATA, id2sc_handle_data);

    if (debug.compare("on") == 0) {
	debugfile << "[" << time(NULL) << "] CALLBACKS: ... done!" << endl;
    }

    debugfile.close();

    ConnectionPool_stop(pool);
    ConnectionPool_free(&pool);
    URL_free(&url_t);

    write_to_all_logs("id2sc: Connection Pool deinitialized.", NSLOG_INFO_MESSAGE);

    write_to_all_logs("id2sc: shutdown ...", NSLOG_INFO_MESSAGE);

    return 0;
}

int id2sc_handle_data(int event_type, void *data) {
    nebstruct_service_status_data *ssdata = NULL;
    nebstruct_host_status_data *hsdata = NULL;
    nebstruct_program_status_data *psdata = NULL;
    nebstruct_downtime_data *dtdata = NULL;
    service *temp_service = NULL;
    host *temp_host = NULL;
    string es[100];
    char *ec[100];
    string type;
    string mode;
    string name;
    Configuration s;
    char *array[101];
    int i;

    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] EVENT: " << event_type << endl; }

    switch (event_type) {
	case NEBCALLBACK_SERVICE_STATUS_DATA:
	    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] EVENT-SWITCH: NEBCALLBACK_SERVICE_STATUS_DATA " << endl; }
	    if ((ssdata = (nebstruct_service_status_data *)data)) {
		type = "tbd"; mode = "tbd"; name = "tbd"; /* tbd = to be defined */ int hstid=0000; int srvid=0000; int timestamp = (int)time(NULL);
		if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] EVENT-SWITCH-IN: NEBCALLBACK_SERVICE_STATUS_DATA " << endl; }
		/* get data from struct */
		temp_service = (service *)ssdata->object_ptr;
		temp_host = (host *)temp_service->host_ptr;
		es[0] = escape_buffer(temp_service->host_name);
		es[1] = escape_buffer(temp_service->display_name);
		ec[2] = escape_buffer(temp_service->plugin_output);
		ec[3] = escape_buffer(temp_service->long_plugin_output);
		ec[4] = escape_buffer(temp_service->perf_data);
		es[5] = s.Trimm(s.ToString(temp_service->check_period));
		es[8] = s.Trimm(s.ToString(temp_host->address));
		es[7] = es[1].substr(es[1].find_first_of("_")+1);
		
		if(ec[4] != NULL) {
		    if(strlen(ec[4]) > MAX_TEXT_LEN) {
			ec[4][MAX_TEXT_LEN] = '\0';
		    }
		}
		
		if(ec[3] != NULL) {
		    if(strlen(ec[3]) > MAX_TEXT_LEN) {
			ec[3][MAX_TEXT_LEN] = '\0';
		    }
		}
		
		if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] EVENT-SWITCH-SERVICE_STATUS: " << es[1] << endl; }
		/* deformat service description string: <TYPE>_<MODE>_<NAME>*/
		type = es[1].substr(0,es[1].find_first_of("_"));
		mode = es[7].substr(0, es[7].find_first_of("_"));
		name = es[7].substr(es[7].find_first_of("_")+1);
		
		if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] EVENT-SWITCH_PRE: temp[100] = " << temp[100] << " :: name = " << name << " :: cc = " << cc << endl; }
		
		if(temp[100].empty()) { cc = 0; temp[100] = name; } else { if(temp[100].compare(name) != 0) { cc = 0; temp[100] = name; } else { cc++; } }
		
		if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] EVENT-SWITCH_POST: temp[100] = " << temp[100] << " :: name = " << name << " :: cc = " << cc << endl; }
		
		/* end */
		if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] EVENT-SWITCH-SERVICE_STATUS-SPLIT: type = " << type << " :: mode = " << mode << " :: name = " << name << endl; }
		/* basic actions */
		con = ConnectionPool_getConnection(pool);
		TRY {
		    /* Get Host ID */
		    PreparedStatement_T shsd = Connection_prepareStatement(con, "SELECT hstid FROM monitoring_info_host WHERE instid=? AND hstln=? AND ipaddr=?");
		    PreparedStatement_setInt(shsd, 1, instid);
		    PreparedStatement_setString(shsd, 2, es[0].c_str());
		    PreparedStatement_setString(shsd, 3, es[8].c_str());
		    ResultSet_T instance1 = PreparedStatement_executeQuery(shsd);
		    if (ResultSet_next(instance1)) {
		        hstid = ResultSet_getIntByName(instance1, "hstid");
		    } else {
		        /* Insert Host Entry */
		        PreparedStatement_T ihsd = Connection_prepareStatement(con, "INSERT INTO monitoring_info_host(HSTLN,IPADDR,HTYPID,DSC,INSTID,CHECK_PERIOD,CREATED) VALUES (?,?,?,?,?,?,?)");
		        PreparedStatement_setString(ihsd, 1, es[0].c_str());
		        PreparedStatement_setString(ihsd, 2, es[8].c_str());
		        PreparedStatement_setInt(ihsd, 3, 1);
		        PreparedStatement_setString(ihsd, 4, "-");
		        PreparedStatement_setInt(ihsd, 5, instid);
		        PreparedStatement_setString(ihsd, 6, es[5].c_str());
		        PreparedStatement_setInt(ihsd, 7, timestamp);
		        PreparedStatement_execute(ihsd);
		        /* Select Host ID */
		        PreparedStatement_T shsd2 = Connection_prepareStatement(con, "SELECT hstid FROM monitoring_info_host WHERE instid=? AND hstln=? AND ipaddr=?");
		        PreparedStatement_setInt(shsd2, 1, instid);
		        PreparedStatement_setString(shsd2, 2, es[0].c_str());
		        PreparedStatement_setString(shsd2, 3, es[8].c_str());
		        ResultSet_T instance12 = PreparedStatement_executeQuery(shsd2);
		        if (ResultSet_next(instance12)) {
		    	    hstid = ResultSet_getIntByName(instance12, "hstid");
		        }
		    }
		    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] EVENT-SWITCH-SQL: Get Host ID :: " << hstid << endl; }
		    /* Service */
		    PreparedStatement_T shsrvd = Connection_prepareStatement(con, "SELECT srvid FROM monitoring_info_service WHERE instid=? AND hstid=? AND srvna=?");
		    PreparedStatement_setInt(shsrvd, 1, instid);
		    PreparedStatement_setInt(shsrvd, 2, hstid);
		    PreparedStatement_setString(shsrvd, 3, es[1].c_str());
		    ResultSet_T instance2 = PreparedStatement_executeQuery(shsrvd);
		    if (ResultSet_next(instance2)) {
		        srvid = ResultSet_getIntByName(instance2, "srvid");
		    } else {
		        /* Insert Service Entry */
		        PreparedStatement_T ihsrvd = Connection_prepareStatement(con, "INSERT INTO monitoring_info_service(HSTID,SRVNA,DSC,INSTID,CHECK_PERIOD,CREATED) VALUES (?,?,?,?,?,?)");
		        PreparedStatement_setInt(ihsrvd, 1, hstid);
		        PreparedStatement_setString(ihsrvd, 2, es[1].c_str());
		        PreparedStatement_setString(ihsrvd, 3, "-");
		        PreparedStatement_setInt(ihsrvd, 4, instid);
		        PreparedStatement_setString(ihsrvd, 5, es[5].c_str());
		        PreparedStatement_setInt(ihsrvd, 6, timestamp);
		        PreparedStatement_execute(ihsrvd);
		        /* Select Service ID */
		        PreparedStatement_T shsrvd2 = Connection_prepareStatement(con, "SELECT srvid FROM monitoring_info_service WHERE instid=? AND hstid=? AND srvna=?");
		        PreparedStatement_setInt(shsrvd2, 1, instid);
		        PreparedStatement_setInt(shsrvd2, 2, hstid);
		        PreparedStatement_setString(shsrvd2, 3, es[1].c_str());
		        ResultSet_T instance22 = PreparedStatement_executeQuery(shsrvd2);
		        if (ResultSet_next(instance22)) {
			    srvid = ResultSet_getIntByName(instance22, "srvid");
		        }
		    }
		    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] EVENT-SWITCH-SQL: Get Service ID :: " << srvid << endl; }
		    /* Insert or Update Status Table */
		    PreparedStatement_T smise = Connection_prepareStatement(con, "SELECT sid FROM monitoring_status WHERE srvid=?");
		    PreparedStatement_setInt(smise, 1, srvid);
		    ResultSet_T instance3 = PreparedStatement_executeQuery(smise);
		    if (ResultSet_next(instance3)) {
			PreparedStatement_T uhpd = Connection_prepareStatement(con, "UPDATE monitoring_status SET OUTPUT=encode(?,'base64'), LONG_OUTPUT=encode(?,'base64'), CURRENT_STATE=?, LAST_STATE=?, LAST_CHECK=?, NEXT_CHECK=?, LAST_TIME_OK=?, LAST_TIME_WA=?, LAST_TIME_CR=?, LAST_TIME_UN=?, PERCENT_STATE_CHANGE=?, PERF_DATA=encode(?,'base64'), CREATED=? WHERE sid=?");
			PreparedStatement_setString(uhpd, 1, ec[2]);
			PreparedStatement_setString(uhpd, 2, ec[3]);
			PreparedStatement_setInt(uhpd, 3, temp_service->current_state);
			PreparedStatement_setInt(uhpd, 4, temp_service->last_state);
			PreparedStatement_setInt(uhpd, 5, temp_service->last_check);
			PreparedStatement_setInt(uhpd, 6, temp_service->next_check);
			PreparedStatement_setInt(uhpd, 7, temp_service->last_time_ok);
			PreparedStatement_setInt(uhpd, 8, temp_service->last_time_warning);
			PreparedStatement_setInt(uhpd, 9, temp_service->last_time_critical);
			PreparedStatement_setInt(uhpd, 10, temp_service->last_time_unknown);
			PreparedStatement_setInt(uhpd, 11, temp_service->percent_state_change);
			PreparedStatement_setString(uhpd, 12, ec[4]);
			PreparedStatement_setInt(uhpd, 13, timestamp);
			PreparedStatement_setInt(uhpd, 14, ResultSet_getIntByName(instance3, "sid"));
			PreparedStatement_execute(uhpd);
			/* Fill Status History table */
			PreparedStatement_T ihhd = Connection_prepareStatement(con, "INSERT INTO monitoring_status_history(HSTID,SRVID,OUTPUT,LONG_OUTPUT,CURRENT_STATE,LAST_STATE,PERF_DATA,CREATED) VALUES (?,?,encode(?,'base64'),encode(?,'base64'),?,?,encode(?,'base64'),?)");
			PreparedStatement_setInt(ihhd, 1, hstid);
			PreparedStatement_setInt(ihhd, 2, srvid);
			PreparedStatement_setString(ihhd, 3, ec[2]);
			PreparedStatement_setString(ihhd, 4, ec[3]);
			PreparedStatement_setInt(ihhd, 5, temp_service->current_state);
			PreparedStatement_setInt(ihhd, 6, temp_service->last_state);
			PreparedStatement_setString(ihhd, 7, ec[4]);
			PreparedStatement_setInt(ihhd, 8, timestamp);
			PreparedStatement_execute(ihhd);
			/* check for state change */
			if ( (temp_service->current_state != temp_service->last_state) && (temp_service->problem_has_been_acknowledged == 0) ) {
			    PreparedStatement_T msc = Connection_prepareStatement(con, "INSERT INTO monitoring_state_change(HSTID,SRVID,STATE,LAST_STATE,OUTPUT,NEW_PROBLEM,MAIL,CREATED) VALUES (?,?,?,?,encode(?,'base64'),?,?,?)");
			    PreparedStatement_setInt(msc, 1, hstid);
			    PreparedStatement_setInt(msc, 2, srvid);
			    PreparedStatement_setInt(msc, 3, temp_service->current_state);
			    PreparedStatement_setInt(msc, 4, temp_service->last_state);
			    PreparedStatement_setString(msc, 5, ec[2]);
			    PreparedStatement_setInt(msc, 6, 1);
			    PreparedStatement_setInt(msc, 7, 0);
			    PreparedStatement_setInt(msc, 8, timestamp);
			    PreparedStatement_execute(msc);
			    /* Remove Acknowledgement */
			    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] EVENT-SWITCH-SERVICE_STATUS STATE_CHANGE: Remove Acknowledgement cs " << temp_service->current_state << " != ls " << temp_service->last_state << " phba == 0 :: " << temp_service->problem_has_been_acknowledged << endl; }
			    PreparedStatement_T msc2 = Connection_prepareStatement(con, "UPDATE monitoring_status SET ack=false, ackid='0' WHERE srvid=?");
			    PreparedStatement_setInt(msc2, 1, srvid);
			    PreparedStatement_execute(msc2);
			    /* Prepare for Mailing */
			    int mtyp;
			    switch (temp_service->last_state) {
				case 0:
				    mtyp=3;
				    break;
				case 1:
				    if (temp_service->current_state == 0) { mtyp = 4; } else { mtyp = 3; }
				    break;
				case 2:
				    if (temp_service->current_state == 0) { mtyp = 4; } else { mtyp = 3; }
				    break;
				case 3:
				    if (temp_service->current_state == 0) { mtyp = 4; } else { mtyp = 3; }
				    break;
				default:
				    mtyp=3;
				    break;
			    }
			    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] EVENT-SWITCH-SERVICE_STATUS STATE_CHANGE: Fill Monitoring Mailing" << endl; }
			    PreparedStatement_T pfm = Connection_prepareStatement(con, "INSERT INTO monitoring_mailing(DONE,HSTID,SRVID,MTYPID,USR,COMMENT,APPID,T1,T2,CREATED) VALUES (false,?,?,?,encode('system','base64'),encode(?,'base64'),'0','0','0',?)");
			    PreparedStatement_setInt(pfm, 1, hstid);
			    PreparedStatement_setInt(pfm, 2, srvid);
			    PreparedStatement_setInt(pfm, 3, mtyp);
			    PreparedStatement_setString(pfm, 4, ec[2]);
			    PreparedStatement_setInt(pfm, 5, timestamp);
			    PreparedStatement_execute(pfm);
			}
		    } else {
			PreparedStatement_T ihpd = Connection_prepareStatement(con, "INSERT INTO monitoring_status(HSTID,SRVID,OUTPUT,LONG_OUTPUT,CURRENT_STATE,LAST_STATE,LAST_CHECK,NEXT_CHECK,LAST_TIME_OK,LAST_TIME_WA,LAST_TIME_CR,LAST_TIME_UN,PERCENT_STATE_CHANGE,PERF_DATA,ACK,ACKID,DTM,DTMID,CREATED) VALUES (?,?,encode(?,'base64'),encode(?,'base64'),?,?,?,?,?,?,?,?,?,encode(?,'base64'),false,'0',false,'0',?)");
			PreparedStatement_setInt(ihpd, 1, hstid);
			PreparedStatement_setInt(ihpd, 2, srvid);
			PreparedStatement_setString(ihpd, 3, ec[2]);
			PreparedStatement_setString(ihpd, 4, ec[3]);
			PreparedStatement_setInt(ihpd, 5, temp_service->current_state);
			PreparedStatement_setInt(ihpd, 6, temp_service->last_state);
			PreparedStatement_setInt(ihpd, 7, temp_service->last_check);
			PreparedStatement_setInt(ihpd, 8, temp_service->next_check);
			PreparedStatement_setInt(ihpd, 9, temp_service->last_time_ok);
			PreparedStatement_setInt(ihpd, 10, temp_service->last_time_warning);
			PreparedStatement_setInt(ihpd, 11, temp_service->last_time_critical);
			PreparedStatement_setInt(ihpd, 12, temp_service->last_time_unknown);
			PreparedStatement_setInt(ihpd, 13, temp_service->percent_state_change);
			PreparedStatement_setString(ihpd, 14, ec[4]);
			PreparedStatement_setInt(ihpd, 15, timestamp);
			PreparedStatement_execute(ihpd);
			/* Fill Status History table */
			PreparedStatement_T ihhd = Connection_prepareStatement(con, "INSERT INTO monitoring_status_history(HSTID,SRVID,OUTPUT,LONG_OUTPUT,CURRENT_STATE,LAST_STATE,PERF_DATA,CREATED) VALUES (?,?,encode(?,'base64'),encode(?,'base64'),?,?,encode(?,'base64'),?)");
			PreparedStatement_setInt(ihhd, 1, hstid);
			PreparedStatement_setInt(ihhd, 2, srvid);
			PreparedStatement_setString(ihhd, 3, ec[2]);
			PreparedStatement_setString(ihhd, 4, ec[3]);
			PreparedStatement_setInt(ihhd, 5, temp_service->current_state);
			PreparedStatement_setInt(ihhd, 6, temp_service->last_state);
			PreparedStatement_setString(ihhd, 7, ec[4]);
			PreparedStatement_setInt(ihhd, 8, timestamp);
			PreparedStatement_execute(ihhd);
			/* check for state change */
			if ( (temp_service->current_state > 0) && (temp_service->problem_has_been_acknowledged == 0) ) {
			    PreparedStatement_T msc = Connection_prepareStatement(con, "INSERT INTO monitoring_state_change(HSTID,SRVID,STATE,LAST_STATE,OUTPUT,NEW_PROBLEM,MAIL,CREATED) VALUES (?,?,?,?,encode(?,'base64'),?,?,?)");
			    PreparedStatement_setInt(msc, 1, hstid);
			    PreparedStatement_setInt(msc, 2, srvid);
			    PreparedStatement_setInt(msc, 3, temp_service->current_state);
			    PreparedStatement_setInt(msc, 4, 0);
			    PreparedStatement_setString(msc, 5, ec[2]);
			    PreparedStatement_setInt(msc, 6, 1);
			    PreparedStatement_setInt(msc, 7, 0);
			    PreparedStatement_setInt(msc, 8, timestamp);
			    PreparedStatement_execute(msc);
			    /* Remove Acknowledgement */
			    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] EVENT-SWITCH-SERVICE_STATUS STATE_CHANGE: Remove Acknowledgement cs > 0 && phba == 0" << endl; }
			    PreparedStatement_T msc2 = Connection_prepareStatement(con, "UPDATE monitoring_status SET ack=false, ackid='0' WHERE srvid=?");
			    PreparedStatement_setInt(msc2, 1, srvid);
			    PreparedStatement_execute(msc2);
			    /* Prepare for Mailing */
			    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] EVENT-SWITCH-SERVICE_STATUS STATE_CHANGE: Fill Monitoring Mailing" << endl; }
			    PreparedStatement_T pfm = Connection_prepareStatement(con, "INSERT INTO monitoring_mailing(DONE,HSTID,SRVID,MTYPID,USR,COMMENT,APPID,T1,T2,CREATED) VALUES (false,?,?,?,encode('system','base64'),encode(?,'base64'),'0','0','0',?)");
			    PreparedStatement_setInt(pfm, 1, hstid);
			    PreparedStatement_setInt(pfm, 2, srvid);
			    PreparedStatement_setInt(pfm, 3, 4);
			    PreparedStatement_setString(pfm, 4, ec[2]);
			    PreparedStatement_setInt(pfm, 5, timestamp);
			    PreparedStatement_execute(pfm);
			}
		    }
		    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] EVENT-SWITCH-SQL: Update Status Table " << endl; }
		    /* Check if Passiv Check */
		    int next_check=0; int last_check=0;
		    if (temp_service->check_type == 0) {
		        next_check = temp_service->next_check;
		        last_check = temp_service->last_check;
		    } else {
		        PreparedStatement_T slac2 = Connection_prepareStatement(con, "SELECT created FROM monitoring_availability WHERE srvid=? ORDER BY 1 DESC LIMIT 1");
		        PreparedStatement_setInt(slac2, 1, srvid);
		        ResultSet_T instanceS = PreparedStatement_executeQuery(slac2);
		        if (ResultSet_next(instanceS)) {
		            next_check = timestamp;
		            last_check = ResultSet_getIntByName(instanceS, "created");
		        } else {
		            next_check = temp_service->last_check;
		            last_check = temp_service->last_check;
		        }
		    }
		    /* Get Durations */
		    int timeok=0; int timewa=0; int timecr=0; int timeun=0;
		    switch (temp_service->current_state) {
		        case 0:
			    timeok = next_check - last_check;
			    break;
		        case 1:
			    timewa = next_check - last_check;
			    break;
		        case 2:
			    timecr = next_check - last_check;
			    break;
		        case 3:
			    timeun = next_check - last_check;
			    break;
		        default:
			    break;
		    }
		    /* Update Availability Table */
		    PreparedStatement_T smase = Connection_prepareStatement(con, "SELECT aid FROM monitoring_availability WHERE srvid=? AND created=?");
		    PreparedStatement_setInt(smase, 1, srvid);
		    PreparedStatement_setInt(smase, 2, timestamp);
		    ResultSet_T instance4 = PreparedStatement_executeQuery(smase);
		    if (ResultSet_next(instance4)) {
		        /* nothing temp_service->last_check */
		    } else {
		        PreparedStatement_T ihad = Connection_prepareStatement(con, "INSERT INTO monitoring_availability(SRVID,TIMEOK,TIMEWA,TIMECR,TIMEUN,CREATED) VALUES (?,?,?,?,?,?)");
		        PreparedStatement_setInt(ihad, 1, srvid);
		        PreparedStatement_setInt(ihad, 2, timeok);
		        PreparedStatement_setInt(ihad, 3, timewa);
		        PreparedStatement_setInt(ihad, 4, timecr);
		        PreparedStatement_setInt(ihad, 5, timeun);
		        PreparedStatement_setInt(ihad, 6, timestamp);
		        PreparedStatement_execute(ihad);
		    }
		    /* select on type  */
		    /* Oracle Database */
		    if ( (ec[4] != NULL) && (strlen(ec[4])>5) ) {
			if (type.compare("Check") == 0) {
			    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_Check_MK_START: mode = " << mode << " :: " << ec[2] << endl; }
			    Configuration st;
			    if (mode.find("MK") != string::npos) {
				string key; string val; string entry; temp[75] = ec[2];
				key = temp[75].substr(temp[75].find_first_of("-")+16);
				val = key.substr(0, key.find_first_of(","));
				if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_Check_MK_START: val = " << val << endl; }
				PreparedStatement_T sysi = Connection_prepareStatement(con, "UPDATE monitoring_info_host SET agent_version=? WHERE hstid=?");
				PreparedStatement_setString(sysi, 1, val.c_str());
				PreparedStatement_setInt(sysi, 2, hstid);
				PreparedStatement_execute(sysi);
			    } else {
				/* nothing */
			    }
			} else if (type.compare("MEMORY") == 0) {
			    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_MEMORY_START: mode = " << mode << " :: " << ec[4] << endl; }
			    Configuration st; i=0; array[i] = strtok(ec[4], " "); while(array[i]!=NULL) { array[++i] = strtok(NULL," "); }
			    for (int j=0;j<100;j++) {
				string key; string val; string element = st.ToString(array[j]); string usage; string size;
				key = element.substr(0,element.find_first_of("="));
				val = element.substr(element.find_first_of("=")+1);
				usage = val.substr(0,val.find_first_of(";"));
				usage = usage.substr(0, usage.size()-2);
				size = val.substr(val.find_last_of(";")+1);
				if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_FILESYSTEM_KV: element = " << element << " :: key = " << key << " :: val = " << val << endl; }
				PreparedStatement_T sysi = Connection_prepareStatement(con, "INSERT INTO monitoring_performance(HSTID,SRVID,CLASS,KEY,NAME,DSC,USAGE,ALLOC,CREATED) VALUES (?,?,?,?,?,?,?,?,?)");
				PreparedStatement_setInt(sysi, 1, hstid);
				PreparedStatement_setInt(sysi, 2, srvid);
				PreparedStatement_setString(sysi, 3, "MEM");
				PreparedStatement_setString(sysi, 4, "ALLOC");
				PreparedStatement_setString(sysi, 5, name.c_str());
				PreparedStatement_setString(sysi, 6, key.c_str());
				PreparedStatement_setString(sysi, 7, usage.c_str());
				PreparedStatement_setString(sysi, 8, size.c_str());
				PreparedStatement_setInt(sysi, 9, timestamp);
				PreparedStatement_execute(sysi);
				/* Insert Host OS */
				if(key.compare("pagefile") == 0) {
				    PreparedStatement_T sysi = Connection_prepareStatement(con, "UPDATE monitoring_info_host SET os=? WHERE hstid=?");
				    PreparedStatement_setString(sysi, 1, "Windows");
				    PreparedStatement_setInt(sysi, 2, hstid);
				    PreparedStatement_execute(sysi);
				} else if(key.compare("swapused") == 0) {
				    PreparedStatement_T sysi = Connection_prepareStatement(con, "UPDATE monitoring_info_host SET os=? WHERE hstid=?");
				    PreparedStatement_setString(sysi, 1, "Unix/Linux");
				    PreparedStatement_setInt(sysi, 2, hstid);
				    PreparedStatement_execute(sysi);
				} else {
				    /* nothing */
				}
				if(array[j+1] == NULL) { break; }
			    }
			} else if (type.compare("CPU") == 0) {
			    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_CPU_START: mode = " << mode << " :: " << ec[4] << endl; }
			    Configuration st; i=0; array[i] = strtok(ec[4], " "); while(array[i]!=NULL) { array[++i] = strtok(NULL," "); }
			    for (int j=0;j<100;j++) {
				string key; string val; string element = st.ToString(array[j]); string usage; string size;
				key = element.substr(0,element.find_first_of("="));
				val = element.substr(element.find_first_of("=")+1);
				usage = val.substr(0,val.find_first_of(";"));
				usage = usage.substr(0, usage.size()-2);
				size = val.substr(val.find_last_of(";")+1);
				if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_FILESYSTEM_KV: element = " << element << " :: key = " << key << " :: val = " << val << endl; }
				PreparedStatement_T sysi = Connection_prepareStatement(con, "INSERT INTO monitoring_performance(HSTID,SRVID,CLASS,KEY,NAME,DSC,USAGE,ALLOC,CREATED) VALUES (?,?,?,?,?,?,?,?,?)");
				PreparedStatement_setInt(sysi, 1, hstid);
				PreparedStatement_setInt(sysi, 2, srvid);
				PreparedStatement_setString(sysi, 3, "CPU");
				PreparedStatement_setString(sysi, 4, "USAGE");
				PreparedStatement_setString(sysi, 5, name.c_str());
				PreparedStatement_setString(sysi, 6, key.c_str());
				PreparedStatement_setString(sysi, 7, usage.c_str());
				PreparedStatement_setString(sysi, 8, size.c_str());
				PreparedStatement_setInt(sysi, 9, timestamp);
				PreparedStatement_execute(sysi);
				if(array[j+1] == NULL) { break; }
			    }
			} else if (type.compare("SYSTEM") == 0) {
			    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_SYSTEM_START: mode = " << mode << " :: " << ec[4] << endl; }
			    Configuration st; i=0; array[i] = strtok(ec[4], " "); while(array[i]!=NULL) { array[++i] = strtok(NULL," "); }
			    if (mode.find("Uptime") != string::npos) {
				for (int j=0;j<100;j++) {
				    string key; string val; string element = st.ToString(array[j]); string entry;
				    key = element.substr(0,element.find_first_of("="));
				    val = element.substr(element.find_first_of("=")+1);
				    entry = val.substr(0,val.find_first_of(";"));
				    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_SYSTEM_KV: element = " << element << " :: key = " << key << " :: val = " << val << endl; }
				    PreparedStatement_T sysi = Connection_prepareStatement(con, "UPDATE monitoring_info_host SET uptime=? WHERE hstid=?");
				    PreparedStatement_setString(sysi, 1, entry.c_str());
				    PreparedStatement_setInt(sysi, 2, hstid);
				    PreparedStatement_execute(sysi);
				    if(array[j+1] == NULL) { break; }
				}
			    } else {
				/* Nothing */
			    }
			} else if (type.compare("FILESYSTEM") == 0) {
			    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_FILESYSTEM_START: mode = " << mode << " :: " << ec[4] << endl; }
			    Configuration st; i=0; array[i] = strtok(ec[4], " "); while(array[i]!=NULL) { array[++i] = strtok(NULL," "); }
			    /* I/O Summary */
			    if (mode.find("I/O SUMMARY") != string::npos) {
				for (int j=0;j<100;j++) {
				    string key; string val; string element = st.ToString(array[j]); string entry;
				    key = element.substr(0,element.find_first_of("="));
				    val = element.substr(element.find_first_of("=")+1);
				    entry = val.substr(0,val.find_first_of(";"));
				    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_FILESYSTEM_KV: element = " << element << " :: key = " << key << " :: val = " << val << endl; }
				    PreparedStatement_T sysi = Connection_prepareStatement(con, "INSERT INTO monitoring_performance(HSTID,SRVID,CLASS,KEY,NAME,USAGE,CREATED) VALUES (?,?,?,?,?,?,?)");
				    PreparedStatement_setInt(sysi, 1, hstid);
				    PreparedStatement_setInt(sysi, 2, srvid);
				    PreparedStatement_setString(sysi, 3, "FS");
				    PreparedStatement_setString(sysi, 4, "IO");
				    PreparedStatement_setString(sysi, 5, key.c_str());
				    PreparedStatement_setString(sysi, 6, entry.c_str());
				    PreparedStatement_setInt(sysi, 7, timestamp);
				    PreparedStatement_execute(sysi);
				    if(array[j+1] == NULL) { break; }
				}
			    /* HDDs */
			    } else {
				for (int j=0;j<100;j++) {
				    string key; string val; string element = st.ToString(array[j]); string usage; string size;
				    key = element.substr(0,element.find_first_of("="));
				    val = element.substr(element.find_first_of("=")+1);
				    usage = val.substr(0,val.find_first_of(";"));
				    usage = usage.substr(0, usage.size()-2);
				    size = val.substr(val.find_last_of(";")+1);
				    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_FILESYSTEM_KV: element = " << element << " :: key = " << key << " :: val = " << val << endl; }
				    if (key.find("growth") != string::npos) {
				    } else if(key.find("trend") != string::npos) {
				    } else {
					PreparedStatement_T sysi = Connection_prepareStatement(con, "INSERT INTO monitoring_performance(HSTID,SRVID,CLASS,KEY,NAME,DSC,USAGE,ALLOC,CREATED) VALUES (?,?,?,?,?,?,?,?,?)");
					PreparedStatement_setInt(sysi, 1, hstid);
					PreparedStatement_setInt(sysi, 2, srvid);
					PreparedStatement_setString(sysi, 3, "FS");
					PreparedStatement_setString(sysi, 4, "SPACE");
					PreparedStatement_setString(sysi, 5, name.c_str());
					PreparedStatement_setString(sysi, 6, key.c_str());
					PreparedStatement_setString(sysi, 7, usage.c_str());
					PreparedStatement_setString(sysi, 8, size.c_str());
					PreparedStatement_setInt(sysi, 9, timestamp);
					PreparedStatement_execute(sysi);
				    }
				    if(array[j+1] == NULL) { break; }
				}
			    }
			} else if ( (type.compare("OLTP") == 0) || (type.compare("ODWH") == 0) || (type.compare("OSOB") == 0) || (type.compare("OKUW") == 0) ) {
			    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE: type = " << type << endl; }
				int dbid = 0;
				/* Get Database */
				PreparedStatement_T odbs = Connection_prepareStatement(con, "SELECT dbid FROM monitoring_oracle_database_info WHERE hstid=? AND sid=? AND type=?");
				PreparedStatement_setInt(odbs, 1, hstid);
				PreparedStatement_setString(odbs, 2, name.c_str());
				PreparedStatement_setString(odbs, 3, type.c_str());
				ResultSet_T instanceODBS = PreparedStatement_executeQuery(odbs);
				if (ResultSet_next(instanceODBS)) {
				    dbid = ResultSet_getIntByName(instanceODBS, "dbid");
				} else {
				    /* Insert Database */
				    PreparedStatement_T odbi = Connection_prepareStatement(con, "INSERT INTO monitoring_oracle_database_info(HSTID,SID,TYPE,CHECK_PERIOD,CREATED) VALUES (?,?,?,?,?)");
				    PreparedStatement_setInt(odbi, 1, hstid);
				    PreparedStatement_setString(odbi, 2, name.c_str());
				    PreparedStatement_setString(odbi, 3, type.c_str());
				    PreparedStatement_setString(odbi, 4, es[5].c_str());
				    PreparedStatement_setInt(odbi, 5, timestamp);
				    PreparedStatement_execute(odbi);
				    /* Select DBID */
				    PreparedStatement_T odbs2 = Connection_prepareStatement(con, "SELECT dbid FROM monitoring_oracle_database_info WHERE hstid=? AND sid=? AND type=?");
				    PreparedStatement_setInt(odbs2, 1, hstid);
				    PreparedStatement_setString(odbs2, 2, name.c_str());
				    PreparedStatement_setString(odbs2, 3, type.c_str());
				    ResultSet_T instanceODBS2 = PreparedStatement_executeQuery(odbs2);
				    if (ResultSet_next(instanceODBS2)) {
					dbid = ResultSet_getIntByName(instanceODBS2, "dbid");
				    }
				    /* Add Dummy Status Entry */
				    PreparedStatement_T odbi2 = Connection_prepareStatement(con, "INSERT INTO monitoring_oracle_database_status(DBID,CREATED) VALUES (?,?)");
				    PreparedStatement_setInt(odbi2, 1, dbid);
				    PreparedStatement_setInt(odbi2, 2, timestamp);
				    PreparedStatement_execute(odbi2);
				}
				if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE: dbid = " << dbid << " :: perf_data = " << ec[4] << endl; }
				/* split perfdata into elements by ' ' */
				Configuration st; i=0; array[i] = strtok(ec[4], " "); while(array[i]!=NULL) { array[++i] = strtok(NULL," "); }
				
				    /* select on module */
				    if (mode.compare("DBST") == 0) {
					temp[0]="-";temp[1]="-";temp[2]="-";temp[3]="-";
					if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_START: mode = " << mode << endl; }
					for (int j=0;j<4;j++) {
					    string key; string val; string element = st.ToString(array[j]);
					    key = element.substr(0,element.find_first_of("="));
					    val = element.substr(element.find_first_of("=")+1);
					    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_KV: element = " << element << " :: key = " << key << " :: val = " << val << endl; }
					    if (key.compare("DATABASE") == 0) {
						temp[0] = val.substr(0,val.find_first_of(";"));
					    } else if (key.compare("LOGINS") == 0) {
						temp[1] = val.substr(0,val.find_first_of(";"));
					    } else if (key.compare("DB") == 0) {
						temp[2] = val.substr(0,val.find_first_of(";"));
					    } else if(key.compare("BLOCKED") == 0) {
						temp[3] = val.substr(0,val.find_first_of(";"));
					    } else {
						break;
					    }
					    /* break if NULL reached */
					    if(array[j+1]==NULL) { break; }
					}
					/* Update SRVID for DB Status Service */
					PreparedStatement_T odbi21 = Connection_prepareStatement(con, "UPDATE monitoring_oracle_database_info SET srvid=? WHERE dbid=?");
					PreparedStatement_setInt(odbi21, 1, srvid);
					PreparedStatement_setInt(odbi21, 2, dbid);
					PreparedStatement_execute(odbi21);
					/* vector element */
					
				    } else if (mode.compare("RMAN") == 0) {
					temp[4]="00";temp[5]="00";
					if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_START: mode = " << mode << endl; }
					for (int j=0;j<100;j++) {
					    string key; string val; string element = st.ToString(array[j]);
					    key = element.substr(0,element.find_first_of("="));
					    val = element.substr(element.find_first_of("=")+1);
					    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_KV: element = " << element << " :: key = " << key << " :: val = " << val << endl; }
					    if (key.compare("RMANPROBLEMSL3D") == 0) {
						temp[4] = val.substr(0,val.find_first_of(";"));
					    } else if (key.compare("BLOCKKORRUPTION") == 0) {
						temp[5] = val.substr(0,val.find_first_of(";"));
					    } else {
						break;
					    }
					}
				    } else if (mode.compare("RESI") == 0) {
					temp[6]="00";
					if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_START: mode = " << mode << endl; }
					for (int j=0;j<100;j++) {
					    string key; string val; string element = st.ToString(array[j]);
					    key = element.substr(0,element.find_first_of("="));
					    val = element.substr(element.find_first_of("=")+1);
					    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_KV: element = " << element << " :: key = " << key << " :: val = " << val << endl; }
					    if (key.compare("LASTREDOSWITCH") == 0) {
						temp[6] = val.substr(0,val.find_first_of(";"));
					    } else {
						break;
					    }
					}
				    } else if (mode.compare("PROU") == 0) {
					temp[7]="-";temp[8]="-";
					if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_START: mode = " << mode << endl; }
					for (int j=0;j<100;j++) {
					    string key; string val; string element = st.ToString(array[j]);
					    key = element.substr(0,element.find_first_of("="));
					    val = element.substr(element.find_first_of("=")+1);
					    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_KV: element = " << element << " :: key = " << key << " :: val = " << val << endl; }
					    if (key.compare("PROCESS") == 0) {
						temp[7] = val.substr(0,val.find_first_of(";"));
					    } else if (key.compare("SESSION") == 0) {
						temp[8] = val.substr(0,val.find_first_of(";"));
					    } else {
						break;
					    }
					}
				    } else if (mode.compare("LOBJ") == 0) {
					temp[9]="00";
					if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_START: mode = " << mode << endl; }
					for (int j=0;j<100;j++) {
					    string key; string val; string element = st.ToString(array[j]);
					    key = element.substr(0,element.find_first_of("="));
					    val = element.substr(element.find_first_of("=")+1);
					    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_KV: element = " << element << " :: key = " << key << " :: val = " << val << endl; }
					    if (key.compare("LOCKEDSESSIONS") == 0) {
						temp[9] = val.substr(0,val.find_first_of(";"));
					    } else {
						break;
					    }
					}
				    } else if (mode.compare("IOST") == 0) {
					temp[10]="00";temp[11]="00";temp[12]="00";temp[13]="00";temp[14]="00";temp[15]="00";temp[16]="00";
					if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_START: mode = " << mode << endl; }
					for (int j=0;j<100;j++) {
					    string key; string val; string element = st.ToString(array[j]);
					    key = element.substr(0,element.find_first_of("="));
					    val = element.substr(element.find_first_of("=")+1);
					    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_KV: element = " << element << " :: key = " << key << " :: val = " << val << endl; }
					    if (key.compare("PRTBPS") == 0) {
						temp[10] = val.substr(0,val.find_first_of(";"));
					    } else if (key.compare("OCC") == 0) {
						temp[11] = val.substr(0,val.find_first_of(";"));
					    } else if (key.compare("DBG") == 0) {
						temp[12] = val.substr(0,val.find_first_of(";"));
					    } else if (key.compare("PRTIOR") == 0) {
						temp[13] = val.substr(0,val.find_first_of(";"));
					    } else if (key.compare("DBC") == 0) {
						temp[14] = val.substr(0,val.find_first_of(";"));
					    } else if (key.compare("PWTBPS") == 0) {
						temp[15] = val.substr(0,val.find_first_of(";"));
					    } else if (key.compare("PWTIOR") == 0) {
						temp[16] = val.substr(0,val.find_first_of(";"));
					    } else {
						break;
					    }
					}
				    } else if (mode.compare("IOST") == 0) {
					temp[17]="00";temp[18]="00";temp[19]="00";temp[20]="00";
					if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_START: mode = " << mode << endl; }
					for (int j=0;j<100;j++) {
					    string key; string val; string element = st.ToString(array[j]);
					    key = element.substr(0,element.find_first_of("="));
					    val = element.substr(element.find_first_of("=")+1);
					    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_KV: element = " << element << " :: key = " << key << " :: val = " << val << endl; }
					    if (key.compare("INVALID_OBJECTS") == 0) {
						temp[17] = val.substr(0,val.find_first_of(";"));
						if (!temp[17].size() > 0) { temp[20] = "0"; }
					    } else if (key.compare("INVALID_PART") == 0) {
						temp[18] = val.substr(0,val.find_first_of(";"));
						if (!temp[18].size() > 0) { temp[20] = "0"; }
					    } else if (key.compare("INVALID_INDIZES") == 0) {
						temp[19] = val.substr(0,val.find_first_of(";"));
						if (!temp[19].size() > 0) { temp[20] = "0"; }
					    } else if (key.compare("INVALID_REGISTRY_COMPONENTS") == 0) {
						temp[20] = val.substr(0,val.find_first_of(";"));
						if (!temp[20].size() > 0) { temp[20] = "0"; }
					    } else {
						break;
					    }
					}
				    } else if (mode.compare("DFST") == 0) {
					temp[21]="00";temp[22]="00";temp[23]="00";
					if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_START: mode = " << mode << endl; }
					for (int j=0;j<100;j++) {
					    string key; string val; string element = st.ToString(array[j]);
					    key = element.substr(0,element.find_first_of("="));
					    val = element.substr(element.find_first_of("=")+1);
					    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_KV: element = " << element << " :: key = " << key << " :: val = " << val << endl; }
					    if (key.compare("DATAFILES") == 0) {
						temp[21] = val.substr(0,val.find_first_of(";"));
						if (!temp[21].size() > 0) { temp[21] = "0"; }
					    } else if (key.compare("CONTROLFILES") == 0) {
						temp[22] = val.substr(0,val.find_first_of(";"));
						if (!temp[22].size() > 0) { temp[22] = "0"; }
					    } else if (key.compare("REDOLOGFILES") == 0) {
						temp[23] = val.substr(0,val.find_first_of(";"));
						if (!temp[23].size() > 0) { temp[23] = "0"; }
					    } else {
						break;
					    }
					}
				    } else if (mode.compare("CPUD") == 0) {
					temp[24]="0";temp[25]="0";temp[26]="0";temp[27]="0";temp[28]="0";temp[29]="0";temp[30]="0";
					if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_START: mode = " << mode << endl; }
					for (int j=0;j<100;j++) {
					    string key; string val; string element = st.ToString(array[j]);
					    key = element.substr(0,element.find_first_of("="));
					    val = element.substr(element.find_first_of("=")+1);
					    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_KV: element = " << element << " :: key = " << key << " :: val = " << val << endl; }
					    if (key.compare("CPU_ORA") == 0) {
						temp[24] = val.substr(0,val.find_first_of(";"));
					    } else if (key.compare("WAIT") == 0) {
						temp[25] = val.substr(0,val.find_first_of(";"));
					    } else if (key.compare("COMMIT") == 0) {
						temp[26] = val.substr(0,val.find_first_of(";"));
					    } else if (key.compare("CPU_ORA_WAIT") == 0) {
						temp[27] = val.substr(0,val.find_first_of(";"));
					    } else if (key.compare("CPU_TOTAL") == 0) {
						temp[28] = val.substr(0,val.find_first_of(";"));
					    } else if (key.compare("READIO") == 0) {
						temp[29] = val.substr(0,val.find_first_of(";"));
					    } else if (key.compare("CPU_OS") == 0) {
						temp[30] = val.substr(0,val.find_first_of(";"));
					    } else {
						break;
					    }
					}
				    } else if (mode.compare("ALOW") == 0) {
					temp[31]="00";temp[32]="00";temp[33]="00";
					if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_START: mode = " << mode << endl; }
					for (int j=0;j<100;j++) {
					    string key; string val; string element = st.ToString(array[j]);
					    key = element.substr(0,element.find_first_of("="));
					    val = element.substr(element.find_first_of("=")+1);
					    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_KV: element = " << element << " :: key = " << key << " :: val = " << val << endl; }
					    if (key.compare("oraalerts_warnings") == 0) {
						temp[31] = val.substr(0,val.find_first_of(";"));
					    } else if (key.compare("oraalerts_criticals") == 0) {
						temp[32] = val.substr(0,val.find_first_of(";"));
					    } else if (key.compare("oraalerts_unknowns") == 0) {
						temp[33] = val.substr(0,val.find_first_of(";"));
					    } else {
						break;
					    }
					}
				    } else if (mode.compare("LOUS") == 0) {
					temp[34]="00";
					if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_START: mode = " << mode << endl; }
					for (int j=0;j<100;j++) {
					    string key; string val; string element = st.ToString(array[j]);
					    key = element.substr(0,element.find_first_of("="));
					    val = element.substr(element.find_first_of("=")+1);
					    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_KV: element = " << element << " :: key = " << key << " :: val = " << val << endl; }
					    if (key.compare("USER") == 0) {
						temp[34] = val.substr(0,val.find_first_of(";"));
					    } else {
						break;
					    }
					}
				    } else if (mode.compare("TSFR") == 0) {
					temp[35]="00";
					/* Insert Database */
					if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_START: mode = " << mode << endl; }
					for (int j=0;j<100;j++) {
					    string key; string val; string element = st.ToString(array[j]);
					    key = element.substr(0,element.find_first_of("="));
					    val = element.substr(element.find_first_of("=")+1);
					    temp[35] = val.substr(0,val.find_first_of(";"));
					    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_KV: element = " << element << " :: key = " << key << " :: val = " << val << endl; }
					    if (key.find(".exe") == string::npos) {
						/* select if entry for tablespace exist */
						PreparedStatement_T odbstsfr = Connection_prepareStatement(con, "SELECT dbtsid FROM monitoring_oracle_database_tablespace WHERE dbid=? AND tsna=? AND created>?");
						PreparedStatement_setInt(odbstsfr, 1, dbid);
						PreparedStatement_setString(odbstsfr, 2, key.c_str());
						PreparedStatement_setInt(odbstsfr, 3, timestamp-5);
						ResultSet_T instanceODBSTSFR = PreparedStatement_executeQuery(odbstsfr);
						if (ResultSet_next(instanceODBSTSFR)) {
						    int dbtsid = ResultSet_getIntByName(instanceODBSTSFR, "dbtsid");
						    PreparedStatement_T odbutsfr = Connection_prepareStatement(con, "UPDATE monitoring_oracle_database_tablespace SET TSFRAG=? WHERE DBTSID=?");
						    PreparedStatement_setString(odbutsfr, 1, temp[35].c_str());
						    PreparedStatement_setInt(odbutsfr, 2, dbtsid);
						    PreparedStatement_execute(odbutsfr);
						} else {
						    PreparedStatement_T odbitsfr = Connection_prepareStatement(con, "INSERT INTO monitoring_oracle_database_tablespace(DBID,TSNA,TSUSAGE,TSSIZE,TSMAXSIZE,TSFRAG,CREATED) VALUES (?,?,?,?,?,?,?)");
						    PreparedStatement_setInt(odbitsfr, 1, dbid);
						    PreparedStatement_setString(odbitsfr, 2, key.c_str());
						    PreparedStatement_setString(odbitsfr, 3, "0");
						    PreparedStatement_setString(odbitsfr, 4, "0");
						    PreparedStatement_setString(odbitsfr, 5, "0");
						    PreparedStatement_setString(odbitsfr, 6, temp[35].c_str());
						    PreparedStatement_setInt(odbitsfr, 7, timestamp);
						    PreparedStatement_execute(odbitsfr);
						}
					    }
					    if(array[j+1] == NULL) { break; }
					}
				    } else if (mode.compare("TBST") == 0) {
					temp[36]="00";temp[37]="00";temp[38]="00";temp[39]="00";temp[40]="00";temp[41]="00";temp[42]="00";
					/* Insert Database */
					if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_START: mode = " << mode << endl; }
					for (int j=0;j<100;j++) {
					    string key; string val; string element = st.ToString(array[j]);
					    key = element.substr(0,element.find_first_of("="));
					    val = element.substr(element.find_first_of("=")+1);
					    /* TS Status */
					    temp[36] = val.substr(0,val.find_first_of(";"));
					    temp[37] = val.substr(val.find_first_of(";")+1);
					    /* TS SIZE */
					    temp[38] = temp[37].substr(0,temp[37].find_first_of(";"));
					    temp[39] = temp[37].substr(temp[37].find_first_of(";")+1);
					    /* TS USAGE */
					    temp[40] = temp[39].substr(0,temp[39].find_first_of(";"));
					    temp[41] = temp[39].substr(temp[39].find_first_of(";")+1);
					    /* TS MAXSIZE */
					    temp[42] = temp[41].substr(0,temp[41].find_first_of(";"));
					    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_KV: element = " << element << " :: key = " << key << " :: val = " << val << endl; }
					    if (key.find(".exe") == string::npos) {
						/* select if entry for tablespace exist */
						PreparedStatement_T odbstsfr = Connection_prepareStatement(con, "SELECT dbtsid FROM monitoring_oracle_database_tablespace WHERE dbid=? AND tsna=? AND created>?");
						PreparedStatement_setInt(odbstsfr, 1, dbid);
						PreparedStatement_setString(odbstsfr, 2, key.c_str());
						PreparedStatement_setInt(odbstsfr, 3, timestamp-5);
						ResultSet_T instanceODBSTSFR = PreparedStatement_executeQuery(odbstsfr);
						if (ResultSet_next(instanceODBSTSFR)) {
						    int dbtsid = ResultSet_getIntByName(instanceODBSTSFR, "dbtsid");
						    PreparedStatement_T odbutsfr = Connection_prepareStatement(con, "UPDATE monitoring_oracle_database_tablespace SET TSSTATE=?, TSUSAGE=?, TSSIZE=?, TSMAXSIZE=? WHERE DBTSID=?");
						    PreparedStatement_setString(odbutsfr, 1, temp[36].c_str());
						    PreparedStatement_setString(odbutsfr, 2, temp[40].c_str());
						    PreparedStatement_setString(odbutsfr, 3, temp[38].c_str());
						    PreparedStatement_setString(odbutsfr, 4, temp[42].c_str());
						    PreparedStatement_setInt(odbutsfr, 5, dbtsid);
						    PreparedStatement_execute(odbutsfr);
						} else {
						    /* select if entry for tablespace exist */
						    PreparedStatement_T odbitsfr = Connection_prepareStatement(con, "INSERT INTO monitoring_oracle_database_tablespace(DBID,TSNA,TSSTATE,TSUSAGE,TSSIZE,TSMAXSIZE,TSFRAG,CREATED) VALUES (?,?,?,?,?,?,?,?)");
						    PreparedStatement_setInt(odbitsfr, 1, dbid);
						    PreparedStatement_setString(odbitsfr, 2, key.c_str());
						    PreparedStatement_setString(odbitsfr, 3, temp[36].c_str());
						    PreparedStatement_setString(odbitsfr, 4, temp[40].c_str());
						    PreparedStatement_setString(odbitsfr, 5, temp[38].c_str());
						    PreparedStatement_setString(odbitsfr, 6, temp[42].c_str());
						    PreparedStatement_setString(odbitsfr, 7, "0");
						    PreparedStatement_setInt(odbitsfr, 8, timestamp);
						    PreparedStatement_execute(odbitsfr);
						}
					    }
					    if(array[j+1] == NULL) { break; }
					}
				    } else if (mode.compare("FRAU") == 0) {
					temp[43]="0";temp[44]="0";temp[45]="0";temp[46]="0"; int ccc=0; temp[47]="0"; temp[48]="0";
					if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_START: mode = " << mode << endl; }
					for (int j=0;j<100;j++) {
					    string key; string val; string element = st.ToString(array[j]);
					    key = element.substr(0,element.find_first_of("="));
					    val = element.substr(element.find_first_of("=")+1);
					    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_KV: element = " << element << " :: key = " << key << " :: val = " << val << endl; }
					    if (key.compare("SIZE") == 0) {
						temp[43] = val.substr(0,val.find_first_of(";"));
						ccc++;
					    } else if (key.compare("USAGE") == 0) {
						temp[44] = val.substr(0,val.find_first_of(";"));
						ccc++;
					    } else if (key.compare("REC") == 0) {
						temp[45] = val.substr(0,val.find_first_of(";"));
						ccc++;
					    } else if (key.compare("FILES") == 0) {
						temp[46] = val.substr(0,val.find_first_of(";"));
						ccc++;
					    } else {
						break;
					    }
					    if(array[j+1] == NULL) { break; }
					}
					temp[47] = ec[2];
					temp[48] = temp[47].substr(temp[47].find_first_of("'")+1);
					string franame = temp[48].substr(0,temp[48].find_first_of("'"));
					if (ccc == 4) {
					    PreparedStatement_T odbifrau = Connection_prepareStatement(con, "INSERT INTO monitoring_oracle_database_fra(DBID,PATH,SIZE,USAGE,REC,FILES,CREATED) VALUES (?,encode(?,'base64'),?,?,?,?,?)");
					    PreparedStatement_setInt(odbifrau, 1, dbid);
					    PreparedStatement_setString(odbifrau, 2, franame.c_str());
					    PreparedStatement_setString(odbifrau, 3, temp[43].c_str());
					    PreparedStatement_setString(odbifrau, 4, temp[44].c_str());
					    PreparedStatement_setString(odbifrau, 5, temp[45].c_str());
					    PreparedStatement_setString(odbifrau, 6, temp[46].c_str());
					    PreparedStatement_setInt(odbifrau, 7, timestamp);
					    PreparedStatement_execute(odbifrau);
					}
				    } else {
					if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_START: mode = " << mode << endl; }
				    }
				/* Output */
				if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_UPDATE_PRE: done" << endl; }
				if (cc == 15) {
				    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_UPDATE_IN: in loop done" << endl; }
				    /* Update Database Status */
				    PreparedStatement_T odbu = Connection_prepareStatement(con, "UPDATE monitoring_oracle_database_status SET STARTMODE=?, LOGINS=?, STATUS=?, BLOCKED=?, RMANPROBLEMS=?, BLOCKKORRUPTION=?, REDOLOGSSWITCH=?, PROCESS=?, SESSION=?, LSESSIONS=?, PRTBPS=?, OCC=?, DBG=?, PRTIOR=?, DBC=?, PWTBPS=?, PWTIOR=?, INV_INDIZES=?, INV_PART=?, INV_REG_COMP=?, INV_OBJECTS=?, DATAFILES=?, CONTROLFILES=?, REDOLOGFILES=?, CPU_ORA=?, WAIT=?, COMMIT=?, CPU_ORA_WAIT=?, CPU_TOTAL=?, READIO=?, CPU_OS=?, WAALERTS=?, CRALERTS=?, UNALERTS=?, CLUSERS=?, CREATED=? WHERE dbid=?");
				    PreparedStatement_setString(odbu, 1, temp[0].c_str());
				    PreparedStatement_setString(odbu, 2, temp[1].c_str());
				    PreparedStatement_setString(odbu, 3, temp[2].c_str());
				    PreparedStatement_setString(odbu, 4, temp[3].c_str());
				    PreparedStatement_setInt(odbu, 5, atoi(temp[4].c_str()));
				    PreparedStatement_setInt(odbu, 6, atoi(temp[5].c_str()));
				    PreparedStatement_setInt(odbu, 7, atoi(temp[6].c_str()));
				    PreparedStatement_setString(odbu, 8, temp[7].c_str());
				    PreparedStatement_setString(odbu, 9, temp[8].c_str());
				    PreparedStatement_setInt(odbu, 10, atoi(temp[9].c_str()));
				    PreparedStatement_setInt(odbu, 11, atoi(temp[10].c_str()));
				    PreparedStatement_setInt(odbu, 12, atoi(temp[11].c_str()));
				    PreparedStatement_setInt(odbu, 13, atoi(temp[12].c_str()));
				    PreparedStatement_setInt(odbu, 14, atoi(temp[13].c_str()));
				    PreparedStatement_setInt(odbu, 15, atoi(temp[14].c_str()));
				    PreparedStatement_setInt(odbu, 16, atoi(temp[15].c_str()));
				    PreparedStatement_setInt(odbu, 17, atoi(temp[16].c_str()));
				    PreparedStatement_setInt(odbu, 18, atoi(temp[17].c_str()));
				    PreparedStatement_setInt(odbu, 19, atoi(temp[18].c_str()));
				    PreparedStatement_setInt(odbu, 20, atoi(temp[19].c_str()));
				    PreparedStatement_setInt(odbu, 21, atoi(temp[20].c_str()));
				    PreparedStatement_setInt(odbu, 22, atoi(temp[21].c_str()));
				    PreparedStatement_setInt(odbu, 23, atoi(temp[22].c_str()));
				    PreparedStatement_setInt(odbu, 24, atoi(temp[23].c_str()));
				    PreparedStatement_setString(odbu, 25, temp[24].c_str());
				    PreparedStatement_setString(odbu, 26, temp[25].c_str());
				    PreparedStatement_setString(odbu, 27, temp[26].c_str());
				    PreparedStatement_setString(odbu, 28, temp[27].c_str());
				    PreparedStatement_setString(odbu, 29, temp[28].c_str());
				    PreparedStatement_setString(odbu, 30, temp[29].c_str());
				    PreparedStatement_setString(odbu, 31, temp[30].c_str());
				    PreparedStatement_setInt(odbu, 32, atoi(temp[31].c_str()));
				    PreparedStatement_setInt(odbu, 33, atoi(temp[32].c_str()));
				    PreparedStatement_setInt(odbu, 34, atoi(temp[33].c_str()));
				    PreparedStatement_setInt(odbu, 35, atoi(temp[34].c_str()));
				    PreparedStatement_setInt(odbu, 36, timestamp);
				    PreparedStatement_setInt(odbu, 37, dbid);
				    PreparedStatement_execute(odbu);
				    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_UPDATE_IN: insert done" << endl; }
				    /* if no update executed */
				    if (PreparedStatement_rowsChanged(odbu) == 0) {
					PreparedStatement_T odbui = Connection_prepareStatement(con, "INSERT INTO monitoring_oracle_database_status(DBID,STARTMODE,LOGINS,STATUS,BLOCKED,RMANPROBLEMS,BLOCKKORRUPTION,REDOLOGSSWITCH,PROCESS,SESSION,LSESSIONS,PRTBPS,OCC,DBG,PRTIOR,DBC,PWTBPS,PWTIOR,INV_INDIZES,INV_PART,INV_REG_COMP,INV_OBJECTS,DATAFILES,CONTROLFILES,REDOLOGFILES,CPU_ORA,WAIT,COMMIT,CPU_ORA_WAIT,CPU_TOTAL,READIO,CPU_OS,WAALERTS,CRALERTS,UNALERTS,CLUSERS,CREATED) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
					PreparedStatement_setInt(odbui, 1, dbid);
					PreparedStatement_setString(odbui, 2, temp[0].c_str());
					PreparedStatement_setString(odbui, 3, temp[1].c_str());
					PreparedStatement_setString(odbui, 4, temp[2].c_str());
					PreparedStatement_setString(odbui, 5, temp[3].c_str());
					PreparedStatement_setInt(odbui, 6, atoi(temp[4].c_str()));
					PreparedStatement_setInt(odbui, 7, atoi(temp[5].c_str()));
					PreparedStatement_setInt(odbui, 8, atoi(temp[6].c_str()));
					PreparedStatement_setString(odbui, 9, temp[7].c_str());
					PreparedStatement_setString(odbui, 10, temp[8].c_str());
					PreparedStatement_setInt(odbui, 11, atoi(temp[9].c_str()));
					PreparedStatement_setInt(odbui, 12, atoi(temp[10].c_str()));
					PreparedStatement_setInt(odbui, 13, atoi(temp[11].c_str()));
					PreparedStatement_setInt(odbui, 14, atoi(temp[12].c_str()));
					PreparedStatement_setInt(odbui, 15, atoi(temp[13].c_str()));
					PreparedStatement_setInt(odbui, 16, atoi(temp[14].c_str()));
					PreparedStatement_setInt(odbui, 17, atoi(temp[15].c_str()));
					PreparedStatement_setInt(odbui, 18, atoi(temp[16].c_str()));
					PreparedStatement_setInt(odbui, 19, atoi(temp[17].c_str()));
					PreparedStatement_setInt(odbui, 20, atoi(temp[18].c_str()));
					PreparedStatement_setInt(odbui, 21, atoi(temp[19].c_str()));
					PreparedStatement_setInt(odbui, 22, atoi(temp[20].c_str()));
					PreparedStatement_setInt(odbui, 23, atoi(temp[21].c_str()));
					PreparedStatement_setInt(odbui, 24, atoi(temp[22].c_str()));
					PreparedStatement_setInt(odbui, 25, atoi(temp[23].c_str()));
					PreparedStatement_setString(odbui, 26, temp[24].c_str());
					PreparedStatement_setString(odbui, 27, temp[25].c_str());
					PreparedStatement_setString(odbui, 28, temp[26].c_str());
					PreparedStatement_setString(odbui, 29, temp[27].c_str());
					PreparedStatement_setString(odbui, 30, temp[28].c_str());
					PreparedStatement_setString(odbui, 31, temp[29].c_str());
					PreparedStatement_setString(odbui, 32, temp[30].c_str());
					PreparedStatement_setInt(odbui, 33, atoi(temp[31].c_str()));
					PreparedStatement_setInt(odbui, 34, atoi(temp[32].c_str()));
					PreparedStatement_setInt(odbui, 35, atoi(temp[33].c_str()));
					PreparedStatement_setInt(odbui, 36, atoi(temp[34].c_str()));
					PreparedStatement_setInt(odbui, 37, timestamp);
					PreparedStatement_execute(odbui);
					if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_UPDATE_IN: insert dummy done" << endl; }
				    }
				    /* fill history table */
				    PreparedStatement_T odbuih = Connection_prepareStatement(con, "INSERT INTO monitoring_oracle_database_status_history(DBID,STARTMODE,LOGINS,STATUS,BLOCKED,RMANPROBLEMS,BLOCKKORRUPTION,REDOLOGSSWITCH,PROCESS,SESSION,LSESSIONS,PRTBPS,OCC,DBG,PRTIOR,DBC,PWTBPS,PWTIOR,INV_INDIZES,INV_PART,INV_REG_COMP,INV_OBJECTS,DATAFILES,CONTROLFILES,REDOLOGFILES,CPU_ORA,WAIT,COMMIT,CPU_ORA_WAIT,CPU_TOTAL,READIO,CPU_OS,WAALERTS,CRALERTS,UNALERTS,CLUSERS,CREATED) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
				    PreparedStatement_setInt(odbuih, 1, dbid);
				    PreparedStatement_setString(odbuih, 2, temp[0].c_str());
				    PreparedStatement_setString(odbuih, 3, temp[1].c_str());
				    PreparedStatement_setString(odbuih, 4, temp[2].c_str());
				    PreparedStatement_setString(odbuih, 5, temp[3].c_str());
				    PreparedStatement_setInt(odbuih, 6, atoi(temp[4].c_str()));
				    PreparedStatement_setInt(odbuih, 7, atoi(temp[5].c_str()));
				    PreparedStatement_setInt(odbuih, 8, atoi(temp[6].c_str()));
				    PreparedStatement_setString(odbuih, 9, temp[7].c_str());
				    PreparedStatement_setString(odbuih, 10, temp[8].c_str());
				    PreparedStatement_setInt(odbuih, 11, atoi(temp[9].c_str()));
				    PreparedStatement_setInt(odbuih, 12, atoi(temp[10].c_str()));
				    PreparedStatement_setInt(odbuih, 13, atoi(temp[11].c_str()));
				    PreparedStatement_setInt(odbuih, 14, atoi(temp[12].c_str()));
				    PreparedStatement_setInt(odbuih, 15, atoi(temp[13].c_str()));
				    PreparedStatement_setInt(odbuih, 16, atoi(temp[14].c_str()));
				    PreparedStatement_setInt(odbuih, 17, atoi(temp[15].c_str()));
				    PreparedStatement_setInt(odbuih, 18, atoi(temp[16].c_str()));
				    PreparedStatement_setInt(odbuih, 19, atoi(temp[17].c_str()));
				    PreparedStatement_setInt(odbuih, 20, atoi(temp[18].c_str()));
				    PreparedStatement_setInt(odbuih, 21, atoi(temp[19].c_str()));
				    PreparedStatement_setInt(odbuih, 22, atoi(temp[20].c_str()));
				    PreparedStatement_setInt(odbuih, 23, atoi(temp[21].c_str()));
				    PreparedStatement_setInt(odbuih, 24, atoi(temp[22].c_str()));
				    PreparedStatement_setInt(odbuih, 25, atoi(temp[23].c_str()));
				    PreparedStatement_setString(odbuih, 26, temp[24].c_str());
				    PreparedStatement_setString(odbuih, 27, temp[25].c_str());
				    PreparedStatement_setString(odbuih, 28, temp[26].c_str());
				    PreparedStatement_setString(odbuih, 29, temp[27].c_str());
				    PreparedStatement_setString(odbuih, 30, temp[28].c_str());
				    PreparedStatement_setString(odbuih, 31, temp[29].c_str());
				    PreparedStatement_setString(odbuih, 32, temp[30].c_str());
				    PreparedStatement_setInt(odbuih, 33, atoi(temp[31].c_str()));
				    PreparedStatement_setInt(odbuih, 34, atoi(temp[32].c_str()));
				    PreparedStatement_setInt(odbuih, 35, atoi(temp[33].c_str()));
				    PreparedStatement_setInt(odbuih, 36, atoi(temp[34].c_str()));
				    PreparedStatement_setInt(odbuih, 37, timestamp);
				    PreparedStatement_execute(odbuih);
				    /* output */
				    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_UPDATE: done" << endl; }
				}
				/* end */
				if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_DATABASE_END: mode = " << mode << endl; }
			    /* Oracle Weblogic Server */
			} else if (type.compare("WLS") == 0) {
			    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_MIDDLEWARE: type = " << type << endl; }
			    int mwid = 0;
			    string sPort = "0"; string sType = "0";
			    sPort = name.substr(name.find_first_of(":")+1);
			    sType = name.substr(0, name.find_first_of(":"));
			    /* Get Middleware */
			    PreparedStatement_T wls = Connection_prepareStatement(con, "SELECT mwid FROM monitoring_oracle_middleware_info WHERE hstid=? AND port=? AND type=?");
			    PreparedStatement_setInt(wls, 1, hstid);
			    PreparedStatement_setString(wls, 2, sPort.c_str());
			    PreparedStatement_setString(wls, 3, sType.c_str());
			    ResultSet_T instanceWLS = PreparedStatement_executeQuery(wls);
			    if (ResultSet_next(instanceWLS)) {
				mwid = ResultSet_getIntByName(instanceWLS, "mwid");
			    } else {
				/* Insert Database */
				PreparedStatement_T wlsi = Connection_prepareStatement(con, "INSERT INTO monitoring_oracle_middleware_info(HSTID,TYPE,PORT,CHECK_PERIOD,CREATED) VALUES (?,?,?,?,?)");
				PreparedStatement_setInt(wlsi, 1, hstid);
				PreparedStatement_setString(wlsi, 2, sType.c_str());
				PreparedStatement_setString(wlsi, 3, sPort.c_str());
				PreparedStatement_setString(wlsi, 4, es[5].c_str());
				PreparedStatement_setInt(wlsi, 5, timestamp);
				PreparedStatement_execute(wlsi);
				/* Select MWID */
				PreparedStatement_T wls2 = Connection_prepareStatement(con, "SELECT mwid FROM monitoring_oracle_middleware_info WHERE hstid=? AND port=? AND type=?");
				PreparedStatement_setInt(wls2, 1, hstid);
				PreparedStatement_setString(wls2, 2, sPort.c_str());
				PreparedStatement_setString(wls2, 3, sType.c_str());
				ResultSet_T instanceWLS2 = PreparedStatement_executeQuery(wls2);
				if (ResultSet_next(instanceWLS2)) {
				    mwid = ResultSet_getIntByName(instanceWLS2, "mwid");
				}
			    }
			    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_MIDDLEWARE: mwid = " << mwid << " :: perf_data = " << ec[4] << endl; }
			    /* split perfdata into elements by ' ' */
			    Configuration st; i=0; array[i] = strtok(ec[4], " "); while(array[i]!=NULL) { array[++i] = strtok(NULL," "); }
			    if (mode.compare("APP") == 0) {
				if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_MIDDLEWARE_START: mode = " << mode << endl; }
				for (int j=0;j<100;j++) {
				    string key; string val; string element = st.ToString(array[j]); string hMode; string hName; string entry;
				    key = element.substr(0,element.find_first_of("="));
				    val = element.substr(element.find_first_of("=")+1);
				    entry = val.substr(0,val.find_first_of(";"));
				    hMode = key.substr(0,key.find_first_of("_"));
				    hName = key.substr(key.find_first_of("_")+1);
				    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_MIDDLEWARE_KV: element = " << element << " :: key = " << key << " :: val = " << val << endl; }
				    /* Check if status entry exist */
				    if (key.find(".exe") == string::npos) {
					PreparedStatement_T wls3 = Connection_prepareStatement(con, "SELECT mwsid FROM monitoring_oracle_middleware_status WHERE mwid=? AND type=? AND mode=? AND name=?");
					PreparedStatement_setInt(wls3, 1, mwid);
					PreparedStatement_setString(wls3, 2, "APP");
					PreparedStatement_setString(wls3, 3, hMode.c_str());
					PreparedStatement_setString(wls3, 4, hName.c_str());
					ResultSet_T instanceWLS3 = PreparedStatement_executeQuery(wls3);
					if (ResultSet_next(instanceWLS3)) {
					    PreparedStatement_T wlsuapp = Connection_prepareStatement(con, "UPDATE monitoring_oracle_middleware_status SET status=?, created=? WHERE mwid=? AND type=? AND mode=? AND name=?");
					    PreparedStatement_setString(wlsuapp, 1, entry.c_str());
					    PreparedStatement_setInt(wlsuapp, 2, timestamp);
					    PreparedStatement_setInt(wlsuapp, 3, mwid);
					    PreparedStatement_setString(wlsuapp, 4, "APP");
					    PreparedStatement_setString(wlsuapp, 5, hMode.c_str());
					    PreparedStatement_setString(wlsuapp, 6, hName.c_str());
					    PreparedStatement_execute(wlsuapp);
					} else {
					    PreparedStatement_T wlsiapp = Connection_prepareStatement(con, "INSERT INTO monitoring_oracle_middleware_status(MWID,TYPE,MODE,NAME,STATUS,CREATED) VALUES (?,?,?,?,?,?)");
					    PreparedStatement_setInt(wlsiapp, 1, mwid);
					    PreparedStatement_setString(wlsiapp, 2, "APP");
					    PreparedStatement_setString(wlsiapp, 3, hMode.c_str());
					    PreparedStatement_setString(wlsiapp, 4, hName.c_str());
					    PreparedStatement_setString(wlsiapp, 5, entry.c_str());
					    PreparedStatement_setInt(wlsiapp, 6, timestamp);
					    PreparedStatement_execute(wlsiapp);
					}
					PreparedStatement_T wlsi2app = Connection_prepareStatement(con, "INSERT INTO monitoring_oracle_middleware_status_history(MWID,TYPE,MODE,NAME,STATUS,CREATED) VALUES (?,?,?,?,?,?)");
					PreparedStatement_setInt(wlsi2app, 1, mwid);
					PreparedStatement_setString(wlsi2app, 2, "APP");
					PreparedStatement_setString(wlsi2app, 3, hMode.c_str());
					PreparedStatement_setString(wlsi2app, 4, hName.c_str());
					PreparedStatement_setString(wlsi2app, 5, entry.c_str());
					PreparedStatement_setInt(wlsi2app, 6, timestamp);
					PreparedStatement_execute(wlsi2app);
				    }
				    if(array[j+1] == NULL) { break; }
				}
			    } else if (mode.compare("SRV") == 0) {
				if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_MIDDLEWARE_START: mode = " << mode << endl; }
				for (int j=0;j<100;j++) {
				    string key; string val; string element = st.ToString(array[j]); string entry; string hMode; string hName;
				    key = element.substr(0,element.find_first_of("="));
				    val = element.substr(element.find_first_of("=")+1);
				    entry = val.substr(0,val.find_first_of(";"));
				    hMode = key.substr(0,key.find_first_of("_"));
				    hName = key.substr(key.find_first_of("_")+1);
				    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_MIDDLEWARE_KV: element = " << element << " :: key = " << key << " :: val = " << val << endl; }
				    /* Check if status entry exist */
				    if (key.find(".exe") == string::npos) {
					PreparedStatement_T wls3 = Connection_prepareStatement(con, "SELECT mwsid FROM monitoring_oracle_middleware_status WHERE mwid=? AND type=? AND mode=? AND name=?");
					PreparedStatement_setInt(wls3, 1, mwid);
					PreparedStatement_setString(wls3, 2, "SRV");
					PreparedStatement_setString(wls3, 3, hMode.c_str());
					PreparedStatement_setString(wls3, 4, hName.c_str());
					ResultSet_T instanceWLS3 = PreparedStatement_executeQuery(wls3);
					if (ResultSet_next(instanceWLS3)) {
					    PreparedStatement_T wlsuapp = Connection_prepareStatement(con, "UPDATE monitoring_oracle_middleware_status SET status=?, created=? WHERE mwid=? AND type=? AND mode=? AND name=?");
					    PreparedStatement_setString(wlsuapp, 1, entry.c_str());
					    PreparedStatement_setInt(wlsuapp, 2, timestamp);
					    PreparedStatement_setInt(wlsuapp, 3, mwid);
					    PreparedStatement_setString(wlsuapp, 4, "SRV");
					    PreparedStatement_setString(wlsuapp, 5, hMode.c_str());
					    PreparedStatement_setString(wlsuapp, 6, hName.c_str());
					    PreparedStatement_execute(wlsuapp);
					} else {
					    PreparedStatement_T wlsiapp = Connection_prepareStatement(con, "INSERT INTO monitoring_oracle_middleware_status(MWID,TYPE,MODE,NAME,STATUS,CREATED) VALUES (?,?,?,?,?,?)");
					    PreparedStatement_setInt(wlsiapp, 1, mwid);
					    PreparedStatement_setString(wlsiapp, 2, "SRV");
					    PreparedStatement_setString(wlsiapp, 3, hMode.c_str());
					    PreparedStatement_setString(wlsiapp, 4, hName.c_str());
					    PreparedStatement_setString(wlsiapp, 5, entry.c_str());
					    PreparedStatement_setInt(wlsiapp, 6, timestamp);
					    PreparedStatement_execute(wlsiapp);
					}
					PreparedStatement_T wlsi2app = Connection_prepareStatement(con, "INSERT INTO monitoring_oracle_middleware_status_history(MWID,TYPE,MODE,NAME,STATUS,CREATED) VALUES (?,?,?,?,?,?)");
					PreparedStatement_setInt(wlsi2app, 1, mwid);
					PreparedStatement_setString(wlsi2app, 2, "SRV");
					PreparedStatement_setString(wlsi2app, 3, hMode.c_str());
					PreparedStatement_setString(wlsi2app, 4, hName.c_str());
					PreparedStatement_setString(wlsi2app, 5, entry.c_str());
					PreparedStatement_setInt(wlsi2app, 6, timestamp);
					PreparedStatement_execute(wlsi2app);
				    }
				    if(array[j+1] == NULL) { break; }
				}
				/* Update SRVID for DB Status Service */
				PreparedStatement_T wlsi21 = Connection_prepareStatement(con, "UPDATE monitoring_oracle_middleware_info SET srvid=? WHERE mwid=?");
				PreparedStatement_setInt(wlsi21, 1, srvid);
				PreparedStatement_setInt(wlsi21, 2, mwid);
				PreparedStatement_execute(wlsi21);
			    } else if (mode.compare("JVM") == 0) {
				if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_MIDDLEWARE_START: mode = " << mode << endl; }
				for (int j=0;j<100;j++) {
				    string key; string val; string element = st.ToString(array[j]); string entry;
				    key = element.substr(0,element.find_first_of("="));
				    val = element.substr(element.find_first_of("=")+1);
				    entry = val.substr(0,val.find_first_of(";"));
				    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_ORACLE_MIDDLEWARE_KV: element = " << element << " :: key = " << key << " :: val = " << val << endl; }
				    /* Check if status entry exist */
				    if (key.find(".exe") == string::npos) {
					PreparedStatement_T wls3 = Connection_prepareStatement(con, "SELECT mwsid FROM monitoring_oracle_middleware_status WHERE mwid=? AND type=? AND mode=? AND name=?");
					PreparedStatement_setInt(wls3, 1, mwid);
					PreparedStatement_setString(wls3, 2, mode.c_str());
					PreparedStatement_setString(wls3, 3, "USAGE");
					PreparedStatement_setString(wls3, 4, key.c_str());
					ResultSet_T instanceWLS3 = PreparedStatement_executeQuery(wls3);
					if (ResultSet_next(instanceWLS3)) {
					    PreparedStatement_T wlsuapp = Connection_prepareStatement(con, "UPDATE monitoring_oracle_middleware_status SET status=?, created=? WHERE mwid=? AND type=? AND mode=? AND name=?");
					    PreparedStatement_setString(wlsuapp, 1, entry.c_str());
					    PreparedStatement_setInt(wlsuapp, 2, timestamp);
					    PreparedStatement_setInt(wlsuapp, 3, mwid);
					    PreparedStatement_setString(wlsuapp, 4, mode.c_str());
					    PreparedStatement_setString(wlsuapp, 5, "USAGE");
					    PreparedStatement_setString(wlsuapp, 6, key.c_str());
					    PreparedStatement_execute(wlsuapp);
					} else {
					    PreparedStatement_T wlsiapp = Connection_prepareStatement(con, "INSERT INTO monitoring_oracle_middleware_status(MWID,TYPE,MODE,NAME,STATUS,CREATED) VALUES (?,?,?,?,?,?)");
					    PreparedStatement_setInt(wlsiapp, 1, mwid);
					    PreparedStatement_setString(wlsiapp, 2, mode.c_str());
					    PreparedStatement_setString(wlsiapp, 3, "USAGE");
					    PreparedStatement_setString(wlsiapp, 4, key.c_str());
					    PreparedStatement_setString(wlsiapp, 5, entry.c_str());
					    PreparedStatement_setInt(wlsiapp, 6, timestamp);
					    PreparedStatement_execute(wlsiapp);
					}
					PreparedStatement_T wlsi2app = Connection_prepareStatement(con, "INSERT INTO monitoring_oracle_middleware_status_history(MWID,TYPE,MODE,NAME,STATUS,CREATED) VALUES (?,?,?,?,?,?)");
					PreparedStatement_setInt(wlsi2app, 1, mwid);
					PreparedStatement_setString(wlsi2app, 2, mode.c_str());
					PreparedStatement_setString(wlsi2app, 3, "USAGE");
					PreparedStatement_setString(wlsi2app, 4, key.c_str());
					PreparedStatement_setString(wlsi2app, 5, entry.c_str());
					PreparedStatement_setInt(wlsi2app, 6, timestamp);
					PreparedStatement_execute(wlsi2app);
				    }
				    if(array[j+1] == NULL) { break; }
				}
			    } else {
				/* Nothing */
			    }
			/* System Linux / Windows */
			} else {
			    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_IF: " << es[1] << endl; }
			}
		    }
		} CATCH(SQLException) {
		    snprintf(temp_buffer, sizeof(temp_buffer) - 1, "id2sc: NEBCALLBACK_SERVICE_STATUS_DATA SQLException - %s :: %s :: %s\n", es[0].c_str(), es[1].c_str(), Exception_frame.message);
		    temp_buffer[sizeof(temp_buffer)-1] = '\x0';
		    write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);
		    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS: NEBCALLBACK_SERVICE_STATUS_DATA SQLException - " << Exception_frame.message << endl; }
		} FINALLY {
		    Connection_close(con);
		    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] SERVICE_STATUS_CONNECTION_CLOSE: done. " << endl; }
		} END_TRY;
		/* end */
	    }
	    break;
	case NEBCALLBACK_HOST_STATUS_DATA:
	    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] EVENT-SWITCH: NEBCALLBACK_SERVICE_STATUS_DATA " << endl; }
	    if ((hsdata = (nebstruct_host_status_data *)data)) {
		type = "tbd"; mode = "tbd"; name = "tbd"; /* tbd = to be defined */ int hstid=0000; int srvid=0000; int timestamp = (int)time(NULL);
		temp_host = (host *)hsdata->object_ptr;
		if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] HOST_STATUS: " << temp_host->name << endl; }
		/* get data from struct */
		es[0] = escape_buffer(temp_host->name);
		es[1] = "SYSTEM_ICMP_REQUEST";
		ec[2] = escape_buffer(temp_host->plugin_output);
		ec[3] = escape_buffer(temp_host->long_plugin_output);
		ec[4] = escape_buffer(temp_host->perf_data);
		es[5] = s.Trimm(s.ToString(temp_host->check_period));
		es[8] = s.Trimm(s.ToString(temp_host->address));
		es[7] = es[1].substr(es[1].find_first_of("_")+1);
		
		if(ec[4] != NULL) {
		    if(strlen(ec[4]) > MAX_TEXT_LEN) {
			ec[4][MAX_TEXT_LEN] = '\0';
		    }
		}
		
		if(ec[3] != NULL) {
		    if(strlen(ec[3]) > MAX_TEXT_LEN) {
			ec[3][MAX_TEXT_LEN] = '\0';
		    }
		}

		if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] EVENT-SWITCH-HOST_STATUS: " << es[1] << endl; }
		/* deformat service description string: <TYPE>_<MODE>_<NAME>*/
		type = es[1].substr(0,es[1].find_first_of("_"));
		mode = es[7].substr(0, es[7].find_first_of("_"));
		name = es[7].substr(es[7].find_first_of("_")+1);

		TRY {
		    /* Get Host ID */
		    PreparedStatement_T shsd = Connection_prepareStatement(con, "SELECT hstid FROM monitoring_info_host WHERE instid=? AND hstln=? AND ipaddr=?");
		    PreparedStatement_setInt(shsd, 1, instid);
		    PreparedStatement_setString(shsd, 2, es[0].c_str());
		    PreparedStatement_setString(shsd, 3, es[8].c_str());
		    ResultSet_T instance1 = PreparedStatement_executeQuery(shsd);
		    if (ResultSet_next(instance1)) {
		        hstid = ResultSet_getIntByName(instance1, "hstid");
		    } else {
		        /* Insert Host Entry */
		        PreparedStatement_T ihsd = Connection_prepareStatement(con, "INSERT INTO monitoring_info_host(HSTLN,IPADDR,HTYPID,DSC,INSTID,CHECK_PERIOD,CREATED) VALUES (?,?,?,?,?,?,?)");
		        PreparedStatement_setString(ihsd, 1, es[0].c_str());
		        PreparedStatement_setString(ihsd, 2, es[8].c_str());
		        PreparedStatement_setInt(ihsd, 3, 1);
		        PreparedStatement_setString(ihsd, 4, "-");
		        PreparedStatement_setInt(ihsd, 5, instid);
		        PreparedStatement_setString(ihsd, 6, es[5].c_str());
		        PreparedStatement_setInt(ihsd, 7, timestamp);
		        PreparedStatement_execute(ihsd);
		        /* Select Host ID */
		        PreparedStatement_T shsd2 = Connection_prepareStatement(con, "SELECT hstid FROM monitoring_info_host WHERE instid=? AND hstln=? AND ipaddr=?");
		        PreparedStatement_setInt(shsd2, 1, instid);
		        PreparedStatement_setString(shsd2, 2, es[0].c_str());
		        PreparedStatement_setString(shsd2, 3, es[8].c_str());
		        ResultSet_T instance12 = PreparedStatement_executeQuery(shsd2);
		        if (ResultSet_next(instance12)) {
			    hstid = ResultSet_getIntByName(instance12, "hstid");
		        }
		    }
		    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] EVENT-SWITCH-SQL: Get Host ID :: " << hstid << endl; }
		    /* Service */
		    PreparedStatement_T shsrvd = Connection_prepareStatement(con, "SELECT srvid FROM monitoring_info_service WHERE instid=? AND hstid=? AND srvna=?");
		    PreparedStatement_setInt(shsrvd, 1, instid);
		    PreparedStatement_setInt(shsrvd, 2, hstid);
		    PreparedStatement_setString(shsrvd, 3, es[1].c_str());
		    ResultSet_T instance2 = PreparedStatement_executeQuery(shsrvd);
		    if (ResultSet_next(instance2)) {
		        srvid = ResultSet_getIntByName(instance2, "srvid");
		    } else {
		        /* Insert Service Entry */
		        PreparedStatement_T ihsrvd = Connection_prepareStatement(con, "INSERT INTO monitoring_info_service(HSTID,SRVNA,DSC,INSTID,CHECK_PERIOD,CREATED) VALUES (?,?,?,?,?,?)");
		        PreparedStatement_setInt(ihsrvd, 1, hstid);
		        PreparedStatement_setString(ihsrvd, 2, es[1].c_str());
		        PreparedStatement_setString(ihsrvd, 3, "-");
		        PreparedStatement_setInt(ihsrvd, 4, instid);
		        PreparedStatement_setString(ihsrvd, 5, es[5].c_str());
		        PreparedStatement_setInt(ihsrvd, 6, timestamp);
		        PreparedStatement_execute(ihsrvd);
		        /* Select Service ID */
		        PreparedStatement_T shsrvd2 = Connection_prepareStatement(con, "SELECT srvid FROM monitoring_info_service WHERE instid=? AND hstid=? AND srvna=?");
		        PreparedStatement_setInt(shsrvd2, 1, instid);
		        PreparedStatement_setInt(shsrvd2, 2, hstid);
		        PreparedStatement_setString(shsrvd2, 3, es[1].c_str());
		        ResultSet_T instance22 = PreparedStatement_executeQuery(shsrvd2);
		        if (ResultSet_next(instance22)) {
			    srvid = ResultSet_getIntByName(instance22, "srvid");
		        }
		    }
		    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] EVENT-SWITCH-SQL: Get Service ID :: " << srvid << endl; }
		    /* Insert or Update Status Table */
		    PreparedStatement_T smise = Connection_prepareStatement(con, "SELECT sid FROM monitoring_status WHERE srvid=?");
		    PreparedStatement_setInt(smise, 1, srvid);
		    ResultSet_T instance3 = PreparedStatement_executeQuery(smise);
		    if (ResultSet_next(instance3)) {
			PreparedStatement_T uhpd = Connection_prepareStatement(con, "UPDATE monitoring_status SET OUTPUT=encode(?,'base64'), LONG_OUTPUT=encode(?,'base64'), CURRENT_STATE=?, LAST_STATE=?, LAST_CHECK=?, NEXT_CHECK=?, LAST_TIME_OK=?, LAST_TIME_WA=?, LAST_TIME_CR=?, LAST_TIME_UN=?, PERCENT_STATE_CHANGE=?, PERF_DATA=encode(?,'base64'), CREATED=? WHERE sid=?");
			PreparedStatement_setString(uhpd, 1, ec[2]);
			PreparedStatement_setString(uhpd, 2, ec[3]);
			PreparedStatement_setInt(uhpd, 3, temp_host->current_state);
			PreparedStatement_setInt(uhpd, 4, temp_host->last_state);
			PreparedStatement_setInt(uhpd, 5, temp_host->last_check);
			PreparedStatement_setInt(uhpd, 6, temp_host->next_check);
			PreparedStatement_setInt(uhpd, 7, temp_host->last_time_up);
			PreparedStatement_setInt(uhpd, 8, 0);
			PreparedStatement_setInt(uhpd, 9, temp_host->last_time_down);
			PreparedStatement_setInt(uhpd, 10, temp_host->last_time_unreachable);
			PreparedStatement_setInt(uhpd, 11, temp_host->percent_state_change);
			PreparedStatement_setString(uhpd, 12, ec[4]);
			PreparedStatement_setInt(uhpd, 13, timestamp);
			PreparedStatement_setInt(uhpd, 14, ResultSet_getIntByName(instance3, "sid"));
			PreparedStatement_execute(uhpd);
			/* Fill Status History table */
			PreparedStatement_T ihhd = Connection_prepareStatement(con, "INSERT INTO monitoring_status_history(HSTID,SRVID,OUTPUT,LONG_OUTPUT,CURRENT_STATE,LAST_STATE,PERF_DATA,CREATED) VALUES (?,?,encode(?,'base64'),encode(?,'base64'),?,?,encode(?,'base64'),?)");
			PreparedStatement_setInt(ihhd, 1, hstid);
			PreparedStatement_setInt(ihhd, 2, srvid);
			PreparedStatement_setString(ihhd, 3, ec[2]);
			PreparedStatement_setString(ihhd, 4, ec[3]);
			PreparedStatement_setInt(ihhd, 5, temp_host->current_state);
			PreparedStatement_setInt(ihhd, 6, temp_host->last_state);
			PreparedStatement_setString(ihhd, 7, ec[4]);
			PreparedStatement_setInt(ihhd, 8, timestamp);
			PreparedStatement_execute(ihhd);
			/* check for state change */
			if ( (temp_host->current_state != temp_host->last_state) && (temp_host->problem_has_been_acknowledged == 0) ) {
			    PreparedStatement_T msc = Connection_prepareStatement(con, "INSERT INTO monitoring_state_change(HSTID,SRVID,STATE,LAST_STATE,OUTPUT,NEW_PROBLEM,MAIL,CREATED) VALUES (?,?,?,?,encode(?,'base64'),?,?,?)");
			    PreparedStatement_setInt(msc, 1, hstid);
			    PreparedStatement_setInt(msc, 2, srvid);
			    PreparedStatement_setInt(msc, 3, temp_host->current_state);
			    PreparedStatement_setInt(msc, 4, temp_host->last_state);
			    PreparedStatement_setString(msc, 5, ec[2]);
			    PreparedStatement_setInt(msc, 6, 1);
			    PreparedStatement_setInt(msc, 7, 0);
			    PreparedStatement_setInt(msc, 8, timestamp);
			    PreparedStatement_execute(msc);
			    /* Remove Acknowledgement */
			    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] EVENT-SWITCH-HOST_STATUS STATE_CHANGE: Remove Acknowledgement cs != ls" << endl; }
			    PreparedStatement_T msc2 = Connection_prepareStatement(con, "UPDATE monitoring_status SET ack=false, ackid='0' WHERE srvid=?");
			    PreparedStatement_setInt(msc2, 1, srvid);
			    PreparedStatement_execute(msc2);
			    /* Prepare for Mailing */
			    int mtyp;
			    switch (temp_host->last_state) {
				case 0:
				    mtyp=3;
				    break;
				case 1:
				    if (temp_host->current_state == 0) { mtyp = 4; } else { mtyp = 3; }
				    break;
				case 2:
				    if (temp_host->current_state == 0) { mtyp = 4; } else { mtyp = 3; }
				    break;
				case 3:
				    if (temp_host->current_state == 0) { mtyp = 4; } else { mtyp = 3; }
				    break;
				default:
				    mtyp=3;
				    break;
			    }
			    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] EVENT-SWITCH-HOST_STATUS STATE_CHANGE: Fill Monitoring Mailing" << endl; }
			    PreparedStatement_T pfm = Connection_prepareStatement(con, "INSERT INTO monitoring_mailing(DONE,HSTID,SRVID,MTYPID,USR,COMMENT,APPID,T1,T2,CREATED) VALUES (false,?,?,?,encode('system','base64'),encode(?,'base64'),'0','0','0',?)");
			    PreparedStatement_setInt(pfm, 1, hstid);
			    PreparedStatement_setInt(pfm, 2, srvid);
			    PreparedStatement_setInt(pfm, 3, mtyp);
			    PreparedStatement_setString(pfm, 4, ec[2]);
			    PreparedStatement_setInt(pfm, 5, timestamp);
			    PreparedStatement_execute(pfm);
			}
		    } else {
			PreparedStatement_T ihpd = Connection_prepareStatement(con, "INSERT INTO monitoring_status(HSTID,SRVID,OUTPUT,LONG_OUTPUT,CURRENT_STATE,LAST_STATE,LAST_CHECK,NEXT_CHECK,LAST_TIME_OK,LAST_TIME_WA,LAST_TIME_CR,LAST_TIME_UN,PERCENT_STATE_CHANGE,PERF_DATA,ACK,ACKID,DTM,DTMID,CREATED) VALUES (?,?,encode(?,'base64'),encode(?,'base64'),?,?,?,?,?,?,?,?,?,encode(?,'base64'),false,'0',false,'0',?)");
			PreparedStatement_setInt(ihpd, 1, hstid);
			PreparedStatement_setInt(ihpd, 2, srvid);
			PreparedStatement_setString(ihpd, 3, ec[2]);
			PreparedStatement_setString(ihpd, 4, ec[3]);
			PreparedStatement_setInt(ihpd, 5, temp_host->current_state);
			PreparedStatement_setInt(ihpd, 6, temp_host->last_state);
			PreparedStatement_setInt(ihpd, 7, temp_host->last_check);
			PreparedStatement_setInt(ihpd, 8, temp_host->next_check);
			PreparedStatement_setInt(ihpd, 9, temp_host->last_time_up);
			PreparedStatement_setInt(ihpd, 10, 0);
			PreparedStatement_setInt(ihpd, 11, temp_host->last_time_down);
			PreparedStatement_setInt(ihpd, 12, temp_host->last_time_unreachable);
			PreparedStatement_setInt(ihpd, 13, temp_host->percent_state_change);
			PreparedStatement_setString(ihpd, 14, ec[4]);
			PreparedStatement_setInt(ihpd, 15, timestamp);
			PreparedStatement_execute(ihpd);
			/* Fill Status History table */
			PreparedStatement_T ihhd = Connection_prepareStatement(con, "INSERT INTO monitoring_status_history(HSTID,SRVID,OUTPUT,LONG_OUTPUT,CURRENT_STATE,LAST_STATE,PERF_DATA,CREATED) VALUES (?,?,encode(?,'base64'),encode(?,'base64'),?,?,encode(?,'base64'),?)");
			PreparedStatement_setInt(ihhd, 1, hstid);
			PreparedStatement_setInt(ihhd, 2, srvid);
			PreparedStatement_setString(ihhd, 3, ec[2]);
			PreparedStatement_setString(ihhd, 4, ec[3]);
			PreparedStatement_setInt(ihhd, 5, temp_host->current_state);
			PreparedStatement_setInt(ihhd, 6, temp_host->last_state);
			PreparedStatement_setString(ihhd, 7, ec[4]);
			PreparedStatement_setInt(ihhd, 8, timestamp);
			PreparedStatement_execute(ihhd);
			/* check for state change */
			if ( (temp_host->current_state > 0) && (temp_host->problem_has_been_acknowledged == 0) ) {
			    PreparedStatement_T msc = Connection_prepareStatement(con, "INSERT INTO monitoring_state_change(HSTID,SRVID,STATE,LAST_STATE,OUTPUT,NEW_PROBLEM,MAIL,CREATED) VALUES (?,?,?,?,encode(?,'base64'),?,?,?)");
			    PreparedStatement_setInt(msc, 1, hstid);
			    PreparedStatement_setInt(msc, 2, srvid);
			    PreparedStatement_setInt(msc, 3, temp_host->current_state);
			    PreparedStatement_setInt(msc, 4, 0);
			    PreparedStatement_setString(msc, 5, ec[2]);
			    PreparedStatement_setInt(msc, 6, 1);
			    PreparedStatement_setInt(msc, 7, 0);
			    PreparedStatement_setInt(msc, 8, timestamp);
			    PreparedStatement_execute(msc);
			    /* Remove Acknowledgement */
			    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] EVENT-SWITCH-HOST_STATUS STATE_CHANGE: Remove Acknowledgement" << endl; }
			    PreparedStatement_T msc2 = Connection_prepareStatement(con, "UPDATE monitoring_status SET ack=false, ackid='0' WHERE srvid=?");
			    PreparedStatement_setInt(msc2, 1, srvid);
			    PreparedStatement_execute(msc2);
			    /* Prepare for Mailing */
			    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] EVENT-SWITCH-HOST_STATUS STATE_CHANGE: Fill Monitoring Mailing" << endl; }
			    PreparedStatement_T pfm = Connection_prepareStatement(con, "INSERT INTO monitoring_mailing(DONE,HSTID,SRVID,MTYPID,USR,COMMENT,APPID,T1,T2,CREATED) VALUES (false,?,?,?,encode('system','base64'),encode(?,'base64'),'0','0','0',?)");
			    PreparedStatement_setInt(pfm, 1, hstid);
			    PreparedStatement_setInt(pfm, 2, srvid);
			    PreparedStatement_setInt(pfm, 3, 4);
			    PreparedStatement_setString(pfm, 4, ec[2]);
			    PreparedStatement_setInt(pfm, 5, timestamp);
			    PreparedStatement_execute(pfm);
			}
		    }
		    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] EVENT-SWITCH-SQL: Update Status Table " << endl; }
		    /* Check if Passiv Check */
		    int next_check=0; int last_check=0;
		    if (temp_host->check_type == 0) {
		        next_check = temp_host->next_check;
		        last_check = temp_host->last_check;
		    } else {
		        PreparedStatement_T slac2 = Connection_prepareStatement(con, "SELECT created FROM monitoring_availability WHERE srvid=? ORDER BY 1 DESC LIMIT 1");
		        PreparedStatement_setInt(slac2, 1, srvid);
		        ResultSet_T instanceS = PreparedStatement_executeQuery(slac2);
		        if (ResultSet_next(instanceS)) {
		            next_check = timestamp;
		            last_check = ResultSet_getIntByName(instanceS, "created");
		        } else {
		            next_check = temp_host->last_check;
		            last_check = temp_host->last_check;
		        }
		    }
		    /* Get Durations */
		    int timeok=0; int timewa=0; int timecr=0; int timeun=0;
		    switch (temp_host->current_state) {
		        case 0:
			    timeok = next_check - last_check;
			    break;
		        case 1:
			    timewa = next_check - last_check;
			    break;
		        case 2:
			    timecr = next_check - last_check;
			    break;
		        case 3:
			    timeun = next_check - last_check;
			    break;
		        default:
			    break;
		    }
		    /* Update Availability Table */
		    PreparedStatement_T smase = Connection_prepareStatement(con, "SELECT aid FROM monitoring_availability WHERE srvid=? AND created=?");
		    PreparedStatement_setInt(smase, 1, srvid);
		    PreparedStatement_setInt(smase, 2, timestamp);
		    ResultSet_T instance4 = PreparedStatement_executeQuery(smase);
		    if (ResultSet_next(instance4)) {
		        /* nothing temp_service->last_check */
		    } else {
		        PreparedStatement_T ihad = Connection_prepareStatement(con, "INSERT INTO monitoring_availability(SRVID,TIMEOK,TIMEWA,TIMECR,TIMEUN,CREATED) VALUES (?,?,?,?,?,?)");
		        PreparedStatement_setInt(ihad, 1, srvid);
		        PreparedStatement_setInt(ihad, 2, timeok);
		        PreparedStatement_setInt(ihad, 3, timewa);
		        PreparedStatement_setInt(ihad, 4, timecr);
		        PreparedStatement_setInt(ihad, 5, timeun);
		        PreparedStatement_setInt(ihad, 6, timestamp);
		        PreparedStatement_execute(ihad);
		    }
		    /* select on type  */
		} CATCH(SQLException) {
		    snprintf(temp_buffer, sizeof(temp_buffer) - 1, "id2sc: NEBCALLBACK_HOST_STATUS_DATA SQLException - %s :: %s :: %s\n", es[0].c_str(), es[1].c_str(), Exception_frame.message);
		    temp_buffer[sizeof(temp_buffer)-1] = '\x0';
		    write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);
		    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] HOST_STATUS: NEBCALLBACK_HOST_STATUS_DATA SQLException - " << Exception_frame.message << endl; }
		} FINALLY {
		    Connection_close(con);
		    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] HOST_STATUS_CONNECTION_CLOSE: done. " << endl; }
		} END_TRY;
	    }
		/* end */
	    break;
	case NEBCALLBACK_PROGRAM_STATUS_DATA:
	    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] EVENT-SWITCH: NEBCALLBACK_PROGRAM_STATUS_DATA " << endl; }
	    if ((psdata = (nebstruct_program_status_data *)data)) {
		if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] PROGRAM_STATUS: " << psdata->pid << endl; }
		int timestamp = (int)time(NULL);
		con = ConnectionPool_getConnection(pool);
		TRY {
		    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] PROGRAM_STATUS: UPDATE INFO_INSTANCE" << endl; }
		    PreparedStatement_T iupd = Connection_prepareStatement(con, "UPDATE monitoring_info_instance SET last_active = ?, startup = ?, pid = ?");
		    PreparedStatement_setInt(iupd, 1, psdata->timestamp.tv_sec);
		    PreparedStatement_setInt(iupd, 2, (unsigned long)psdata->program_start);
		    PreparedStatement_setInt(iupd, 3, psdata->pid);
		    PreparedStatement_execute(iupd);
		    /*  */
		    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] PROGRAM_STATUS: SELECT TASK SELECT" << endl; }
		    PreparedStatement_T st = Connection_prepareStatement(con, "select a.tid,a.type,b.hstln,c.srvna,a.tsstart,a.tsend,a.usr,a.comment,a.hstid,a.srvid from monitoring_task a, monitoring_info_host b, monitoring_info_service c where a.hstid=b.hstid and a.srvid=c.srvid and a.done=false and a.instid=?");
		    PreparedStatement_setInt(st, 1, instid);
		    ResultSet_T result = PreparedStatement_executeQuery(st);
		    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] PROGRAM_STATUS: SELECT TASK FCT" << endl; }
		    if (ResultSet_next(result)) {
			if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] PROGRAM_STATUS: SELECT TASK VARIABLE" << endl; }
			int tid = ResultSet_getIntByName(result, "tid");
			int type = ResultSet_getIntByName(result, "type");
			string host = ResultSet_getString(result, 3);
			string service = ResultSet_getString(result, 4);
			long tsstart = ResultSet_getIntByName(result, "tsstart");
			long tsend = ResultSet_getIntByName(result, "tsend");
			string usr = ResultSet_getString(result, 7);
			string comment = ResultSet_getString(result, 8);
			int hstid = ResultSet_getIntByName(result, "hstid");
			int srvid = ResultSet_getIntByName(result, "srvid");
			long duration = tsend-tsstart;
			if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] PROGRAM_STATUS: type = " << type << ", target = " << host << "@" << service << ", tsstart = " << tsstart << ", tsend = " << tsend << endl; }
			/* Switch Type */
			switch (type) {
			    case 0: /* ReSchedule Service: SCHEDULE_FORCED_SVC_CHECK;<host_name>;<service_description>;<check_time> */
				snprintf(temp_buffer, sizeof(temp_buffer) - 1, "[%lu] SCHEDULE_FORCED_SVC_CHECK;%s;%s;%lu\n", tsstart, host.c_str(), service.c_str(), tsstart);
				temp_buffer[sizeof(temp_buffer)-1] = '\x0';
				config.WriteToCmdfile(commandfile,temp_buffer);
				/* Update done to true */
				TRY {
				    PreparedStatement_T stu = Connection_prepareStatement(con, "UPDATE monitoring_task SET done=true WHERE tid=?");
				    PreparedStatement_setInt(stu, 1, tid);
				    PreparedStatement_execute(stu);
				    /* Prepare for Mailing */
				    PreparedStatement_T pfm = Connection_prepareStatement(con, "INSERT INTO monitoring_mailing(DONE,HSTID,SRVID,MTYPID,USR,COMMENT,APPID,T1,T2,CREATED) VALUES (false,?,?,'5',?,?,'0','0','0',?)");
				    PreparedStatement_setInt(pfm, 1, hstid);
				    PreparedStatement_setInt(pfm, 2, srvid);
				    PreparedStatement_setString(pfm, 3, usr.c_str());
				    PreparedStatement_setString(pfm, 4, comment.c_str());
				    PreparedStatement_setInt(pfm, 5, timestamp);
				    PreparedStatement_execute(pfm);
				} CATCH(SQLException) {
				    snprintf(temp_buffer, sizeof(temp_buffer) - 1, "id2sc: NEBCALLBACK_PROGRAM_STATUS_DATA SQLException - %s\n", Exception_frame.message);
				    temp_buffer[sizeof(temp_buffer)-1] = '\x0';
				    write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);
				    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] PROGRAM_STATUS: NEBCALLBACK_PROGRAM_STATUS_DATA SQLException - " << Exception_frame.message << endl; }
				} FINALLY {
				    Connection_close(con);
				} END_TRY;
				break;
			    case 1: /* ReSchedule Host: SCHEDULE_FORCED_HOST_CHECK;<host_name>;<check_time> */
				snprintf(temp_buffer, sizeof(temp_buffer) - 1, "[%lu] SCHEDULE_FORCED_HOST_CHECK;%s;%lu\n", tsstart, host.c_str(), tsstart);
				temp_buffer[sizeof(temp_buffer)-1] = '\x0';
				config.WriteToCmdfile(commandfile,temp_buffer);
				/* Update done to true */
				TRY {
				    PreparedStatement_T stu = Connection_prepareStatement(con, "UPDATE monitoring_task SET done=true WHERE tid=?");
				    PreparedStatement_setInt(stu, 1, tid);
				    PreparedStatement_execute(stu);
				    /* Prepare for Mailing */
				    PreparedStatement_T pfm = Connection_prepareStatement(con, "INSERT INTO monitoring_mailing(DONE,HSTID,SRVID,MTYPID,USR,COMMENT,APPID,T1,T2,CREATED) VALUES (false,?,?,'5',?,?,'0','0','0',?)");
				    PreparedStatement_setInt(pfm, 1, hstid);
				    PreparedStatement_setInt(pfm, 2, srvid);
				    PreparedStatement_setString(pfm, 3, usr.c_str());
				    PreparedStatement_setString(pfm, 4, comment.c_str());
				    PreparedStatement_setInt(pfm, 5, timestamp);
				    PreparedStatement_execute(pfm);
				} CATCH(SQLException) {
				    snprintf(temp_buffer, sizeof(temp_buffer) - 1, "id2sc: NEBCALLBACK_PROGRAM_STATUS_DATA SQLException - %s\n", Exception_frame.message);
				    temp_buffer[sizeof(temp_buffer)-1] = '\x0';
				    write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);
				    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] PROGRAM_STATUS: NEBCALLBACK_PROGRAM_STATUS_DATA SQLException - " << Exception_frame.message << endl; }
				} FINALLY {
				    Connection_close(con);
				} END_TRY;
				break;
			    case 2: /* Define Service Downtime: SCHEDULE_SVC_DOWNTIME;<host_name>;<service_desription><start_time>;<end_time>;<fixed>;<trigger_id>;<duration>;<author>;<comment> */
				snprintf(temp_buffer, sizeof(temp_buffer) - 1, "[%lu] SCHEDULE_SVC_DOWNTIME;%s;%s;%lu;%lu;0;0;%lu;%s;%s\n", tsstart, host.c_str(), service.c_str(), tsstart, tsend, duration, usr.c_str(), comment.c_str());
				temp_buffer[sizeof(temp_buffer)-1] = '\x0';
				config.WriteToCmdfile(commandfile,temp_buffer);
				/* Update done to true */
				TRY {
				    PreparedStatement_T ihad = Connection_prepareStatement(con, "INSERT INTO monitoring_downtime(TSSTART,TSEND,USR,COMMENT,HSTID,SRVID,APPID,CREATED) VALUES (?,?,?,?,?,?,'0',?)");
				    PreparedStatement_setInt(ihad, 1, tsstart);
				    PreparedStatement_setInt(ihad, 2, tsend);
				    PreparedStatement_setString(ihad, 3, usr.c_str());
				    PreparedStatement_setString(ihad, 4, comment.c_str());
				    PreparedStatement_setInt(ihad, 5, hstid);
				    PreparedStatement_setInt(ihad, 6, srvid);
				    PreparedStatement_setInt(ihad, 7, timestamp);
				    PreparedStatement_execute(ihad);
				    /* Select ackid */
				    PreparedStatement_T said = Connection_prepareStatement(con, "SELECT dtmid FROM monitoring_downtime WHERE srvid=? AND created=?");
				    PreparedStatement_setInt(said, 1, srvid);
				    PreparedStatement_setInt(said, 2, timestamp);
				    ResultSet_T instanceSAID = PreparedStatement_executeQuery(said);
				    if (ResultSet_next(instanceSAID)) {
					PreparedStatement_T msc3 = Connection_prepareStatement(con, "UPDATE monitoring_status SET dtm=true, dtmid=? WHERE srvid=?");
					PreparedStatement_setInt(msc3, 1, ResultSet_getIntByName(instanceSAID, "dtmid") );
					PreparedStatement_setInt(msc3, 2, srvid );
			    		PreparedStatement_execute(msc3);
				    }
				    /* Abschluss */
				    PreparedStatement_T stu = Connection_prepareStatement(con, "UPDATE monitoring_task SET done=true WHERE tid=?");
				    PreparedStatement_setInt(stu, 1, tid);
				    PreparedStatement_execute(stu);
				    /* Prepare for Mailing */
				    PreparedStatement_T pfm = Connection_prepareStatement(con, "INSERT INTO monitoring_mailing(DONE,HSTID,SRVID,MTYPID,USR,COMMENT,APPID,T1,T2,CREATED) VALUES (false,?,?,'2',?,?,'0',?,?,?)");
				    PreparedStatement_setInt(pfm, 1, hstid);
				    PreparedStatement_setInt(pfm, 2, srvid);
				    PreparedStatement_setString(pfm, 3, usr.c_str());
				    PreparedStatement_setString(pfm, 4, comment.c_str());
				    PreparedStatement_setInt(pfm, 5, tsstart);
				    PreparedStatement_setInt(pfm, 6, tsend);
				    PreparedStatement_setInt(pfm, 7, timestamp);
				    PreparedStatement_execute(pfm);
				} CATCH(SQLException) {
				    snprintf(temp_buffer, sizeof(temp_buffer) - 1, "id2sc: NEBCALLBACK_PROGRAM_STATUS_DATA SQLException - %s\n", Exception_frame.message);
				    temp_buffer[sizeof(temp_buffer)-1] = '\x0';
				    write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);
				    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] PROGRAM_STATUS: NEBCALLBACK_PROGRAM_STATUS_DATA SQLException - " << Exception_frame.message << endl; }
				} FINALLY {
				    Connection_close(con);
				} END_TRY;
				break;
			    case 3: /* Define Host incl. Service Downtime: SCHEDULE_HOST_SVC_DOWNTIME;<host_name>;<start_time>;<end_time>;<fixed>;<trigger_id>;<duration>;<author>;<comment> */
				snprintf(temp_buffer, sizeof(temp_buffer) - 1, "[%lu] SCHEDULE_HOST_SVC_DOWNTIME;%s;%lu;%lu;0;0;%lu;%s;%s\n", tsstart, host.c_str(), tsstart, tsend, duration, usr.c_str(), comment.c_str());
				temp_buffer[sizeof(temp_buffer)-1] = '\x0';
				config.WriteToCmdfile(commandfile,temp_buffer);
				/* Update done to true */
				TRY {
				    PreparedStatement_T ihad = Connection_prepareStatement(con, "INSERT INTO monitoring_downtime(TSSTART,TSEND,USR,COMMENT,HSTID,SRVID,APPID,CREATED) VALUES (?,?,?,?,?,?,'0',?)");
				    PreparedStatement_setInt(ihad, 1, tsstart);
				    PreparedStatement_setInt(ihad, 2, tsend);
				    PreparedStatement_setString(ihad, 3, usr.c_str());
				    PreparedStatement_setString(ihad, 4, comment.c_str());
				    PreparedStatement_setInt(ihad, 5, hstid);
				    PreparedStatement_setInt(ihad, 6, srvid);
				    PreparedStatement_setInt(ihad, 7, timestamp);
				    PreparedStatement_execute(ihad);
				    /* Select ackid */
				    PreparedStatement_T said = Connection_prepareStatement(con, "SELECT dtmid FROM monitoring_downtime WHERE srvid=? AND created=?");
				    PreparedStatement_setInt(said, 1, srvid);
				    PreparedStatement_setInt(said, 2, timestamp);
				    ResultSet_T instanceSAID = PreparedStatement_executeQuery(said);
				    if (ResultSet_next(instanceSAID)) {
					PreparedStatement_T msc3 = Connection_prepareStatement(con, "UPDATE monitoring_status SET dtm=true, dtmid=? WHERE hstid=?");
					PreparedStatement_setInt(msc3, 1, ResultSet_getIntByName(instanceSAID, "dtmid") );
					PreparedStatement_setInt(msc3, 2, hstid );
			    		PreparedStatement_execute(msc3);
				    }
				    /* Abschluss */
				    PreparedStatement_T stu = Connection_prepareStatement(con, "UPDATE monitoring_task SET done=true WHERE tid=?");
				    PreparedStatement_setInt(stu, 1, tid);
				    PreparedStatement_execute(stu);
				    /* Prepare for Mailing */
				    PreparedStatement_T pfm = Connection_prepareStatement(con, "INSERT INTO monitoring_mailing(DONE,HSTID,SRVID,MTYPID,USR,COMMENT,APPID,T1,T2,CREATED) VALUES (false,?,?,'2',?,?,'0',?,?,?)");
				    PreparedStatement_setInt(pfm, 1, hstid);
				    PreparedStatement_setInt(pfm, 2, srvid);
				    PreparedStatement_setString(pfm, 3, usr.c_str());
				    PreparedStatement_setString(pfm, 4, comment.c_str());
				    PreparedStatement_setInt(pfm, 5, tsstart);
				    PreparedStatement_setInt(pfm, 6, tsend);
				    PreparedStatement_setInt(pfm, 7, timestamp);
				    PreparedStatement_execute(pfm);
				} CATCH(SQLException) {
				    snprintf(temp_buffer, sizeof(temp_buffer) - 1, "id2sc: NEBCALLBACK_PROGRAM_STATUS_DATA SQLException - %s\n", Exception_frame.message);
				    temp_buffer[sizeof(temp_buffer)-1] = '\x0';
				    write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);
				    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] PROGRAM_STATUS: NEBCALLBACK_PROGRAM_STATUS_DATA SQLException - " << Exception_frame.message << endl; }
				} FINALLY {
				    Connection_close(con);
				} END_TRY;
				break;
			    case 4: /* Acknowledge Service Problem: ACKNOWLEDGE_SVC_PROBLEM;<host_name>;<service_description>;<sticky>;<notify>;<persistent>;<author>;<comment> */
				snprintf(temp_buffer, sizeof(temp_buffer) - 1, "[%lu] ACKNOWLEDGE_SVC_PROBLEM;%s;%s;2;1;1;%s;%s\n", tsstart, host.c_str(), service.c_str(), usr.c_str(), comment.c_str());
				temp_buffer[sizeof(temp_buffer)-1] = '\x0';
				config.WriteToCmdfile(commandfile,temp_buffer);
				/* Update done to true */
				TRY {
				    PreparedStatement_T ihad = Connection_prepareStatement(con, "INSERT INTO monitoring_acknowledge(TS,USR,COMMENT,HSTID,SRVID,APPID,CREATED) VALUES (?,?,?,?,?,'0',?)");
				    PreparedStatement_setInt(ihad, 1, tsstart);
				    PreparedStatement_setString(ihad, 2, usr.c_str());
				    PreparedStatement_setString(ihad, 3, comment.c_str());
				    PreparedStatement_setInt(ihad, 4, hstid);
				    PreparedStatement_setInt(ihad, 5, srvid);
				    PreparedStatement_setInt(ihad, 6, timestamp);
				    PreparedStatement_execute(ihad);
				    /* Select ackid */
				    PreparedStatement_T said = Connection_prepareStatement(con, "SELECT ackid FROM monitoring_acknowledge WHERE srvid=? AND created=?");
				    PreparedStatement_setInt(said, 1, srvid);
				    PreparedStatement_setInt(said, 2, timestamp);
				    ResultSet_T instanceSAID = PreparedStatement_executeQuery(said);
				    if (ResultSet_next(instanceSAID)) {
					PreparedStatement_T msc3 = Connection_prepareStatement(con, "UPDATE monitoring_status SET ack=true, ackid=? WHERE srvid=?");
					PreparedStatement_setInt(msc3, 1, ResultSet_getIntByName(instanceSAID, "ackid") );
					PreparedStatement_setInt(msc3, 2, srvid );
			    		PreparedStatement_execute(msc3);
				    }
				    /* Abschluss */
				    PreparedStatement_T stu = Connection_prepareStatement(con, "UPDATE monitoring_task SET done=true WHERE tid=?");
				    PreparedStatement_setInt(stu, 1, tid);
				    PreparedStatement_execute(stu);
				    /* Prepare for Mailing */
				    PreparedStatement_T pfm = Connection_prepareStatement(con, "INSERT INTO monitoring_mailing(DONE,HSTID,SRVID,MTYPID,USR,COMMENT,APPID,T1,T2,CREATED) VALUES (false,?,?,'1',?,?,'0','0','0',?)");
				    PreparedStatement_setInt(pfm, 1, hstid);
				    PreparedStatement_setInt(pfm, 2, srvid);
				    PreparedStatement_setString(pfm, 3, usr.c_str());
				    PreparedStatement_setString(pfm, 4, comment.c_str());
				    PreparedStatement_setInt(pfm, 5, timestamp);
				    PreparedStatement_execute(pfm);
				} CATCH(SQLException) {
				    snprintf(temp_buffer, sizeof(temp_buffer) - 1, "id2sc: NEBCALLBACK_PROGRAM_STATUS_DATA SQLException - %s\n", Exception_frame.message);
				    temp_buffer[sizeof(temp_buffer)-1] = '\x0';
				    write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);
				    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] PROGRAM_STATUS: NEBCALLBACK_PROGRAM_STATUS_DATA SQLException - " << Exception_frame.message << endl; }
				} FINALLY {
				    Connection_close(con);
				} END_TRY;
				break;
			    case 5: /* Acknowledge Host Problem: ACKNOWLEDGE_HOST_PROBLEM;<host_name>;<sticky>;<notify>;<persistent>;<author>;<comment> */
				snprintf(temp_buffer, sizeof(temp_buffer) - 1, "[%lu] ACKNOWLEDGE_HOST_PROBLEM;%s;2;1;1;%s;%s\n", tsstart, host.c_str(), usr.c_str(), comment.c_str());
				temp_buffer[sizeof(temp_buffer)-1] = '\x0';
				config.WriteToCmdfile(commandfile,temp_buffer);
				/* Update done to true */
				TRY {
				    PreparedStatement_T ihad = Connection_prepareStatement(con, "INSERT INTO monitoring_acknowledge(TS,USR,COMMENT,HSTID,SRVID,APPID,CREATED) VALUES (?,?,?,?,?,'0',?)");
				    PreparedStatement_setInt(ihad, 1, tsstart);
				    PreparedStatement_setString(ihad, 2, usr.c_str());
				    PreparedStatement_setString(ihad, 3, comment.c_str());
				    PreparedStatement_setInt(ihad, 4, hstid);
				    PreparedStatement_setInt(ihad, 5, srvid);
				    PreparedStatement_setInt(ihad, 6, timestamp);
				    PreparedStatement_execute(ihad);
				    /* Select ackid */
				    PreparedStatement_T said = Connection_prepareStatement(con, "SELECT ackid FROM monitoring_acknowledge WHERE srvid=? AND created=?");
				    PreparedStatement_setInt(said, 1, srvid);
				    PreparedStatement_setInt(said, 2, timestamp);
				    ResultSet_T instanceSAID = PreparedStatement_executeQuery(said);
				    if (ResultSet_next(instanceSAID)) {
					PreparedStatement_T msc3 = Connection_prepareStatement(con, "UPDATE monitoring_status SET ack=true, ackid=? WHERE srvid=?");
					PreparedStatement_setInt(msc3, 1, ResultSet_getIntByName(instanceSAID, "ackid") );
					PreparedStatement_setInt(msc3, 2, srvid );
			    		PreparedStatement_execute(msc3);
				    }
				    /* Abschluss */
				    PreparedStatement_T stu = Connection_prepareStatement(con, "UPDATE monitoring_task SET done=true WHERE tid=?");
				    PreparedStatement_setInt(stu, 1, tid);
				    PreparedStatement_execute(stu);
				    /* Prepare for Mailing */
				    PreparedStatement_T pfm = Connection_prepareStatement(con, "INSERT INTO monitoring_mailing(DONE,HSTID,SRVID,MTYPID,USR,COMMENT,APPID,T1,T2,CREATED) VALUES (false,?,?,'1',?,?,'0','0','0',?)");
				    PreparedStatement_setInt(pfm, 1, hstid);
				    PreparedStatement_setInt(pfm, 2, srvid);
				    PreparedStatement_setString(pfm, 3, usr.c_str());
				    PreparedStatement_setString(pfm, 4, comment.c_str());
				    PreparedStatement_setInt(pfm, 5, timestamp);
				    PreparedStatement_execute(pfm);
				} CATCH(SQLException) {
				    snprintf(temp_buffer, sizeof(temp_buffer) - 1, "id2sc: NEBCALLBACK_PROGRAM_STATUS_DATA SQLException - %s\n", Exception_frame.message);
				    temp_buffer[sizeof(temp_buffer)-1] = '\x0';
				    write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);
				    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] PROGRAM_STATUS: NEBCALLBACK_PROGRAM_STATUS_DATA SQLException - " << Exception_frame.message << endl; }
				} FINALLY {
				    Connection_close(con);
				} END_TRY;
				break;
			    default:
				break;
			}
		    }
		} CATCH(SQLException) {
		    snprintf(temp_buffer, sizeof(temp_buffer) - 1, "id2sc: NEBCALLBACK_PROGRAM_STATUS_DATA SQLException - %s\n", Exception_frame.message);
		    temp_buffer[sizeof(temp_buffer)-1] = '\x0';
		    write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);
		    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] PROGRAM_STATUS: NEBCALLBACK_PROGRAM_STATUS_DATA SQLException - " << Exception_frame.message << endl; }
		} FINALLY {
		    Connection_close(con);
		} END_TRY;
	    }
	    break;
	case NEBCALLBACK_DOWNTIME_DATA:
	    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] EVENT-SWITCH: NEBCALLBACK_DOWNTIME_DATA " << endl; }
	    if ((dtdata = (nebstruct_downtime_data *)data)) {
		if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] DOWNTIME: " << dtdata->downtime_id << endl; }
		/* get data from struct */
		es[0] = escape_buffer(dtdata->host_name);
		es[1] = escape_buffer(dtdata->service_description);
		int dtype = dtdata->downtime_type;
		time_t dstart = dtdata->start_time;
		time_t dend = dtdata->end_time;
		int timestamp = (int)time(NULL);
		if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] DOWNTIME GET_INFO: host = " << es[0] << ", service = " << es[1] << " __ " << dtype << "::" << dstart << "::" << dend << endl; }
		con = ConnectionPool_getConnection(pool);
		TRY {
		    PreparedStatement_T ghid = Connection_prepareStatement(con, "select a.hstid,a.srvid from monitoring_status a, monitoring_info_host b, monitoring_info_service c, monitoring_downtime d where a.hstid=b.hstid and a.srvid=c.srvid and a.dtmid=d.dtmid and b.instid=? and b.hstln=? and c.srvna=? and a.dtm=true and d.tsend<?");
		    PreparedStatement_setInt(ghid, 1, instid);
		    PreparedStatement_setString(ghid, 2, es[0].c_str());
		    PreparedStatement_setString(ghid, 3, es[1].c_str());
		    PreparedStatement_setInt(ghid, 4, timestamp);
		    ResultSet_T instanceGHID = PreparedStatement_executeQuery(ghid);
		    if (ResultSet_next(instanceGHID)) {
		        int hstid = ResultSet_getIntByName(instanceGHID, "hstid");
		        int srvid = ResultSet_getIntByName(instanceGHID, "srvid");
		        if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] EVENT-SWITCH-DOWNTIME: Host ID :: " << hstid << " Service ID :: " << srvid << endl; }
		        /* Remove Downtime */
			PreparedStatement_T msc2 = Connection_prepareStatement(con, "UPDATE monitoring_status SET dtm=false, dtmid='0' WHERE srvid=?");
			PreparedStatement_setInt(msc2, 1, srvid);
			PreparedStatement_execute(msc2);
			/* Prepare for Mailing */
			PreparedStatement_T pfm = Connection_prepareStatement(con, "INSERT INTO monitoring_mailing(DONE,HSTID,SRVID,MTYPID,USR,COMMENT,APPID,T1,T2,CREATED) VALUES (false,?,?,'6',encode('system','base64'),encode('Die Downtime des Services ist beendet.','base64'),'0',?,?,?)");
			PreparedStatement_setInt(pfm, 1, hstid);
			PreparedStatement_setInt(pfm, 2, srvid);
			PreparedStatement_setInt(pfm, 3, dstart);
			PreparedStatement_setInt(pfm, 4, dend);
			PreparedStatement_setInt(pfm, 5, timestamp);
			PreparedStatement_execute(pfm);
		    }
		} CATCH(SQLException) {
		    snprintf(temp_buffer, sizeof(temp_buffer) - 1, "id2sc: NEBCALLBACK_DOWNTIME_DATA SQLException - %s :: %s :: %s\n", es[0].c_str(), es[1].c_str(), Exception_frame.message);
		    temp_buffer[sizeof(temp_buffer)-1] = '\x0';
		    write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);
		    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << "] id2sc: NEBCALLBACK_DOWNTIME_DATA SQLException - " << Exception_frame.message << endl; }
		} FINALLY {
		    Connection_close(con);
		    if (debug.compare("on") == 0) { debugfile << "[" << time(NULL) << " DOWNTIME_CONNECTION_CLOSE: done. " << endl; }
		} END_TRY;
	    }
	    break;
	default:
	    break;
    }

    return 0;
}

int process_module_args(char *args) {
    int k;
    char *keyval[10];
    if (args == NULL) {
	return 0;
    } else {
	config.Stripe(args);
	k=0; keyval[k] = strtok(args,"="); while(keyval[k]!=NULL) { keyval[++k] = strtok(NULL,"_"); }
	config.Load(keyval[1]);
	config.Get("id.name",idname);
	config.Get("id.idtf",identifier);
	config.Get("pg.url",pgurl);
	config.Get("debug",debug);
	config.Get("lg.file",dbgfile);
	config.Get("command.file",commandfile);
    }
    return 0;
}

/* escape special characters in string */
extern "C" char *escape_buffer(char *buffer) {
    char *newbuf;
    register int x = 0;
    register int y = 0;
    register int len = 0;

    if (buffer == NULL)
        return NULL;

    /* allocate memory for escaped string */
    if ((newbuf = (char *)malloc((strlen(buffer) * 2) + 1)) == NULL)
        return NULL;

    /* initialize string */
    newbuf[0] = '\x0';

    len = (int)strlen(buffer);
    for (x = 0; x < len; x++) {
        if (buffer[x] == '\t') {
            newbuf[y++] = '\\';
            newbuf[y++] = 't';
        } else if (buffer[x] == '\r') {
            newbuf[y++] = '\\';
            newbuf[y++] = 'r';
        } else if (buffer[x] == '\n') {
            newbuf[y++] = '\\';
            newbuf[y++] = 'n';
        } else if (buffer[x] == '\\') {
            newbuf[y++] = '\\';
            newbuf[y++] = '\\';
        } else
            newbuf[y++] = buffer[x];
    }

    /* terminate new string */
    newbuf[y++] = '\x0';

    return newbuf;
}
