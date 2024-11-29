#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <list>

static constexpr uint32_t FLAG_INPUT  { 0x10000 };
static constexpr uint32_t FLAG_OUTPUT { 0x20000 };

#undef SendMessage

enum class MIDIMessage
{
	InputOpen     = 0x3C1,
	InputClose    = 0x3C2,
	InputData     = 0x3C3,
	InputLongData = 0x3C4,
	InputError    = 0x3C5,
	InputLongError= 0x3C6,
	OutputOpen    = 0x3C7,
	OutputClose   = 0x3C8,
	OutputDone    = 0x3C9,
	InputMoreData = 0x3CC,
	OutputPosition= 0x3CA
};

enum class MessageType
{
	NoteOff=0x8,
	NoteOn=0x9,
	PolyphonicAftertouch=0xA,
	ControlChange=0xB,
	ProgramChange=0xC,
	ChannelAftertouch=0xD,
	PitchWheel=0xE,
	System=0xF
};

enum class SystemMessageType
{
	SysExStart=0,
	MTCQuarterFrame=1,
	SongPositionPointer=2,
	SongSelect=3,
	TuneRequest=6,
	SysExEnd=7,
	TimingClock=8,
	Start=10,
	Continue=11,
	Stop=12,
	ActiveSensing=14,
	Reset=15
};

struct Message
{
	union
	{
		uint8_t smallData[3];
		struct
		{
			uint8_t StatusL : 4;
			uint8_t StatusH : 4;
			union
			{
				uint8_t Note;
				uint8_t Number;
			};
			union
			{
				uint8_t Velocity;
				uint8_t Value;
			};
		};
		struct
		{
			uint8_t Status;
			union
			{
				uint16_t BufferSize;
				struct
				{
					uint8_t Byte1;
					uint8_t Byte2;
				};
			};
		};
	};
	uint8_t* Buffer;

	inline uint16_t Value16() const { return ((uint16_t)Byte1 << 7) | (Byte2); };
	inline uint16_t Value16(uint16_t value) { Byte1 = (value >> 7) & 0x7F; Byte2 = (value & 0x7F); return value; };
	inline MessageType Type() const { return (MessageType)StatusH; };
	inline SystemMessageType SubType() const { return (SystemMessageType)StatusL; };

	inline Message(uintptr_t dw)
		: Status((dw >> 0) & 0xFF)
		, Byte1((dw >> 8) & 0xFF)
		, Byte2((dw >> 16) & 0xFF)
		, Buffer(nullptr)
	{ };

	inline Message(uint8_t* buffer, size_t cbBuffer)
		: Status(0xF0)
		, BufferSize(cbBuffer)
		, Buffer(buffer)
	{ };

	Message() = default;
};

struct MIDIEventArgs
{
public:
	const struct Message Message;
	bool Cancel = false;
	inline MIDIEventArgs(const struct Message message) : Message(message) {};
};
typedef void (*MIDIEventHandler)(void* context, void* sender, MIDIEventArgs& e);
typedef std::pair<void*, MIDIEventHandler> MIDIEvent;

#ifdef _WIN32
#include "Device.win32.hpp"
#else
#include "Device.unix.hpp"
#endif

class Device
{
protected:
	bool m_isOpen = false;
	virtual void callback(MIDIMessage msg, uintptr_t dw1, uintptr_t dw2);
protected:
	Device();
	virtual ~Device();
public:
	virtual bool Open() = 0;
	virtual bool Close() = 0;
	virtual const char* Name() const = 0;
protected:
	static void GlobalMidiCallback(void*);
};
