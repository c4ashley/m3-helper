#include "InputDevice.hpp"
#include <alsa/asoundlib.h>
#include <linux/soundcard.h>

int GetNumSubdevicesForDevice(snd_ctl_t* ctl, int card, int device, bool input)
{
	snd_rawmidi_info_t *info;

	snd_rawmidi_info_alloca(&info);
	snd_rawmidi_info_set_device(info, device);

	snd_rawmidi_info_set_stream(info, input ? SND_RAWMIDI_STREAM_INPUT : SND_RAWMIDI_STREAM_OUTPUT);

	if (snd_ctl_rawmidi_info(ctl, info) < 0)
		return 0;

	return snd_rawmidi_info_get_subdevices_count(info);
};

int GetNumDevicesForCard(int card, bool input)
{
	int nDevices = 0;
	snd_ctl_t* ctl;
	char name[32];
	int device = -1;

	snprintf(name, 32, "hw:%d", card);
	if (snd_ctl_open(&ctl, name, 0) < 0)
		return nDevices;

	while (snd_ctl_rawmidi_next_device(ctl, &device) >= 0 && device >= 0)
		nDevices += GetNumSubdevicesForDevice(ctl, card, device, input);

	snd_ctl_close(ctl);

	return nDevices;
};

int GetTotalDeviceCount(bool input)
{
	int card = -1;
	int nCards = 0;
	int nTotalDevices = 0;
	while (snd_card_next(&card) >= 0 && card >= 0)
	{
		++nCards;
		nTotalDevices += GetNumDevicesForCard(card, input);
	}
	return nTotalDevices;
};

