#pragma once
#include "Device.hpp"

class OutputDevice : public Device
{
private:
	struct ImplType;
	ImplType* m_impl;
	OutputDevice(ImplType* impl);
public:
	static bool EnumerateNext(DeviceEnumerator*);
	static void StopEnumeration(DeviceEnumerator*);
	static bool GetName(const DeviceEnumerator*, char* out, size_t cchOut);
	static OutputDevice* GetByName(const char* name);
	static OutputDevice* GetByID(int id);
	static bool GetName(int id, char* out);
	static size_t Count();
	virtual ~OutputDevice();
	virtual bool Open() override;
	virtual bool Close() override;
	const char* Name() const override;

	void LongMessage(const void* Buffer, size_t cbBuffer);
	void SendMessage(Message* message);
protected:
	virtual void callback(MIDIMessage msg, uintptr_t dw1, uintptr_t dw2) override;
};

