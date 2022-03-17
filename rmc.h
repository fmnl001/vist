/*
 *
 */

#pragma once
#include "types.h"

#define FIELD_TYPE_POSITION 0
#define FIELD_TYPE_DIN      2
#define FIELD_TYPE_DOUT     3
#define FIELD_TYPE_AIN      1
#define FIELD_TYPE_CNT      5
#define FIELD_TYPE_ALARM    8
#define FIELD_TYPE_IBUTTON  10
#define FIELD_TYPE_CAN      11
#define FIELD_TYPE_AUTOGRAPH_GAS_LOAD_DATA 12
#define FIELD_TYPE_LOCATION_TIME 100

using AIN_VALUE_DATA=WORD;

#pragma pack(1)

/*
 *
 */
struct Rmc_field_header
{
  BYTE type;
  BYTE length;
};

/*
 *
 */
struct Rmc_ext_field_header
{
  BYTE type;
  WORD length;
};

/*
 *
 */
struct Sql_rmc_field_analog
{
  BYTE pos;
  AIN_VALUE_DATA val;
};

/*
 *
 */
struct Rmc_
{
  Rmc_field_header header;
  int            latitude;  // minutes * 10000 (>0 - N, <0 - S)
  int            longitude; // minutes * 10000 (>0 - E, <0 - W)
  WORD speed_low;           // m/h
  WORD course;              // minutes
  short          altitude;  // meters (>0 - above, <0 - below)
  DWORD   dt;               // time_t
  union  {
    BYTE data;
    struct {
      BYTE satelites : 4; // visible satelites
      BYTE valid     : 1; // 1 - valid, 0 - invalid
      BYTE speed_hi  : 2;
      BYTE reserved  : 1;
    } bits;
  } ext;
};
#pragma pack()
