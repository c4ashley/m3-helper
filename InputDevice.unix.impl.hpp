#pragma once
#ifndef INPUTDEVICE_HEADER
#include "InputDevice.unix.hpp"
#endif
struct ImplType
{
	//snd_rawmidi_status_t* Status;
	snd_rawmidi_t* Handle;
	int Id;
};
