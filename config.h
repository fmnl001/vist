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

#pragma once

#include <boost/filesystem.hpp>
#include <string>

namespace fs = boost::filesystem;

/*
 *
 */
class Config
{
public:
  Config();

  std::string dbms_host;
  int dbms_port;
  std::string dbms_dbname;
  std::string dbms_login;
  std::string dbms_pwd;

  fs::path log_file_path;
  fs::path log_file_name;
  int log_level;
  int service_poll_timeout;
  int service_reply_timeout;

  std::string vurl;
  std::string vurl2;
  std::string vuser;
  std::string vpwd;

  std::string log_location() const;
  std::string db_host() const;
  int db_port() const;
  std::string db_name() const;
  std::string db_login() const;
  std::string db_pwd() const;
  bool consolelog() const;
  void consolelog(bool flag);

private:
  bool log2console;
};

