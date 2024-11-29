#pragma once
struct DeviceEnumerator
{
	int card, device, subdevice, nSubdevices;
	void* ctl;
};
