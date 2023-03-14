/*
 *
 *
 */

#include "technic_info.h"
#include "rmc.h"
#include <parser2db.h>
#include <arpa/inet.h>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <ostream>
#include <math.h>
#include <boost/log/trivial.hpp>

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#define VIST_ID_PREFIX_IN_DB "VST"
//------------------------------------------------------------------------------
Technic_info::Technic_info (): blob_(nullptr)
{
  reset_data();
}

//------------------------------------------------------------------------------
Technic_info::~Technic_info ()
{
  free(blob_);
  blob_ = nullptr;
}

//------------------------------------------------------------------------------
void
Technic_info::reset_data()
{
  dt = "";
  vec = "";
  mid = "";
  lat = "";
  lon = "";
  fuel = "";
  speed = "";
  course = "";
  height = "";

  mid_int = 0;

  analytic_entity.clear();

//  free(blob_);
//  blob_ = nullptr;
}

//------------------------------------------------------------------------------
time_t
Technic_info::datetime_as_unix()
{
  time_t rc = 0;
  std::tm t = {};
  std::istringstream ss(dt);

//  ss.imbue(std::locale("en_US.utf-8"));

  ss >> std::get_time(&t, "%d.%m.%Y %H:%M:%S");
  if (ss.fail()) {
//    BOOST_LOG_TRIVIAL(error) << "datetime conversion to unix failed: " << dt;
    throw std::invalid_argument("datetime conversion to unix failed");
  } else {
    rc = timegm(&t);
  }

  return rc;
}

//------------------------------------------------------------------------------
size_t
Technic_info::pack_as_blob()
{
  free(blob_);

  size_t rc=0;

  size_t size = sizeof(Rmc_);
  blob_ = reinterpret_cast<BYTE *>(malloc(size + analytic_entity.size()*(sizeof(Sql_rmc_field_analog) + sizeof(Rmc_field_header))
                                          + (fuel.empty() ? 0 : (sizeof(Sql_rmc_field_analog) + sizeof(Rmc_field_header)))));
  std::fill(blob_, blob_+size, 0);

  auto sqlrmc = reinterpret_cast<Rmc_ *> (blob_);

  sqlrmc->header.type = FIELD_TYPE_POSITION;
  sqlrmc->header.length = sizeof(Rmc_) - sizeof(sqlrmc->header);

  sqlrmc->dt = htonl(datetime_as_unix());
  sqlrmc->ext.bits.valid = 1;

  if (!lat.empty()) {
    sqlrmc->latitude = htonl(std::stof(lat) * 600000);
  } else {
    BOOST_LOG_TRIVIAL(warning) << "got latitude empty, skip processing whole record";
    return rc;
  }
  if (!lon.empty()) {
    sqlrmc->longitude = htonl(std::stof(lon) * 600000);
  } else {
    BOOST_LOG_TRIVIAL(warning) << "got longitude empty, skip processing whole record";
    return rc;
  }

  unsigned int sp = 0;
  if (!speed.empty()) {
    sp = static_cast<DWORD>(roundf(std::stof(speed)*1000.0f));
    sqlrmc->speed_low = htons(sp&0x0000FFFF);
    sqlrmc->ext.bits.speed_hi = (sp&0x00030000)>>16;
  }

  char buffer[80];
  time_t navtime = ntohl(sqlrmc->dt);
  strftime(buffer,80,"%F %T%z", gmtime(&navtime));

  BYTE * pdata = blob_ + sizeof(Rmc_);

  if (!fuel.empty()) {
    try {
      auto f = std::stoi(fuel);
      BOOST_LOG_TRIVIAL(trace) << "write to blob fuel val: " << f << "\n";
//      data = reinterpret_cast<BYTE *>(realloc(data, size + sizeof(Sql_rmc_field_analog) + sizeof(Rmc_field_header)));
      auto fld = reinterpret_cast<Rmc_field_header *> (pdata);
      fld->type  = FIELD_TYPE_AIN|0x80;
      fld->length = sizeof(Sql_rmc_field_analog);
      pdata += sizeof(*fld);
      auto dt = reinterpret_cast<Sql_rmc_field_analog *> (pdata);
      dt[0].pos = 0;
      dt[0].val = htons(f);
      pdata += sizeof(*dt);
    } catch (std::exception& ex) {
        BOOST_LOG_TRIVIAL(error) << "got std::exception: " << ex.what();
    }
  }

  try {
    for (const auto & kv : analytic_entity) {
      BOOST_LOG_TRIVIAL(trace) << "write to blob analytic entity id[" << kv.first << "]: " << kv.second << "\n";
      auto fld = reinterpret_cast<Rmc_field_header *> (pdata);
      fld->type  = FIELD_TYPE_AIN|0x80;
      fld->length = sizeof(Sql_rmc_field_analog);
      pdata += sizeof(*fld);
      auto dt = reinterpret_cast<Sql_rmc_field_analog *> (pdata);

      switch (std::stoi(kv.first)) {
      case 741:
        dt[0].pos = 1;
        break;
      case 742:
        dt[0].pos = 2;
        break;
      case 743:
        dt[0].pos = 3;
        break;
      case 744:
        dt[0].pos = 4;
        break;
      case 41:
        dt[0].pos = 5;
        break;
      case 202:
        dt[0].pos = 6;
        break;

      default:
        dt[0].pos = 1;
      }

      dt[0].val = htons(std::stoi(kv.second));
      pdata += sizeof(*dt);
    }
  }
  catch (std::exception& ex) {
    BOOST_LOG_TRIVIAL(error) << "got std::exception: " << ex.what();
  }

  return pdata - blob_;;
}


#include <iostream>
#include "hexdump.h"
//------------------------------------------------------------------------------
void
Technic_info::store_to_db()
{
  auto res = pack_as_blob();

//  std::cerr << *this;

  if (res) {
    std::string id = VIST_ID_PREFIX_IN_DB + vec;

//    if (res > sizeof(Rmc_) + sizeof(Sql_rmc_field_analog) + sizeof(Rmc_field_header)) {
      std::ostringstream os;
      neolib::hex_dump(blob_, res, os);
      BOOST_LOG_TRIVIAL(trace) << "[" << this->vec << "][" << id << "] blob " << res << " bytes hex dump:\n" << os.str();
//    }

    db_store_data(id.c_str(),
                  datetime_as_unix(),
                  (const BYTE*) blob_,
                  res,
                  0);
  } else {
      BOOST_LOG_TRIVIAL(warning) << "blob pack fail, res: " << res;
  }
}

//------------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& os, const Technic_info& ti)
{
  os << "---------------------------------------------\n";
  os << "vehicle_export_code: " << ti.vec << "\n";
  os << "message_id: " << ti.mid << "\n";
  os << "datetime: " << ti.dt << "\n";
  os << "latitude: " << ti.lat << "\n";
  os << "longitude: " << ti.lon << "\n";
  if (!ti.speed.empty())
    os << "speed: " << ti.speed << "\n";
  if (!ti.course.empty())
    os << "course: " << ti.course << "\n";
  if (!ti.height.empty())
    os << "height: " << ti.height << "\n";
  if (!ti.fuel.empty())
    os << "fuel: " << ti.fuel << "\n";
  if (!ti.analytic_entity.empty()) {
    os << "AnalyticEntity:\n";
    for (auto const &i: ti.analytic_entity) {
      os << "\t[" << i.first << "]:"  << i.second << "\n";
    }
  }
  os << "---------------------------------------------\n";
  return os;
}
