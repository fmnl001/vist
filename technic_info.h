/*
 *
 *
 */

#pragma once

#include "types.h"
#include <stdlib.h>
#include <string>

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

  void reset_data();
  friend std::ostream& operator<<(std::ostream& os, const Technic_info& ti);
  void store_to_db();

protected:
  size_t pack_as_blob();
  time_t datetime_as_unix();

private:
 BYTE *blob_;
};
