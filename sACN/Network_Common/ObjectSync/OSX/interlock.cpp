
/* interlock.cpp*/
#include <libkern/OSAtomic.h>

int32_t *InterlockedAllocate ( )
{
	int32_t* p = new int32_t;

    if(p)
		*p = 0;
		
    return p;
}


void InterlockedDeallocate ( int32_t *v )
{
    if(v)
		delete v;
}


/*
-- Subtract one, return result
*/
int32_t	InterlockedDecrement ( int32_t *v )
{
	return OSAtomicDecrement32Barrier(v);
}


/*
-- Add one, return result
*/
int32_t InterlockedIncrement ( int32_t *v )
{
	return OSAtomicIncrement32Barrier(v); 
}

/*
-- Add "incr", return initial value
*/
int32_t InterlockedExchangeAdd ( int32_t *v, int32_t incr )
{
	return OSAtomicAdd32Barrier(incr, v) - incr;
}

