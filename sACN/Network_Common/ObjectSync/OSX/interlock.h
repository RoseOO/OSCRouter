
/* interlock.h */

/*
-- Interlocked operations on words in memory.
-- These functions provide thread-safe, synchronized access to memory.
*/

#ifndef _INTERLOCK_H_
#define _INTERLOCK_H_

/*
-- Allocate memory for use by other functions.
-- Accounts for any size and alignment requirements.
*/
int32_t *InterlockedAllocate ( );
void InterlockedDeallocate ( int32_t *v );

/*
-- Decrement the value in memory by one, return the result.
*/
int32_t InterlockedDecrement ( int32_t *v );
/*
-- Increment the value in memory by one, return the result.
*/
int32_t InterlockedIncrement ( int32_t *v );
/*
-- Add "incr" to the value in memory, return the initial value.
*/
int32_t InterlockedExchangeAdd ( int32_t *v, int32_t incr );


#endif  /* _INTERLOCK_H_ */
