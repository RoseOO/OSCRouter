/*defpack.h
  This file defines the big and little endian packing/unpacking utilities that
  common code uses.

  All functions follow the form PackXY and UpackXY, 
  where X denotes endianness (B or L, for big and little, respectively),
  and Y denotes the number of bytes (1, 2, 4, 8)

  OSX version follows, which is intended for the Universal binary, so it supports
  both platforms.  Yes, it is a little inefficient on the PPC side, since for the 
  typical use of this (big endian) a simple cast and assign would be slightly faster.
  But ppc will eventually go away, intel already has the processing overhead, and the
  same header can be linked into all Universal code.
  
*/
#ifndef	_DEFPACK_H_
#define	_DEFPACK_H_

//defpack requires the default types
#ifndef _DEFTYPES_H_
#error "#include error: defpack.h requires deftypes.h"
#endif

//Declarations
void  PackB1 (uint1* ptr, uint1 val);	//Packs a uint1 to a known big endian buffer
uint1 UpackB1(const uint1* ptr);		//Unpacks a uint1 from a known big endian buffer
void  PackL1 (uint1* ptr, uint1 val);	//Packs a uint1 to a known little endian buffer
uint1 UpackL1(const uint1* ptr);		//Unpacks a uint1 from a known little endian buffer
void  PackB2 (uint1* ptr, uint2 val);	//Packs a uint2 to a known big endian buffer
uint2 UpackB2(const uint1* ptr);		//Unpacks a uint2 from a known big endian buffer
void  PackL2 (uint1* ptr, uint2 val);	//Packs a uint2 to a known little endian buffer
uint2 UpackL2(const uint1* ptr);		//Unpacks a uint2 from a known little endian buffer
void  PackB4 (uint1* ptr, uint4 val);	//Packs a uint4 to a known big endian buffer
uint4 UpackB4(const uint1* ptr);		//Unpacks a uint4 from a known big endian buffer
void  PackL4 (uint1* ptr, uint4 val);	//Packs a uint4 to a known little endian buffer
uint4 UpackL4(const uint1* ptr);		//Unpacks a uint4 from a known little endian buffer
void  PackB8 (uint1* ptr, uint8 val);	//Packs a uint8 to a known big endian buffer
uint8 UpackB8(const uint1* ptr);		//Unpacks a uint4 from a known big endian buffer
void  PackL8 (uint1* ptr, uint8 val);	//Packs a uint8 to a known little endian buffer
uint8 UpackL8(const uint1* ptr);		//Unpacks a uint8 from a known little endian buffer

//---------------------------------------------------------------------------------
//implementation

//Packs a uint1 to a known big endian buffer
inline void	PackB1(uint1* ptr, uint1 val)
{
	*ptr = val;
}

//Unpacks a uint1 from a known big endian buffer
inline uint1 UpackB1(const uint1* ptr)
{
	return *ptr;
}

//Packs a uint1 to a known little endian buffer
inline void	PackL1(uint1* ptr, uint1 val)
{
	*ptr = val;
}

//Unpacks a uint1 from a known little endian buffer
inline uint1 UpackL1(const uint1* ptr)
{
	return *ptr;
}

//Packs a uint2 to a known big endian buffer
inline void PackB2(uint1* ptr, uint2 val)
{
	ptr[1] = (uint1)(val);
	ptr[0] = (uint1)(val >> 8);
}

//Unpacks a uint2 from a known big endian buffer
inline uint2 UpackB2(const uint1* ptr)
{
	return (uint2)(ptr[1] | ptr[0] << 8);
}

//Packs a uint2 to a known little endian buffer
inline void PackL2(uint1* ptr, uint2 val)
{
	ptr[0] = (uint1)val;
	ptr[1] = (uint1)(val >> 8);
}

//Unpacks a uint2 from a known little endian buffer
inline uint2 UpackL2(const uint1* ptr)
{
	return (uint2)(ptr[0] | ptr[1] << 8);
}

//Packs a uint4 to a known big endian buffer
inline void PackB4(uint1* ptr, uint4 val)
{
	ptr[3] = (uint1)val;
	ptr[2] = (uint1)(val >> 8);
	ptr[1] = (uint1)(val >> 16);
	ptr[0] = (uint1)(val >> 24);
}

//Unpacks a uint4 from a known big endian buffer
inline uint4 UpackB4(const uint1* ptr)
{
	return (uint4)(ptr[3] | (ptr[2] << 8) | (ptr[1] << 16) | (ptr[0] << 24));
}

//Packs a uint4 to a known little endian buffer
inline void PackL4(uint1* ptr, uint4 val)
{
	ptr[0] = (uint1)val;
	ptr[1] = (uint1)(val >> 8);
	ptr[2] = (uint1)(val >> 16);
	ptr[3] = (uint1)(val >> 24);
}

//Unpacks a uint4 from a known little endian buffer
inline uint4 UpackL4(const uint1* ptr)
{
	return (uint4)(ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24));
}

//Packs a uint8 to a known big endian buffer
inline void PackB8(uint1* ptr, uint8 val)
{
	ptr[7] = (uint1)val;
	ptr[6] = (uint1)(val >> 8);
	ptr[5] = (uint1)(val >> 16);
	ptr[4] = (uint1)(val >> 24);
	ptr[3] = (uint1)(val >> 32);
	ptr[2] = (uint1)(val >> 40);
	ptr[1] = (uint1)(val >> 48);
	ptr[0] = (uint1)(val >> 56);
}

//Unpacks a uint4 from a known big endian buffer
inline uint8 UpackB8(const uint1* ptr)
{
	return ((uint8)ptr[7]) | (((uint8)ptr[6]) << 8) | (((uint8)ptr[5]) << 16) |
		   (((uint8)ptr[4]) << 24) | (((uint8)ptr[3]) << 32) | 
		   (((uint8)ptr[2]) << 40) | (((uint8)ptr[1]) << 48) | 
		   (((uint8)ptr[0]) << 56);
}

//Packs a uint8 to a known little endian buffer
inline void PackL8(uint1* ptr, uint8 val)
{
	ptr[0] = (uint1)val;
	ptr[1] = (uint1)(val >> 8);
	ptr[2] = (uint1)(val >> 16);
	ptr[3] = (uint1)(val >> 24);
	ptr[4] = (uint1)(val >> 32);
	ptr[5] = (uint1)(val >> 40);
	ptr[6] = (uint1)(val >> 48);
	ptr[7] = (uint1)(val >> 56);
}

//Unpacks a uint8 from a known little endian buffer
inline uint8 UpackL8(const uint1* ptr)
{
	return ((uint8)ptr[0]) | (((uint8)ptr[1]) << 8) | (((uint8)ptr[2]) << 16) |
		   (((uint8)ptr[3]) << 24) | (((uint8)ptr[4]) << 32) | 
		   (((uint8)ptr[5]) << 40) | (((uint8)ptr[6]) << 48) | 
		   (((uint8)ptr[7]) << 56);
}


#endif	/*_DEFPACK_H_*/
