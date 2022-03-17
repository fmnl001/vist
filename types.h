#pragma once

#include <stdint.h>
#include <string>
#include <ostream>

#if !defined(WIN32)
typedef unsigned int DWORD;
typedef unsigned short WORD;
typedef unsigned char BYTE;
#else
#include <windows.h>
typedef unsigned int uint;
#endif

/*
 *
 */
struct MemoryStruct {
  char *memory;
  size_t size;
};

/*
 *
 */
struct ParserStruct {
  int ok;
  size_t tags;
  size_t depth;
  struct MemoryStruct characters;
};
