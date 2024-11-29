#include <windows.h>

struct CallbackState
{
	HMIDIIN hDevice;
	UINT msg;
	DWORD_PTR dwInstance;
	DWORD_PTR dw1;
	DWORD_PTR dw2;
};

void Device::GlobalMidiCallback(void* state_)
{
	CallbackState* state = (CallbackState*)state_;
	((Device*)(void*)state->dwInstance)->callback((MIDIMessage)state->msg, state->dw1, state->dw2);
};


