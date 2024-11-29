#pragma once
#include "Device.hpp"

//#ifdef _WIN33
//#include "InputDevice.win32.hpp"
//#else
//#include "InputDevice.unix.hpp"
//#endif

struct InputDeviceEnumerator;
class InputDevice : public Device
{
private:
	struct ImplType;
	InputDevice(ImplType* impl);
	ImplType* m_impl;
	std::vector<MIDIEvent> m_callbacks;
public:
	static bool EnumerateNext(DeviceEnumerator*);
	static void StopEnumeration(DeviceEnumerator*);
	static bool GetName(const DeviceEnumerator*, char* out, size_t cchOut);
	static InputDevice* GetByName(const char* name);
	static InputDevice* GetByID(int id);
	static bool GetName(int id, char* out);
	static size_t Count();
	virtual ~InputDevice();
	virtual bool Open() override;
	virtual bool Close() override;
	virtual const char* Name() const override;
	void StartReceiveDump(size_t size/*BufferCallback^ callback*/);
	void AddCallback(MIDIEventHandler callback, void* context = nullptr);
	bool RemoveCallback(MIDIEventHandler callback, void* context);
	bool RemoveCallbacks(MIDIEventHandler callback);
	bool RemoveCallbacks(void* context);
	bool RemoveCallbacks();
private:
	void OnMessageReceived(MIDIEventArgs* e);
protected:
	virtual void callback(MIDIMessage msg, uintptr_t dw1, uintptr_t dw2) override;
};

