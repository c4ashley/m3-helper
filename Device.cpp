#include "Device.hpp"

Device::Device()
{ };

Device::~Device()
{ };

bool Device::Open()
{
	m_isOpen = true;
	return true;
};

bool Device::Close()
{
	m_isOpen = false;
	return true;
};

void Device::callback([[maybe_unused]]MIDIMessage msg, [[maybe_unused]]uintptr_t dw1, [[maybe_unused]]uintptr_t dw2)
{
};

#ifdef _WIN32
#include "Device.win32.cpp"
#else
#include "Device.unix.cpp"
#endif
