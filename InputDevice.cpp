#include "InputDevice.hpp"
#include <list>

#ifdef _MSC_VER
#define strncasecmp _strnicmp
#endif

InputDevice::InputDevice(ImplType* impl)
	: m_impl(impl) { };

void InputDevice::OnMessageReceived(MIDIEventArgs* e)
{
#if 0
	if (enableThreadSync)
	{
		if (locked)
			wait->WaitOne();
		locked = true;
		for each (MIDIEvent var in _callbacks)
		{
			if (var.Key == MIDIMessage::InputData) var.Value(this, e);
		}
		locked = false;
		wait->Set();
	}
	else
#endif
	{
		for (auto& callback : m_callbacks)
		{
			callback.second(callback.first, this, *e);
		}
	}
};

void InputDevice::AddCallback(MIDIEventHandler callback, void* context)
{
	m_callbacks.emplace_back(context, callback);
};

bool InputDevice::RemoveCallback(MIDIEventHandler callback, void* context)
{
	for (int i = m_callbacks.size() - 1; i >= 0; --i)
	{
		const auto& item = m_callbacks[i];
		if (item.first == context && item.second == callback)
		{
			m_callbacks.erase(m_callbacks.cbegin() + i);
			return true;
		}
	}
	return false;
};

bool InputDevice::RemoveCallbacks(void* context)
{
	bool hasDeleted = false;
	for (int i = m_callbacks.size() - 1; i >= 0; --i)
	{
		const auto& item = m_callbacks[i];
		if (item.first == context)
		{
			m_callbacks.erase(m_callbacks.cbegin() + i);
			hasDeleted = true;
		}
	}
	return hasDeleted;
};

bool InputDevice::RemoveCallbacks(MIDIEventHandler callback)
{
	bool hasDeleted = false;
	for (int i = m_callbacks.size() - 1; i >= 0; --i)
	{
		const auto& item = m_callbacks[i];
		if (item.second == callback)
		{
			m_callbacks.erase(m_callbacks.cbegin() + i);
			hasDeleted = true;
		}
	}
	return hasDeleted;
};

bool InputDevice::RemoveCallbacks()
{
	bool hasDeleted = !m_callbacks.empty();
	m_callbacks.clear();
	return hasDeleted;
};

#ifdef _WIN32
#include "InputDevice.win32.cpp"
#else
#include "InputDevice.unix.cpp"
#endif
