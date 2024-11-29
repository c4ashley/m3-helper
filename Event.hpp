#pragma once
#ifdef _WIN32
#include "Event.win32.hpp"
#else
#include "Event.unix.hpp"
#endif
class Event
{
private:
	EventImpl m_handle;
public:
	Event(const char* name);
	~Event();
	void Wait();
	void Signal();
};
