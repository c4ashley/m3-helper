#include "OutputDevice.hpp"
#include <alsa/asoundlib.h>
#include <linux/soundcard.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "Device.unix.inc"

struct OutputDevice::ImplType
{
	char Name[32];
	snd_rawmidi_t* Handle;
};

//struct OutputDevice::ImplType
//{
//	UINT Id;
//	MIDIINCAPS Capabilities;
//	HMIDIOUT Handle;
//	void CALLBACK Callback(HMIDIOUT hDevice, UINT msg, DWORD_PTR dwInstance, DWORD_PTR dw1, DWORD_PTR dw2);
//};

const char* OutputDevice::Name() const { return m_impl->Name; };

#ifdef _MSC_VER
#define strncasecmp _strnicmp
#endif

OutputDevice::OutputDevice(ImplType* impl)
	: m_impl(impl) { };

OutputDevice* OutputDevice::GetByName(const char* name)
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
	strncpy(nameIn, name, p - name);
	nameIn[p - name] = 0;

	if ((status = snd_card_next(&card)) < 0)
		return nullptr;

	if (card >= 0)
	{
		while (card >= 0)
		{
			if ((status = snd_card_get_name(card, &cardname)) < 0)
				break;
			if (strcmp(cardname, nameIn) == 0)
				goto GotCard;
			++card;
		}
	}
	fprintf(stderr, "Couldn't find ALSA MIDI Device '%s'\n", name);
	return nullptr;

GotCard:
	ImplType* impl = new ImplType;
	snprintf(impl->Name, 32, "hw:%d,%d,%d", card, device, subdevice);
	impl->Handle = nullptr;
	return new OutputDevice(impl);
};

OutputDevice::~OutputDevice()
{
	Close();
	delete m_impl;
}

bool OutputDevice::Close()
{
	if (m_impl->Handle)
		snd_rawmidi_close(m_impl->Handle);
	m_impl->Handle = 0;
	printf("Closed ALSA output %s\n", Name());
	return true;
};

bool OutputDevice::GetName(int id, char* out)
{
	throw "not implemented";
};

void OutputDevice::StopEnumeration(DeviceEnumerator* i)
{
	if (i->ctl)
		snd_ctl_close((snd_ctl_t*)i->ctl);
	i->ctl = nullptr;
};

bool OutputDevice::GetName(const DeviceEnumerator* i, char* out, size_t cchOut)
{
	if (i->ctl == nullptr)
		return false;

	snd_rawmidi_info_t* info;
	snd_rawmidi_info_alloca(&info);
	snd_rawmidi_info_set_device(info, i->device);
	snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_OUTPUT);
	snd_rawmidi_info_set_subdevice(info, i->subdevice);
	auto name = snd_rawmidi_info_get_subdevice_name(info);
	strncpy(out, name, cchOut);
	return true;
};

bool OutputDevice::EnumerateNext(DeviceEnumerator* i)
{
	if (i->ctl == nullptr)
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
	snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_OUTPUT);
	if (snd_ctl_rawmidi_info((snd_ctl_t*)i->ctl, info) < 0)
		goto NextDevice;
	i->nSubdevices = snd_rawmidi_info_get_subdevices_count(info);
	goto CheckSubdevice;
};

OutputDevice* OutputDevice::GetByID(int id)
{
	throw "not implemented";
};

size_t OutputDevice::Count() { return GetTotalDeviceCount(false); };

bool OutputDevice::Open()
{
	if (m_isOpen)
		return true;

	snd_rawmidi_t* handle;
	if (snd_rawmidi_open(NULL, &handle, m_impl->Name, SND_RAWMIDI_NONBLOCK) < 0)
	{
		fprintf(stderr, "Failed to open ALSA MIDI device '%s'\n", m_impl->Name);
		return false;
	}

	printf("ALSA MIDI Device '%s' opened\n", m_impl->Name);
	m_impl->Handle = handle;
	Device::Open();
	return true;
};

static constexpr int PAD(int x) { return ((x+3)/4)*4; };

void OutputDevice::LongMessage(const void* Buffer, size_t cbBuffer)
{
	if(!m_isOpen)
		return;

	snd_rawmidi_write(m_impl->Handle, Buffer, cbBuffer);
};

#undef SendMessage
void OutputDevice::SendMessage(Message* message)
{
	if(!m_isOpen)
		return;

	static constexpr uint8_t SystemMessageLengths[16] = {1,2,3,2,1,1,1,1,1,1,1,1,1,1,1,1};
	static constexpr uint8_t MessageLengths[7]        = {3,3,3,3,2,2,3};

	if(message->Type() == MessageType::System && message->SubType() == SystemMessageType::SysExStart)
		LongMessage(message->Buffer, message->BufferSize);
	else
	{
		int size = message->Type() == MessageType::System ? SystemMessageLengths[(int)message->SubType()] : MessageLengths[(int)message->Type() - 8];
		snd_rawmidi_write(m_impl->Handle, message, size);
	}
};

//struct CallbackState
//{
//	HMIDIOUT hDevice;
//	UINT msg;
//	DWORD_PTR dwInstance;
//	DWORD_PTR dw1;
//	DWORD_PTR dw2;
//};
//
//void CALLBACK OutputDevice::ImplType::Callback(HMIDIOUT hDevice, UINT msg, DWORD_PTR dwInstance, DWORD_PTR dw1, DWORD_PTR dw2)
//{
//	CallbackState state {
//		.hDevice = hDevice,
//		.msg = msg,
//		.dwInstance = dwInstance,
//		.dw1 = dw1,
//		.dw2 = dw2
//	};
//	Device::GlobalMidiCallback(&state);
//};
