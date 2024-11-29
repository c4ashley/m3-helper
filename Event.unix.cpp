#include "Event.hpp"
#include <fcntl.h>
#include <sys/stat.h>
#include <limits.h>
#include <cstdio>

Event::Event(const char* name)
{
	char newName[NAME_MAX-4];
	snprintf(newName, NAME_MAX-4, "/%s", name);
	m_handle = sem_open(newName, O_CREAT | O_EXCL, O_RDWR, 0u);
};

Event::~Event()
{
	sem_close(m_handle);
	m_handle = nullptr;
};

void Event::Wait()
{
	sem_wait(m_handle);
};

void Event::Signal()
{
	sem_post(m_handle);
};


