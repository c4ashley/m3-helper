#include "Event.hpp"

Event::Event(const char* name)
	: m_handle(CreateEvent(NULL, FALSE, FALSE, name))
{ };

Event::~Event()
{
	CloseHandle(m_handle);
	m_handle = INVALID_HANDLE_VALUE;
};

void Event::Wait()
{
	WaitForSingleObject(m_handle, INFINITE);
};

void Event::Signal()
{
	SetEvent(m_handle);
};


