/* tock.cpp
  
   OSX implementation of the platforms-specific part of the tock library.
*/


#include <mach/mach_time.h>

#include "deftypes.h"
#include "../tock.h"

//This uses mach time, which is the ratio of number of nanoseconds to number of ticks.
//We want a simple scale that converts the ticks to milliseconds
long double scale = 0;
int tock_resolution = 0;  //A simplification of scale, purely for Tock_GetRes's use. I'd be very surprised if it's > 1 on any OS X

//Initializes the tock layer.  Only needs to be called once per application
bool Tock_StartLib()
{
	//Create the scale factor
	mach_timebase_info_data_t t;
	mach_timebase_info(&t);
	
	scale = ((long double) t.numer) / (((long double) t.denom) * ((long double) 1000000));
		
	// But make sure that the tock resolution is between 1-10ms
	if(scale <= 1)
		tock_resolution = 1;
	else
		tock_resolution = scale;
	
	return tock_resolution <= 10;
}

//Gets a tock representing the current time
tock Tock_GetTock()
{
	uint64_t time = mach_absolute_time();
	return time * scale;  //This should scale down to the point where it safely truncates into a uint4.
}

//Shuts down the tock layer.
void Tock_StopLib()
{
}

//Returns the number of ms between tocks
int Tock_GetRes()
{
	return tock_resolution;
}
