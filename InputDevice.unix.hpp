#pragma once
struct InputDeviceEnumerator
{
	int card, device, subdevice, nSubdevices;
	snd_ctl_t* ctl;
};
