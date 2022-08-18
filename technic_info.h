/*
 *
 *
 */

#pragma once

#include "types.h"
#include <stdlib.h>
#include <string>
#include <map>

/*
 *
 */
struct Technic_info
{
  Technic_info ();
  ~Technic_info ();

  std::string dt;
  std::string vec;
  std::string mid;
  int mid_int;
  std::string lat;
  std::string lon;
  std::string fuel;
  std::string speed;

  std::string height;
  std::string course;
  std::map<std::string,std::string> analytic_entity;

  void reset_data();
  friend std::ostream& operator<<(std::ostream& os, const Technic_info& ti);
  void store_to_db();

protected:
  size_t pack_as_blob();
  time_t datetime_as_unix();

private:
 BYTE *blob_;
};
