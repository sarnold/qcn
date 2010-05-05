/* This program 2will monitor the "live" triggers in memory 
   (the mysql trigmem.qcn_trigger_memory table)

The general idea is that a query is run every few seconds to see if any quakes were
detected by QCN sensors via lat/lng & time etc

if there were "hits" in a certain area, then flag this as a quake and put in the qcn_quake table

logic:
  1) check for numerous trickles within a region in a short time period (i.e. 10 seconds) -- see g_strSQLTrigger
  2) if there are numerous trickles - see if this event has been reported in qcnalpha.qcn_quake - lookup by time/lat/lng
  3) if has been reported, use that event to tag trickles; else make an entry in qcn_quake and tag triggers
  4) request uploads from these triggers as appropriate

Example usage:
./qcn_trigmon -d 3 -sleep_interval 1 -count 2 -time_interval 100 -delete_interval 300

(c) 2010  Stanford University School of Earth Sciences

*/

#include "qcn_trigmon.h"

DB_CONN trigmem_db;

double g_dTimeCurrent = 0.0;  // global for current time

// global params for trigger monitoring behavior
double g_dSleepInterval = -1.0;   // number of seconds to sleep between trigmem enumerations
int g_iTriggerTimeInterval = -1;  // number of seconds to check for triggers (i.e. "time width" of triggers for an event)
int g_iTriggerCount = -1;         // number of distinct hostid triggers to count as a qcn quake event
int g_iTriggerDeleteInterval = -1;  // number of seconds to delete trigmem table array

#define QUERY_DELETE 0
#define QUERY_LATLNG 1
#define QUERY_QUAKE  2

char g_strSQL[6][256] = { {""}, {""}, {""}, {""}, {""}, {""} };

// keep a global vector of recent QCN quake events, remove them after an hour or so
// this way follup triggers can be matched to a known QCN quake event (i.e. qcn_quake table)
//vector<QCN_QUAKE_EVENT> vQuakeEvent;

void close_db()
{
   log_messages.printf(MSG_DEBUG, "Closing databases.\n");
   boinc_db.close();
   trigmem_db.close();
}

void do_delete_trigmem()  
{
    if (!g_strSQL[QUERY_DELETE][0]) { // err, something wrong, no query string
        log_messages.printf(MSG_CRITICAL,
            "do_delete_trigmem() error: %s\n", "No Query String Found"
        );
        exit(10);
    }

    int retval = trigmem_db.do_query(g_strSQL[QUERY_DELETE]);
    if (retval) {
        log_messages.printf(MSG_CRITICAL,
            "do_delete_trigmem() error: %s\n", boincerror(retval)
        );
    }
    else {
        log_messages.printf(MSG_DEBUG,
            "do_delete_trigmem(): Removed old triggers from memory\n"
        );
    }
}

// first test query - simple just rounts lat/lng to integers, does 10 seconds
void setQueries() 
{
   sprintf(g_strSQL[QUERY_DELETE],
    "DELETE FROM trigmem.qcn_trigger_memory WHERE time_trigger<(unix_timestamp() - %d"
       ") OR time_trigger>(unix_timestamp()+1.0)",
      g_iTriggerDeleteInterval);

   sprintf(g_strSQL[QUERY_LATLNG],
"select  "
       "  round(latitude,0) rlat,  "
       "  round(longitude,0) rlng,  "
       "  count(distinct hostid) ctr, "
       "  min(time_trigger) time_min, "
       "  max(time_trigger) time_max "
      "from trigmem.qcn_trigger_memory "
      "where time_trigger > (unix_timestamp() - %d) "
      "group by rlat, rlng "
      "having ctr > %d",
         g_iTriggerTimeInterval,
         g_iTriggerCount
    );

}

void do_trigmon() 
{
   //vector<DB_QCN_TRIGGER_MEMORY> vqtm;

    //fprintf(stdout, "%s\n", g_strSQL[QUERY_LATLNG]);
    // first get a vector of potential matchups by region i.e. lat/lnt/count/time
    int retval = trigmem_db.do_query(g_strSQL[QUERY_LATLNG]);
    if (retval) { // big error, should probably quit as may have lost database connection
        log_messages.printf(MSG_CRITICAL,
            "do_trigmon() error: %s - %s\n", "Query Error", boincerror(retval)
        );
        exit(10);
    }

    MYSQL_ROW row;
    MYSQL_RES* rp;
    int numRows = 0;

    rp = mysql_store_result(trigmem_db.mysql);
    while ((row = mysql_fetch_row(rp))) {
      // if we have rows from the above query, then we have detected an event
      // so need to see if this is in our current event array, and add it if it isn't

      numRows++;
      double dLat = atof(row[0]);
      double dLng = atof(row[1]);
      int iCtr = atoi(row[2]);
      double dTimeMin = atof(row[3]);
      double dTimeMax = atof(row[4]);
      log_messages.printf(MSG_DEBUG,
          "  #%d  %f - (%f, %f) - %d distinct hosts from %f to %f\n", 
           numRows, g_dTimeCurrent, dLat, dLng, iCtr, dTimeMin, dTimeMax
      );

      checkQCNEvent(dLat, dLng, iCtr, dTimeMin, dTimeMax);
     
    }
    if (!numRows) { 
       fprintf(stdout, "  No rows found\n");
    }

    mysql_free_result(rp);


}

// check this potential event is in our vector of quake events (i.e. we may have
// already processed triggers from other hosts for this event) -- also make sure
// this event is reported in the qcn_quake table and save the qcn_quakeid to update the
// qcn_trigger entries
void checkQCNEvent(const double& dLat, 
    const double& dLng, 
    const int& iCtr, 
    const double& dTimeMin, 
    const double& dTimeMax)
{
   int iQCNQuakeID = 0L;  // store qcn_quakeid for quick retrieval later

   // enumerate through our vector to see if it matches within reason
   // based on lat/lng & trigger time
/*
   vector<QCN_QUAKE_EVENT>::const_iterator ciqe;
   for (ciqe = vQuakeEvent.begin(); ciqe != vQuakeEvent.end(); ciqe++) {
      if ( floor(ciqe->latitude) == floor(dLat) 
        && floor(ciqe->longitude) == floor(dLng)
        && ciqe->dTime > g_dTimeCurrent - 30
        && ciqe->dTime < g_dTimeCurrent + 5
       ) {
         // this record matches so get the qcn_quakeid
         iQCNQuakeID = ciqe->qcn_quakeid;
         break;
       }
   }
 
   if (!iQCNQuakeID) { // didn't find anything so make a record in qcn_quake table and an entry 
*/
     DB_QCN_QUAKE dqq;

     sprintf(g_strSQL[QUERY_QUAKE],
        "WHERE ROUND(latitude,0) = ROUND(%f,0) "
        "  AND ROUND(longitude,0)= ROUND(%f,0) "
        "  AND time_utc BETWEEN %f AND %f ",
          dLat, dLng, dTimeMin-10.0, dTimeMax+10.0
     );

      // search via dqq.lookup(WHERE_CLAUSE)
     int iRetVal = dqq.lookup(g_strSQL[QUERY_QUAKE]);
     switch(iRetVal) {
        case ERR_DB_NOT_FOUND:  // insert new qcn_quake record
           dqq.clear();
           dqq.time_utc = dTimeMin;  // min time of triggers found // (dTimeMin + dTimeMax) / 2.0;  // avg time of trigger?
           dqq.latitude = dLat;
           dqq.longitude = dLng;
           sprintf(dqq.description, "QCN Event %ld", (long) dTimeMin);
           dqq.processed = 0;
           sprintf(dqq.guid, "QCN_%ld", (long) dTimeMin);
           iRetVal = dqq.insert();
           if (iRetVal) {
              log_messages.printf(
                 SCHED_MSG_LOG::MSG_CRITICAL, "qcn_quake insert failed for (%f,%f) at %f\n",
                      dLat, dLng, dTimeMin);
           } else { // trigger got in OK
              iQCNQuakeID = dqq.db->insert_id();
           }
           break;
        case 0: // found, get the ID
           iQCNQuakeID = dqq.id;
           break;
        default:  // other database error
           log_messages.printf(
              SCHED_MSG_LOG::MSG_CRITICAL, "qcn_quake lookupfailed for (%f,%f) at %f\n%s\n\n",
                 dLat, dLng, dTimeMin, g_strSQL[QUERY_QUAKE]);
     }

     if (!iQCNQuakeID) return; // must have had an error

     // if here we have a quake id we can associate with the triggers and update the trigger table

}

int main(int argc, char** argv) 
{
    int retval;

    //vQuakeEvent.clear();
 
    retval = config.parse_file();
    if (retval) {
        log_messages.printf(MSG_CRITICAL,
            "Can't parse config.xml: %s\n", boincerror(retval)
        );
        return 1;
    }

    for (int i=1; i<argc; i++) {
        if (!strcmp(argv[i], "-d")) {
            log_messages.set_debug_level(atoi(argv[++i]));
        } else if (!strcmp(argv[i], "-sleep_interval")) {
            g_dSleepInterval = atof(argv[++i]);
        } else if (!strcmp(argv[i], "-count")) {
            g_iTriggerCount = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "-time_interval")) {
            g_iTriggerTimeInterval = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "-delete_interval")) {
            g_iTriggerDeleteInterval = atoi(argv[++i]);
        } else {
            log_messages.printf(MSG_CRITICAL,
                "bad cmdline arg: %s\n\n"
                "Example usage: qcn_trigmon -d 3 -sleep_interval 3 -count 10 -time_interval 10 -delete_interval 300\n\n"
             , argv[i]
            );
            return 2;
        }
    }
    if (g_dSleepInterval < 0) g_dSleepInterval = TRIGGER_SLEEP_INTERVAL;
    if (g_iTriggerTimeInterval < 0) g_iTriggerTimeInterval = TRIGGER_TIME_INTERVAL;
    if (g_iTriggerCount < 0) g_iTriggerCount = TRIGGER_COUNT;
    if (g_iTriggerDeleteInterval < 0) g_iTriggerDeleteInterval = TRIGGER_DELETE_INTERVAL;

    install_stop_signal_handler();
    atexit(close_db);

    setQueries();  // setup the sql queries used by this process

    retval = boinc_db.open(
        config.db_name, config.db_host, config.db_user, config.db_passwd
    );
    if (retval) {
        log_messages.printf(MSG_CRITICAL,
            "boinc_db.open: %d; %s\n", retval, boinc_db.error_string()
        );
        return 3;
    }
    retval = trigmem_db.open(
        DB_TRIGMEM, config.db_host, config.db_user, config.db_passwd
    );
    if (retval) {
        log_messages.printf(MSG_CRITICAL,
            "trigmem_db.open: %d; %s\n", retval, boinc_db.error_string()
        );
        return 4;
    }
    retval = boinc_db.set_isolation_level(REPEATABLE_READ);
    if (retval) {
        log_messages.printf(MSG_CRITICAL,
            "boinc_db.set_isolation_level: %d; %s\n", retval, boinc_db.error_string()
        );
    }

    log_messages.printf(MSG_NORMAL,
            "qcn_trigmon started with the following options:\n"
            "  -count           = %d\n" 
            "  -time_interval   = %d\n" 
            "  -sleep_interval  = %f\n" 
            "  -delete_interval = %d\n\n",
         g_iTriggerCount,
         g_iTriggerTimeInterval,
         g_dSleepInterval,
         g_iTriggerDeleteInterval
    ); 

    //signal(SIGUSR1, show_state);
    double dtDelete = 0.0f; // time to delete old triggers from memory
    while (1) {
      g_dTimeCurrent = dtime();
      double dtEnd = g_dTimeCurrent + g_dSleepInterval;
      if (g_dTimeCurrent > dtDelete) {
         do_delete_trigmem();  // get rid of triggers every once in awhile
         dtDelete = g_dTimeCurrent + g_iTriggerDeleteInterval;
      }
      do_trigmon();
      check_stop_daemons();
      g_dTimeCurrent = dtime();
      if (g_dTimeCurrent < dtEnd && (dtEnd - g_dTimeCurrent) < 60.0) { // sleep a bit if not less than 60 seconds
          log_messages.printf(MSG_DEBUG, "Sleeping %f seconds....\n", dtEnd - g_dTimeCurrent);
          boinc_sleep(dtEnd - g_dTimeCurrent);
      } 
    }
    return 0;
}

