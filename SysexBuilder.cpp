#include "SysexBuilder.hpp"

SysexBuilder::SysexBuilder(uint8_t deviceId) : m_deviceId(deviceId) {};

size_t SysexBuilder::Header(uint8_t* buffer, uint8_t function, size_t payloadSize) const
{
	buffer[0] = 0xF0;
	buffer[1] = 0x42;
	buffer[2] = 0x30 | m_deviceId;
	buffer[3] = 0x75;
	buffer[4] = function;
	if ((int)payloadSize != ~0)
		buffer[5 + payloadSize] = 0xF7;
	return 6 + payloadSize;
};

size_t SysexBuilder::ModeChange(uint8_t* buffer, uint8_t mode) const
{
	size_t size = Header(buffer, 0x4E, 1);
	buffer[5] = mode;
	return size;
};

size_t SysexBuilder::CombiParameterDumpRequest(uint8_t* buffer, uint8_t bank, uint8_t num)
{
	size_t size = Header(buffer, 0x72, 4);
	buffer[5] = 1;
	buffer[6] = bank;
	buffer[7] = 0;
	buffer[8] = num;
	return size;
};

size_t SysexBuilder::StoreCombination(uint8_t* buffer, uint8_t bank, uint8_t num)
{
	size_t size = Header(buffer, 0x77, 4);
	buffer[5] = 1;
	buffer[6] = bank;
	buffer[7] = 0;
	buffer[8] = num;
	return size;
};

size_t SysexBuilder::StoreCombinationBank(uint8_t* buffer, uint8_t bank)
{
	size_t size = Header(buffer, 0x76, 2);
	buffer[5] = 0x11;
	buffer[6] = bank;
	return size;
};
