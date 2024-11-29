#pragma once
#include <cstdint>
#ifdef _UNIX
#include <sys/types.h>
#endif

class SysexBuilder
{
private:
	const uint8_t m_deviceId;

	size_t Header(uint8_t* buffer, uint8_t function, size_t payloadSize = ~0) const;
public:
	SysexBuilder(uint8_t deviceId);
	size_t ModeChange(uint8_t* buffer, uint8_t mode) const;
	size_t CombiParameterDumpRequest(uint8_t* buffer, uint8_t bank, uint8_t num);
	size_t StoreCombination(uint8_t* buffer, uint8_t bank, uint8_t num);
	size_t StoreCombinationBank(uint8_t* buffer, uint8_t bank);


};
