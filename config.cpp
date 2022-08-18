/*
 * EGTS Relay
 *
 * Copyright (c) 2018, Navigation Solutions LLC. and/or its affiliates. All rights reserved.
 */

/*
 * ChangeLog:
 *
 * 2020-01-28 fmnl001 Initial creation.
 *
 */

#include "config.h"

#define default_log_file_name "vistd.log"
#define default_log_file_path "/tmp"
#define default_db_host "localhost"
static int  default_db_port=3306;
#define default_db_name "gmng"
static int MACHINE_POSITION_POLL_TIMEOUT = 30; // seconds
static int log_level_default = 5;

//------------------------------------------------------------------------------
Config::Config():dbms_host(default_db_host),
                 dbms_port(default_db_port),
                 dbms_dbname(default_db_name),
                 log_file_path(default_log_file_path),
                 log_file_name(default_log_file_name),
                 log_level(log_level_default),
                 position_poll_timeout(MACHINE_POSITION_POLL_TIMEOUT),                 
                 log2console(false)
{
}

//------------------------------------------------------------------------------
std::string
Config::log_location() const
{
  auto rc = log_file_path / log_file_name;
  return rc.string();
}

//------------------------------------------------------------------------------
std::string
Config::db_login() const {
  return dbms_login;
}

//------------------------------------------------------------------------------
std::string
Config::db_pwd() const {
  return dbms_pwd;
}

//------------------------------------------------------------------------------
int
Config::db_port() const {
  return dbms_port;
}

//------------------------------------------------------------------------------
std::string
Config::db_name() const {
  return dbms_dbname;
}

//------------------------------------------------------------------------------
std::string
Config::db_host() const {
  return dbms_host;
}

//------------------------------------------------------------------------------
bool
Config::consolelog() const {
  return log2console;
}

//------------------------------------------------------------------------------
void
Config::consolelog(bool flag) {
  log2console = flag;
}
