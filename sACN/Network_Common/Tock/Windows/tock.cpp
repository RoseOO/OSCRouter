/* tock.cpp
  
   Implementation of the platforms-specific part of the tock library.
*/

#include "windows.h"
#include "Mmsystem.h"
#include "deftypes.h"
#include "..\tock.h"

//Initializes the tock layer.  Only needs to be called once per application
bool Tock_StartLib()
{
	return TIMERR_NOERROR == timeBeginPeriod(1);
}

//Gets a tock representing the current time
tock Tock_GetTock()
{
	return tock(timeGetTime());
}

//Shuts down the tock layer.
void Tock_StopLib()
{
	timeEndPeriod(1);
}

//Returns the resolution -- in this case 1ms
int Tock_GetRes()
{
   return 1;
}
