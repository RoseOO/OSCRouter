/*  tock.h

  Provides a standard definition of a tock and a ttimer.

  A tock is the number of milliseconds since a platform-specific date, usually
  system startup. Tocks are never used directly, rather the difference between
  two tocks  (latest - previous) are used to determine the passage of time.
  It is assumed that tocks always move forward.
  
  While tock comparisons assume a 1ms granularity, your system may not actually
  update that quickly.  Some platforms update their system tick every 10ms.
  You can call Tock_GetRes to determine how many ms may occur between each
  tock.
  
  Tock comparisons correctly handle clock rollover.

  A ttimer is a simple abstraction for typical timer usage, which is
  setting a number of milliseconds to time out, and then telling whether
  or not the ttimer has expired.
*/

#ifndef _TOCK_H_
#define _TOCK_H_

//tock requires the default types
#ifndef _DEFTYPES_H_
#error "#include error: tock.h requires deftypes.h"
#endif

class tock;
class ttimer;

//These functions comprise the tock API

//Initializes the tock layer.  Only needs to be called once per application,
//but can be called multiple times as long as there is an equal number of
//Tock_StopLib() calls.
bool Tock_StartLib();  
tock Tock_GetTock();   //Gets a tock representing the current time
void Tock_StopLib();   //Shuts down the tock layer.
//Returns the number of milliseconds that can occur between tocks, depending
//on your platform.  Even if the system resolution is 10ms, tocks are still
//compared as if they are 1ms apart.
int Tock_GetRes(); 

//This is the actual tock 
class tock
{
public:
	//construction and copying
	tock();
	tock(uint4 ms);
	tock(const tock& t);
	tock& operator=(const tock& t);

	//Returns the number of milliseconds that this tock represents
	uint4 Getms();  

	//Used sparingly, but sets the number of milliseconds that this tock represents
	void Setms(uint4 ms);

protected:
	int4 v;  //Signed, so the wraparound calculations will work
	
	friend bool operator>(const tock& t1, const tock& t2);
	friend bool operator>=(const tock& t1, const tock& t2);
	friend bool operator==(const tock& t1, const tock& t2);
	friend bool operator!=(const tock& t1, const tock& t2);
	friend bool operator<(const tock& t1, const tock& t2);
	friend bool operator<=(const tock& t1, const tock& t2);
	friend uint4 operator-(const tock& t1, const tock& t2);
};

//The class used for simple expiration tracking
class ttimer
{
public:
	//construction/setup
	ttimer();				//Will immediately time out if timeout isn't set
	ttimer(int4 ms);	//The number of milliseconds before the timer will time out
	void SetInterval(int4 ms);	//Sets a new timeout interval (in ms) and resets the timer
	int4 GetInterval();			//Returns the current timeout interval (in ms)

	void Reset();	//Resets the timer, using the current timeout interval
	bool Expired();  //Returns true if the timer has expired.
					 //Call Reset() to use this timer again for a new interval.
	int4 How_Expired();  //While normally you want to just call expired, this gives you the
	                     //ms from when the timer was last reset
protected:
	int4 interval;
	tock tockout;
};


//---------------------------------------------------------------------------------
//	Implementation

/*ttimer implementation*/
inline ttimer::ttimer():interval(0) {Reset();}
inline ttimer::ttimer(int4 ms):interval(ms) {Reset();}
inline void ttimer::SetInterval(int4 ms) {interval = ms; Reset();}
inline int4 ttimer::GetInterval() {return interval;}
inline void ttimer::Reset() {tockout.Setms(Tock_GetTock().Getms() + interval);}
inline bool ttimer::Expired() {return Tock_GetTock() > tockout;}
inline int4 ttimer::How_Expired() {return (Tock_GetTock() - tockout) + interval;}

/*tock implementation*/
inline tock::tock():v(0) {}
inline tock::tock(uint4 ms):v(ms) {};
inline tock::tock(const tock& t) {v = t.v;}
inline tock& tock::operator=(const tock& t) {v = t.v; return *this;}

inline uint4 tock::Getms() {return v;}
inline void tock::Setms(uint4 ms) {v = ms;}
	
inline bool operator>(const tock& t1, const tock& t2)  {return t1.v - t2.v > 0;}
inline bool operator>=(const tock& t1, const tock& t2) {return t1.v - t2.v >= 0;}
inline bool operator==(const tock& t1, const tock& t2) {return t1.v - t2.v == 0;}
inline bool operator!=(const tock& t1, const tock& t2) {return t1.v - t2.v != 0;}
inline bool operator<(const tock& t1, const tock& t2)  {return t2.v - t1.v > 0;}
inline bool operator<=(const tock& t1, const tock& t2) {return t2.v - t1.v >= 0;}
inline uint4 operator-(const tock& t1, const tock& t2) {return t1.v - t2.v;}


#endif /*_TOCK_H*/
