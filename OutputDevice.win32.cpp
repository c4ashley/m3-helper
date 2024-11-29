#include "OutputDevice.hpp"
#include <windows.h>

struct OutputDevice::ImplType
{
	UINT Id;
	MIDIINCAPS Capabilities;
	HMIDIOUT Handle;
	void CALLBACK Callback(HMIDIOUT hDevice, UINT msg, DWORD_PTR dwInstance, DWORD_PTR dw1, DWORD_PTR dw2);
};

static MMRESULT Assert(MMRESULT result, const char* message = nullptr);
const char* OutputDevice::Name() const { return m_impl->Capabilities.szPname; };

#ifdef _MSC_VER
#define strncasecmp _strnicmp
#endif

OutputDevice::OutputDevice(ImplType* impl)
	: m_impl(impl) { };

bool OutputDevice::GetName(int id, char* out)
{
	if ((UINT)id >= ::midiOutGetNumDevs())
		return false;
	MIDIOUTCAPS caps {};
	if (Assert(::midiOutGetDevCaps(id, &caps, sizeof(MIDIOUTCAPS)), "Getting output device capabilities"))
		return false;
	strcpy(out, caps.szPname);
	return true;
};

OutputDevice* OutputDevice::GetByName(const char* name)
{
	auto max = ::midiOutGetNumDevs();
	MIDIOUTCAPS caps {};
	for (UINT i = 0; i < max; ++i)
	{
		::midiOutGetDevCaps(i, &caps, sizeof(MIDIOUTCAPS));
		if (strncasecmp(name, caps.szPname, sizeof(caps.szPname)) == 0)
		{
			auto impl = new ImplType;
			impl->Id = i;
			memcpy(&impl->Capabilities, &caps, sizeof(caps));
			return new OutputDevice(impl);
		}
	}
	return nullptr;
};

OutputDevice* OutputDevice::GetByID(int id)
{
	if ((UINT)id >= ::midiOutGetNumDevs())
		return nullptr;
	MIDIOUTCAPS caps {};
	::midiOutGetDevCaps(id, &caps, sizeof(MIDIOUTCAPS));
	ImplType* impl = new ImplType;
	impl->Id = id;
	memcpy(&impl->Capabilities, &caps, sizeof(caps));
	return new OutputDevice(impl);
};

OutputDevice::~OutputDevice()
{
	Close();
	delete m_impl;
};

size_t OutputDevice::Count() { return ::midiOutGetNumDevs(); };

bool OutputDevice::Open()
{
	if (m_isOpen)
		return true;

	HMIDIOUT handle;

	if (Assert(::midiOutOpen(&handle, m_impl->Id, (DWORD_PTR)GlobalMidiCallback, (DWORD_PTR)(void*)this, CALLBACK_FUNCTION), "Opening output device"))
		goto Error;

	m_impl->Handle = handle;
	m_isOpen = true;

	if (Assert(::midiOutReset(handle), "Starting output device"))
		goto Error;

	return true;

Error:
	return false;
};

MMRESULT Assert(MMRESULT result, const char* message)
{
	if (result)
	{
		TCHAR buffer[MAXERRORLENGTH];
		midiOutGetErrorText(result, buffer, MAXERRORLENGTH);
		if (message == nullptr)
			message = "Output device error";
		fprintf(stderr, "%s: %s\n", message, buffer);
	}
	return result;
};

bool OutputDevice::Close()
{
	if (!m_isOpen)
		return true;
	m_isOpen = false;
	if (m_impl->Handle)
		midiOutClose(m_impl->Handle);
	m_impl->Handle = nullptr;
	return true;
};

static constexpr int PAD(int x) { return ((x+3)/4)*4; };

void OutputDevice::LongMessage(const void* Buffer, size_t cbBuffer)
{
	LPMIDIHDR header = (LPMIDIHDR)malloc(sizeof(MIDIHDR)+PAD(cbBuffer));
	if (header == nullptr)
		throw std::bad_alloc();

	memset(header, 0, sizeof(MIDIHDR)+PAD(cbBuffer));
	header->lpData = (LPSTR) (header+1);
	header->dwBufferLength = cbBuffer;
	memcpy(header->lpData, Buffer, cbBuffer);
	
	if(!m_isOpen)
		Open();
	Assert(::midiOutPrepareHeader(m_impl->Handle, header, sizeof(MIDIHDR)), "Preparing SysEx buffer");
	Assert(::midiOutLongMsg(m_impl->Handle, header, sizeof(MIDIHDR)), "Sending SysEx data");
};

#undef SendMessage
void OutputDevice::SendMessage(Message* message)
{
	if(!m_isOpen)
		Open();

	if(message->Type() == MessageType::System && message->SubType() == SystemMessageType::SysExStart)
	{
		MIDIHDR midi = MIDIHDR();
		auto buffer = message->Buffer;
		midi.dwBufferLength = message->BufferSize;
		midi.lpData = (LPSTR)malloc(midi.dwBufferLength);
		if (midi.lpData==NULL)
			throw std::bad_alloc();

		memcpy(midi.lpData, buffer, midi.dwBufferLength);
		midi.dwBytesRecorded = midi.dwBufferLength;
		midi.dwFlags  = 0;
		midi.dwOffset = 0;
		midi.dwUser   = 0;
		Assert(::midiOutPrepareHeader(m_impl->Handle, &midi, sizeof(midi)), "Preparing SysEx buffer");
		Assert(::midiOutLongMsg(m_impl->Handle, &midi, sizeof(midi)), "Sending SysEx data");
		free(midi.lpData);
	}
	else
	{
		Assert(::midiOutShortMsg(m_impl->Handle,
		       ((DWORD)message->Byte2<<16)|
		       ((DWORD)message->Byte1<<8)|
		       ((DWORD)message->StatusH<<4)|
		       ((DWORD)message->StatusL)), "Sending MIDI message");
	}
};

struct CallbackState
{
	HMIDIOUT hDevice;
	UINT msg;
	DWORD_PTR dwInstance;
	DWORD_PTR dw1;
	DWORD_PTR dw2;
};

void CALLBACK OutputDevice::ImplType::Callback(HMIDIOUT hDevice, UINT msg, DWORD_PTR dwInstance, DWORD_PTR dw1, DWORD_PTR dw2)
{
	CallbackState state {
		.hDevice = hDevice,
		.msg = msg,
		.dwInstance = dwInstance,
		.dw1 = dw1,
		.dw2 = dw2
	};
	Device::GlobalMidiCallback(&state);
};
