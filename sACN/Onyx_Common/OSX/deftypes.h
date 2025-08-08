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

typedef unsigned int	uint;		//An arbitrary unsigned integer
typedef signed char		int1;		//A signed integer 1 byte long
typedef unsigned char	uint1;		//An unsigned integer 1 byte long
typedef signed short	int2;		//A signed integer 2 bytes long
typedef unsigned short	uint2;		//An unsigned integer 2 bytes long
typedef signed long		int4;		//A signed integer 4 bytes long
typedef unsigned long	uint4;		//An unsigned integer 4 bytes long
typedef signed long long	int8;	//A signed integer 8 bytes long
typedef unsigned long long	uint8;	//An unsigned integer 8 bytes long

#endif  /*_DEFTYPES_H_*/
