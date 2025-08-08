/******************************************************************************
//
// MODULE NAME:  ReaderWriter.h
//
// DESCRIPTION:  This module defines reader/writer locks for use in windows
// 
// As this is accessed in platform neutral code, the public API should NOT CHANGE,
// or at least not be different from the public APIs of the other platforms.
// 
// It should be pretty easy to port this to other platforms, just use the best
// mutual exclusion mechanism for the OS (binary semaphore, whatever),
// and a mechanism for interlocked increment/decrement
//
// This is provided as part of the ObjectSync library
//
******************************************************************************/
#ifndef _READER_WRITER_H_
#define _READER_WRITER_H_

#ifndef _WINDOWS_
#error "#include error: readerwriter.h needs windows.h"
#endif

//This class is the core reader/writer lock.
//It is named thus to not conflict with the Net2 version
class CXReadWriteLock
{
public:
	CXReadWriteLock();
	~CXReadWriteLock();

	//returns true if locked, false if too many locks
	bool ReadLock();
	void ReadUnlock();  //Clears a read lock

	//returns true if locked, false if timedout or too many locks
	//a timeout < 0 is infinite
	bool WriteLock(int millitimeout = -1); 
	void WriteUnlock();  //Clears the write lock

private:
	CXReadWriteLock& operator=(const CXReadWriteLock&);  //Never implemented

	const int m_COUNTMAX;  //The max number of reader or writer locks
	LONG m_readcount;  //The number of readers who have a lock
	CRITICAL_SECTION m_cs;   //The actual lock
};

#endif