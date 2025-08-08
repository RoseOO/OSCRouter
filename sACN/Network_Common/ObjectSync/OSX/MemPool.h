/******************************************************************************
//
// MODULE NAME:  MemPool.h
//
// DESCRIPTION:  This module defines a memory pool of fixed size blocks for use in windows
// It is essentially a freelist.
// 
// As this is accessed in platform neutral code, the public API should NOT CHANGE,
// or at least not be different from the public APIs of the other platforms.
// 
//
// This is provided as part of the ObjectSync library
//
******************************************************************************/
#ifndef _MEMPOOL_H_
#define _MEMPOOL_H_

#ifndef _DEFTYPES_H_
#error "#include error: mempool.h needs deftypes.h"
#endif

//This class is the memory pool/freelist
class CMemPool
{
public:
	//The mempool will allocate blocks of blocksize, up to some maximum 
	//block count, or infinite if -1.  
	CMemPool(uint blocksize, int max = -1);
	~CMemPool();

	//Reserves a number of free blocks at once
	void Reserve(uint count);

	//Returns a pointer to the block, or NULL if you hit max or a memory error
	//If force is true, this will always attempt to allocate (given max count)
	void* Alloc(bool force = false);

	//"Free"s a pointer back to the pool
	void Free(void* pblock);

	//Releases the current freelist back to the memory heap
	void ReleaseFreelist();

private:
	const int m_COUNTMAX;  //The max number of blocks
	const uint m_BLOCKSIZE;
	void* m_lock;   //The lock around the pool, as a void* so this can be included on multiple platforms
						 //TODO: USE THE SAME TRICK IN READWRITELOCK??
	int m_curcount;  //The current number of allocated blocks
	uint1 * m_head;  //Points to the head block.  The blocks are blobs of memory, with the first
	                 //n bytes being overwritten with the Next pointer (it is "freed", so we 
	                 //don't care what the data is).

	//To get rid of the annoying assignment operator warning
	//We don't want pool class copying anyway, so we'll leave this unimplemented
	CMemPool& operator=(const CMemPool& pool);

	//Because we don't know what the platform specific lock is, this function is called
	//internally to set up the lock
	bool AssureLock();

};

#endif
