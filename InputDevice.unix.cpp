#include "InputDevice.hpp"
#include <alsa/asoundlib.h>
#include <linux/soundcard.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "Device.unix.inc"

struct InputDevice::ImplType
{
	char Name[32];
	snd_rawmidi_t* Handle;
	int Card, Device, Subdevice;
};

InputDevice::~InputDevice()
{
	Close();
	delete m_impl;
};

void InputDevice::StopEnumeration(DeviceEnumerator* i)
{
	if (i->ctl)
		snd_ctl_close((snd_ctl_t*)i->ctl);
	i->ctl = nullptr;
};

bool InputDevice::GetName(const DeviceEnumerator* i, char* out, size_t cchOut)
{
	if (i->ctl == nullptr)
		return false;

	snd_rawmidi_info_t* info;
	snd_rawmidi_info_alloca(&info);
	snd_rawmidi_info_set_device(info, i->device);
	snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_INPUT);
	snd_rawmidi_info_set_subdevice(info, i->subdevice);
	auto name = snd_rawmidi_info_get_subdevice_name(info);
	strncpy(out, name, cchOut);
	return true;
};

bool InputDevice::EnumerateNext(DeviceEnumerator* i)
{
	if (i->ctl == nullptr && i->card <= 0)
	{
		i->card = -1;
		goto NextCard;
	}

	++i->subdevice;

CheckSubdevice:
	if (i->subdevice >= i->nSubdevices)
		goto NextDevice;

	return true;

NextCard:
	char name[32];
	if (i->ctl)
		snd_ctl_close((snd_ctl_t*)i->ctl);
	i->ctl = nullptr;
	if (snd_card_next(&i->card) <= 0 || i->card < 0)
		return false;
	snprintf(name, 32, "hw:%d", i->card);
	if (snd_ctl_open((snd_ctl_t**)&i->ctl, name, 0) < 0)
		goto NextCard;

	i->subdevice = 0;
	i->nSubdevices = 0;
	i->device = -1;
	goto NextDevice;

NextDevice:
	i->subdevice = 0;
	if (snd_ctl_rawmidi_next_device((snd_ctl_t*)i->ctl, &i->device) < 0 || i->device < 0)
		goto NextCard;
	snd_rawmidi_info_t* info;
	snd_rawmidi_info_alloca(&info);
	snd_rawmidi_info_set_device(info, i->device);
	snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_INPUT);
	if (snd_ctl_rawmidi_info((snd_ctl_t*)i->ctl, info) < 0)
		goto NextDevice;
	i->nSubdevices = snd_rawmidi_info_get_subdevices_count(info);
	goto CheckSubdevice;
};

static void list_ports(void)
{
	snd_seq_client_info_t *cinfo;
	snd_seq_port_info_t *pinfo;

	snd_seq_client_info_alloca(&cinfo);
	snd_seq_port_info_alloca(&pinfo);

	puts(" Port    Client name                      Port name");

	snd_seq_client_info_set_client(cinfo, -1);
	while (snd_seq_query_next_client(seq, cinfo) >= 0)
	{
		int client = snd_seq_client_info_get_client(cinfo);

		snd_seq_port_info_set_client(pinfo, client);
		snd_seq_port_info_set_port(pinfo, -1);
		while (snd_seq_query_next_port(seq, pinfo) >= 0)
		{
			/* port must understand MIDI messages */
			if (!(snd_seq_port_info_get_type(pinfo)
			      & SND_SEQ_PORT_TYPE_MIDI_GENERIC))
				continue;
			/* we need both WRITE and SUBS_WRITE */
			if ((snd_seq_port_info_get_capability(pinfo)
			     & (SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE))
			    != (SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE))
				continue;
			printf("%3d:%-3d  %-32.32s %s\n",
			       snd_seq_port_info_get_client(pinfo),
			       snd_seq_port_info_get_port(pinfo),
			       snd_seq_client_info_get_name(cinfo),
			       snd_seq_port_info_get_name(pinfo));
		}
	}
};

InputDevice* InputDevice::GetByName(const char* name)
{
	// cardname:devicenum:subdevicenum
	int status;
	int card = -1;
	char* cardname;
	int device;
	int subdevice = 0;
	char nameIn[32];
	char nameOut[32];
	const char* p;

	p = name + strlen(name);
	do { --p; } while (p >= name && *p != ':');
	subdevice = atoi(p + 1);
	do { --p; } while (p >= name && *p != ':');
	device = atoi(p + 1);
	if (p > (name + 31)) p = name + 31;
	strncpy(nameIn, name, p-name);
	nameIn[p - name] = '\0';

	if ((status = snd_card_next(&card)) < 0)
		return nullptr;

	while (card >= 0)
	{
		if ((status = snd_card_get_name(card, &cardname)) < 0)
			break; 
		if (strcmp(cardname, nameIn) == 0)
			goto GotCard;
		++card;
	}
	fprintf(stderr, "Couldn't find ALSA MIDI device '%s'\n", name);
	return nullptr;

GotCard:
	sprintf(nameOut, "hw:%d,%d,%d", card, device, subdevice);
	ImplType* impl = new ImplType;
	impl->Handle = nullptr;
	impl->Card = card;
	impl->Device = device;
	impl->Subdevice = subdevice;
	memcpy(impl->Name, nameOut, sizeof(nameOut));
	return new InputDevice(impl);
};

bool InputDevice::Open()
{
	if (m_isOpen)
		return true;

	snd_rawmidi_t* handle;
	if (snd_rawmidi_open(&handle, NULL, m_impl->Name, 0) < 0)
	{
		fprintf(stderr, "Failed to open ALSA MIDI device '%s'\n", m_impl->Name);
		return false;
	}

	m_impl->Handle = handle;
	Device::Open();
	return true;
};

bool InputDevice::Close()
{
	if (!m_isOpen)
		return true;

	if (snd_rawmidi_close(m_impl->Handle) < 0)
	{
		fprintf(stderr, "Failed to close ALSA MIDI device '%s'\n", m_impl->Name);
		return false;
	}
	m_impl->Handle = nullptr;
	Device::Close();
	return true;
};

InputDevice* InputDevice::GetByID(int id)
{
	throw "not implemented";
};

size_t InputDevice::Count()
{
	return GetTotalDeviceCount(true);
};

const char* InputDevice::Name() const
{
	return m_impl->Name;
};

bool InputDevice::GetName(int id, char* out)
{
	throw "not implemented";
};

void InputDevice::StartReceiveDump(size_t size/*BufferCallback^ callback*/)
{
	if (!m_isOpen)
		return;

#if 0
	LPMIDIHDR header = (LPMIDIHDR)malloc(sizeof(MIDIHDR)+size);
	if (header == nullptr)
		throw std::bad_alloc();

	header->dwBufferLength  =256;
	header->dwBytesRecorded =0;
	header->dwFlags         =0;
	header->dwOffset        =0;
	header->lpData          = (LPSTR)(header+1);
	m_impl->Headers.push_back(header);
	Assert(::midiInPrepareHeader(m_impl->Handle, header, sizeof(MIDIHDR)), "Preparing SysEx RX Buffer");
	Assert(::midiInAddBuffer(m_impl->Handle, header, sizeof(MIDIHDR)), "Adding SysEx RX buffer to device");
#endif
	throw "not implemented";
};

void InputDevice::callback(MIDIMessage msg, uintptr_t dw1, uintptr_t dw2)
{
};
