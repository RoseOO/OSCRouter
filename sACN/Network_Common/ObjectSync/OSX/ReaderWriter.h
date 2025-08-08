/******************************************************************************
//
// MODULE NAME:  ReaderWriter.h
//
// DESCRIPTION:  This module defines reader/writer locks for use in OSX
//
// As this is accessed in platform neutral code, the public API should NOT CHANGE,
// or at least not be different from the public APIs of the other platforms.
//
// This is provided as part of the ObjectSync library.  It requires <pthreads>
// and the tock library to operate correctly.
//
******************************************************************************/
#ifndef _READER_WRITER_H_
#define _READER_WRITER_H_

//This class is the core reader/writer lock.
class CXReadWriteLock
{
public:
	CXReadWriteLock();
	~CXReadWriteLock();

	//returns true if locked, false if timedout or too many locks
	//a timeout < 0 is infinite
	bool ReadLock(int millitimeout = -1);
	void ReadUnlock();  //Clears a read lock

	//returns true if locked, false if timedout or too many locks
	//a timeout < 0 is infinite
	bool WriteLock(int millitimeout = -1); 
	void WriteUnlock();  //Clears the write lock

private:
	CXReadWriteLock& operator=(const CXReadWriteLock&);  //Never implemented

	pthread_rwlock_t m_rwlock;
};

#endif
