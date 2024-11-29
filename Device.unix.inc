#pragma once

int GetNumSubdevicesForDevice(snd_ctl_t* ctl, int card, int device, bool input);
int GetNumDevicesForCard(int card, bool input);
int GetTotalDeviceCount(bool input);
