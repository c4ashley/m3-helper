#include "InputDevice.hpp"
#include <windows.h>

MMRESULT Assert(MMRESULT result, const char* message);

struct InputDevice::ImplType
{
	public:
		UINT Id;
		MIDIINCAPS Capabilities;
		HMIDIIN Handle;
		std::list<LPMIDIHDR> Headers;
		static void CALLBACK Callback(HMIDIIN hDevice, UINT msg, DWORD_PTR dwInstance, DWORD_PTR dw1, DWORD_PTR dw2);
};

InputDevice* InputDevice::GetByName(const char* name)
{
	UINT max = ::midiInGetNumDevs();
	MIDIINCAPS caps {};
	for (UINT i = 0; i < max; ++i)
	{
		::midiInGetDevCaps(i, &caps, sizeof(MIDIINCAPS));
		if (strncasecmp(name, caps.szPname, sizeof(caps.szPname)) == 0)
		{
			return new InputDevice(new ImplType { .Id = i, .Capabilities = caps });
		}
	}
	return nullptr;
};

InputDevice* InputDevice::GetByID(int id)
{
	if ((UINT)id >= ::midiInGetNumDevs())
		return nullptr;
	MIDIINCAPS caps {};
	::midiInGetDevCaps((UINT)id, &caps, sizeof(MIDIINCAPS));
	ImplType* impl = new ImplType;
	impl->Id = id;
	impl->Capabilities = caps;
	return new InputDevice(impl);
};

const char* InputDevice::Name() const { return m_impl->Capabilities.szPname; };

bool InputDevice::GetName(int id, char* out)
{
	if ((UINT)id >= ::midiInGetNumDevs())
		return false;
	MIDIINCAPS caps {};
	if (Assert(::midiInGetDevCaps(id, &caps, sizeof(MIDIINCAPS)), "Getting input device capabilities"))
		return false;
	strcpy(out, caps.szPname);
	return true;
};

void InputDevice::callback(MIDIMessage msg, uintptr_t dw1, uintptr_t dw2)
{
	LPMIDIHDR pHdr;
	byte* buffer;
	switch (msg)
	{
		case MIDIMessage::InputLongData:
		{
			pHdr = (LPMIDIHDR)dw1;
			if (pHdr->dwBytesRecorded == 0)
			{
				m_impl->Headers.erase(std::find(m_impl->Headers.cbegin(), m_impl->Headers.cend(), (LPMIDIHDR)dw1));
				Assert(midiInUnprepareHeader(m_impl->Handle, (LPMIDIHDR)dw1, sizeof(MIDIHDR)), "Releasing Sysex Header");
				free(((LPMIDIHDR)dw1));
				return;
			}

			buffer = new byte[pHdr->dwBytesRecorded];
			memcpy(buffer, pHdr->lpData, pHdr->dwBytesRecorded);
			MIDIEventArgs e(Message(buffer, pHdr->dwBytesRecorded));
			try
			{
	#if 0
				if (enableThreadSync)
				{
					if (locked)
						wait->WaitOne();
					locked = true;
					// getting pretty shitted off that thread synchronisation is so hard here. Not using enumerators
					// anymore. If the collection changes, deal with it.
					for (int i = 0; i < _callbacks->Count; i++)
					{
						try
						{
							auto var = _callbacks[i];
							if (var.Key == msg) var.Value(this, e);
						}
						catch (Exception^) { break; }
					}
					locked = false;
					wait->Set();
				}
				else
	#endif
				{
					for (const auto& callback : m_callbacks)
					{
						callback.second(callback.first, this, e);
					}
				}

			}
			catch (...)
			{
				throw;
			}

			int size = ((LPMIDIHDR)dw1)->dwBufferLength;
			m_impl->Headers.erase(std::find(m_impl->Headers.cbegin(), m_impl->Headers.cend(), (LPMIDIHDR)dw1));
			Assert(::midiInUnprepareHeader(m_impl->Handle, (LPMIDIHDR)dw1, sizeof(MIDIHDR)), "Releasing sysex header");
			free((LPMIDIHDR)dw1);
			StartReceiveDump(size);

		} break;
		case MIDIMessage::InputData:
		{
			pHdr = NULL;
			buffer = nullptr;
			MIDIEventArgs args = MIDIEventArgs(Message(dw1));
			//default:
			this->OnMessageReceived(&args);
		} break;
	}
};

InputDevice::~InputDevice()
{
	Close();
	delete m_impl;
}

size_t InputDevice::Count() { return ::midiInGetNumDevs(); };

bool InputDevice::Open()
{
	if (m_isOpen)
		return true;

	HMIDIIN handle;
	if (Assert(::midiInOpen(&handle, m_impl->Id, (DWORD_PTR)ImplType::Callback, (DWORD_PTR)(void*)this, CALLBACK_FUNCTION|MIDI_IO_STATUS), "Opening input device"))
		return false;
	m_impl->Handle = handle;
	m_isOpen=true;
	if (Assert(::midiInStart(handle), "Starting input device"))
		return false;
	return true;
};

MMRESULT Assert(MMRESULT result, const char* message)
{
	if (result)
	{
		TCHAR buffer[MAXERRORLENGTH];
		midiInGetErrorText(result, buffer, MAXERRORLENGTH);
		if (message == nullptr)
			message = "Input device error";
		fprintf(stderr, "%s: %s\n", message, buffer);
	}
	return result;
};


bool InputDevice::Close()
{
	if (!m_isOpen)
		return true;
	m_isOpen = false;
	midiInClose(m_impl->Handle);
	m_impl->Handle = nullptr;
	return true;
};

void InputDevice::StartReceiveDump(size_t size/*BufferCallback^ callback*/)
{
	if (!m_isOpen)
		return;

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
};

struct CallbackState
{
	HMIDIIN hDevice;
	UINT msg;
	DWORD_PTR dwInstance;
	DWORD_PTR dw1;
	DWORD_PTR dw2;
};

void CALLBACK InputDevice::ImplType::Callback(HMIDIIN hDevice, UINT msg, DWORD_PTR dwInstance, DWORD_PTR dw1, DWORD_PTR dw2)
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
