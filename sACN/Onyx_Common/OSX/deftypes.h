/*deftypes.h
  This is the list of standard types used in common code.
  Except for uint, they represent types that are always the same size,
    no matter what platform they are on.
  
  Note that int is also a standard type that is acceptable (although
    not always the same size)

  OSX version follows:
*/
#ifndef _DEFTYPES_H_
#define _DEFTYPES_H_

#include <stdint.h>

typedef unsigned int uint;  //An arbitrary unsigned integer
typedef int8_t int1;        //A signed integer 1 byte long
typedef uint8_t uint1;      //An unsigned integer 1 byte long
typedef int16_t int2;       //A signed integer 2 bytes long
typedef uint16_t uint2;     //An unsigned integer 2 bytes long
typedef int32_t int4;       //A signed integer 4 bytes long
typedef uint32_t uint4;     //An unsigned integer 4 bytes long
typedef int64_t	int8;       //A signed integer 8 bytes long
typedef uint64_t uint8;     //An unsigned integer 8 bytes long

#endif  /*_DEFTYPES_H_*/
