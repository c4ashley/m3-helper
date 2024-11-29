#include "InputDevice.hpp"
#include "OutputDevice.hpp"
#include "SysexBuilder.hpp"
#include "Event.hpp"

#ifdef _WIN32
#include <windows.h>
#elif defined(_UNIX)
#include <signal.h>
#include <string.h>
#include <alsa/asoundlib.h>
static void rawmidi_list(void);
#endif

#ifdef _MSC_VER
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

struct ReceiveContext;

InputDevice* s_input;
OutputDevice* s_output;

InputDevice* ChooseInputDevice();
OutputDevice* ChooseOutputDevice();
void MessageReceived(void* context, void* sender, MIDIEventArgs& e);
void SendAndReceive(ReceiveContext* context);
void SysexProgress(ReceiveContext*);
bool Wait(ReceiveContext*);

#ifdef _WIN32
BOOL WINAPI ControlHandler(DWORD fdwCtrlType);
#else
void ControlHandler(int signo);
#endif

typedef void (*ProgressCallback)(ReceiveContext*);

enum class ReceiveStatus
{
	Idle,
	Waiting,
	Receiving,
	Finished,
	Overflow,
	Error
};

struct ReceiveContext
{
	class InputDevice* InputDevice;
	class OutputDevice* OutputDevice;
	const uint8_t* BufferOut;
	size_t cbBufferOut;
	uint8_t expectedFunctionin;
	uint8_t ReceivedFunction;
	uint8_t* BufferIn;
	size_t cbBufferIn;
	void* UserData;
	size_t rxIndex = 0;
	ProgressCallback Callback;
	class Event Event;
	ReceiveStatus Status = ReceiveStatus::Idle;

	inline ReceiveContext()
		: Event("SendReceiveEvt")
	{ };

	inline ~ReceiveContext()
	{ };

	inline void ResetState()
	{
		rxIndex = 0;
		Status = ReceiveStatus::Idle;
		//Event.Reset();
	};

	inline void Initialise(::InputDevice* input, ::OutputDevice* output, ProgressCallback callback = nullptr)
	{
		InputDevice = input;
		OutputDevice = output;
		Callback = callback;
	};

	void FillSimple(const uint8_t* bufferOut, size_t cbBufferOut, uint8_t expectedFunctionIn)
	{
		ResetState();
		BufferOut = bufferOut;
		this->cbBufferOut = cbBufferOut;
		BufferIn = nullptr;
		cbBufferIn = 0;
		this->expectedFunctionin = expectedFunctionIn;
	}
};

int main(int argc, const char* argv[])
{
	rawmidi_list();
	s_output = OutputDevice::GetByName("M3 1 SOUND");
	if (s_output == nullptr)
	{
		s_output = ChooseOutputDevice();
		if (s_output == nullptr)
			return 1;
	}
	printf("Using output device: %s\n", s_output->Name());

	s_input = InputDevice::GetByName("M3 1 KEYBOARD");
	if (s_input == nullptr)
	{
		s_input = ChooseInputDevice();
		if (s_input == nullptr)
			return 1;
	}
	printf("Using input device: %s\n", s_input->Name());

	if (s_input->Open())
		printf("Input device opened OK\n");
	else
		fprintf(stderr, "Failed opening input device\n");

	if (s_output->Open())
		printf("Output device opened OK\n");
	else
		fprintf(stderr, "Failed opening output device\n");

	s_input->AddCallback(MessageReceived);
	s_input->StartReceiveDump(1024);

	SysexBuilder sysex(0);

#ifdef _WIN32
	if (!SetConsoleCtrlHandler(ControlHandler, TRUE))
#else
	if (signal(SIGINT, ControlHandler) == SIG_ERR)
#endif
		fprintf(stderr, "Couldn't set Ctrl-C handler\n");

	uint8_t copydest_bank = 0;
	uint8_t copydest_num  = 0;
	uint8_t copysrc_bank  = 0;
	uint8_t copysrc_num   = 0;

	ReceiveContext context {};
	context.Initialise(s_input, s_output, SysexProgress);

	while (true)
	{
		char input[256];
		uint8_t message[256];
		printf("> ");
		fflush(stdout);
		if (fgets(input, 256, stdin) == nullptr)
			break;

		size_t cchInput = strnlen(input, 255);
		if (cchInput > 0 && input[cchInput - 1] == '\n')
		{
			input[cchInput - 1] = '\0';
			--cchInput;
		}

		if (strncasecmp("help", input, 4) == 0)
		{
			printf("mode      Set the current mode of the keyboard.\n");
			printf("    mode combi       Enter Combi Play mode\n");
			printf("    mode combi play  -- not implemented --\n");
			printf("    mode combi edit  -- not implemented --\n");
			printf("    mode prog        -- not implemented --\n");
			printf("    mode prog play   -- not implemented --\n");
			printf("    mode prog edit   -- not implemented --\n");
			printf("    mode play        -- not implemented --\n");
			printf("    mode edit        -- not implemented --\n");
			printf("    mode seq         -- not implemented --\n");
			printf("    mode samp        -- not implemented --\n");
			printf("    mode global      -- not implemented --\n");
			printf("    mode media       -- not implemented --\n");
			printf("\n");
			printf("copysrc   Set the source bank for future copy operations.\n");
			printf("    copysrc (I-A..I-G | U-A..U-G)\n");
			printf("\n");
			printf("copydest  Set the destination bank for future copy operations.\n");
			printf("    copydest (I-A..I-G | U-A..U-G)\n");
			printf("\n");
			printf("copyseq   Start a sequential copy operation into consecutive destination patches.\n");
			printf("          Enters a prompt, expecting source patches to copy from, or copying incrementally from the source bank.\n");
			printf("          Copy will only exist in temporary memory on the keyboard until committed with 'done'/'stop'/'quit'/'exit'.\n");
			printf("          'cancel' will stop the sequential copy without committing to non-volatile memory.\n");
			printf("    copyseq [startnum]  Start a sequential copy, optionally with the first destination at the given patch number.\n");
			printf("\n");
			printf("copynext\n");
			printf("\n");
			printf("exit|quit Exits the program\n");
		}
		else if (strncasecmp("mode ", input, 5) == 0)
		{
			if (strncasecmp("combi", &input[5], 5) == 0)
			{
				size_t size = sysex.ModeChange(message, 0);
				context.FillSimple(message, size, 0x24);
				SendAndReceive(&context);
				Wait(&context);
			}
		}
		else if (strncasecmp("copysrc ", input, 8) == 0)
		{
			if (cchInput < 11)
				goto CopySrcError;

			copysrc_bank = 0;

			switch (input[8])
			{
			case 'u':
			case 'U':
				copysrc_bank |= 64;
				break;
			case 'i':
			case 'I':
				copysrc_bank &= ~64;
				break;
			default:
				goto CopySrcError;
			}

			if (input[9] != '-')
				goto CopySrcError;

			if (input[10] >= 'A' && input[10] <= 'G')
				copysrc_bank |= input[10] - 'A';
			else if (input[10] >= 'a' && input[10] <= 'g')
				copysrc_bank |= input[10] - 'a';
			else
				goto CopySrcError;

			continue;
		CopySrcError:
			fprintf(stderr, "Invalid input. e.g., copysrc U-A\n");
			continue;
		}
		else if (strncasecmp("copydest ", input, 9) == 0)
		{
			if (cchInput < 12)
				goto CopyDestError;

			copydest_bank = 0;
			copydest_num = 0;

			switch (input[9])
			{
			case 'u':
			case 'U':
				copydest_bank |= 64;
				break;
			case 'i':
			case 'I':
				copydest_bank &= ~64;
				break;
			default:
				goto CopyDestError;
			}

			if (input[10] != '-')
				goto CopyDestError;

			if (input[11] >= 'A' && input[11] <= 'G')
				copydest_bank |= input[11] - 'A';
			else if (input[11] >= 'a' && input[11] <= 'g')
				copydest_bank |= input[11] - 'a';
			else
				goto CopyDestError;

			continue;
		CopyDestError:
			fprintf(stderr, "Invalid input. e.g., copydest U-A\n");
			continue;
		}
		else if (strncasecmp("copyseq", input, 7) == 0)
		{
			if (cchInput > 8)
				copydest_num = strtoul(&input[8], nullptr, 10);

			for (;;)
			{
				printf("%03d < ", copydest_num);
				fflush(stdout);

				if (fgets(input, 256, stdin) == nullptr)
					goto CopyseqDone;

				cchInput = strnlen(input, 255);
				if (cchInput > 0 && input[cchInput - 1] == '\n')
				{
					input[cchInput - 1] = '\0';
					--cchInput;
				}

				if (cchInput >= 4)
				{
					if (strncasecmp(input, "cancel", 6) == 0)
						goto CopyseqCancel;
					if (strncasecmp(input, "done", 4) == 0 || strncasecmp(input, "stop", 4) == 0
						|| strncasecmp(input, "quit", 4) == 0 || strncasecmp(input, "exit", 4) == 0)
						goto CopyseqDone;
				}

				if (cchInput > 0)
				{
					copysrc_num = strtoul(input, nullptr, 10);
				}

				#pragma region Download data
				printf("Copying from %d:%d to %d:%d\n", copysrc_bank, copysrc_num, copydest_bank, copydest_num);
				printf("Receiving");
				fflush(stdout);

				size_t size = sysex.CombiParameterDumpRequest(message, copysrc_bank, copysrc_num);
				uint8_t* combiData = new uint8_t[64 * 1024];
				ReceiveContext context{};
				context.InputDevice = s_input;
				context.OutputDevice = s_output;
				context.BufferIn = combiData;
				context.BufferOut = message;
				context.cbBufferIn = 64 * 1024;
				context.cbBufferOut = size;
				context.expectedFunctionin = 0x73;
				context.Callback = &SysexProgress;

				SendAndReceive(&context);
				if (!Wait(&context))
					goto CopyseqError;
				#pragma endregion

				#pragma region Upload data
				printf("Uploading");
				combiData[6] = copydest_bank;
				combiData[8] = copydest_num;

				{
					size_t bytesReceived = context.rxIndex;
					context.ResetState();
					context.FillSimple(combiData, bytesReceived, 0x24);
				}
				SendAndReceive(&context);
				if (!Wait(&context))
					goto CopyseqError;
				#pragma endregion


				copysrc_num++;
				copydest_num++;
			CopyseqError:
				continue;
			}
		CopyseqDone:
			{
			#pragma region Save data
				printf("Saving");
				size_t size = sysex.StoreCombinationBank(message, copydest_bank);
				context.FillSimple(message, size, 0x24);
				SendAndReceive(&context);
				Wait(&context);
			}
			#pragma endregion

		CopyseqCancel:
			continue;
		}
		else if (strncasecmp("copynext", input, 8) == 0)
		{
			if (cchInput > 8)
				copysrc_num = strtoul(&input[9], nullptr, 10);

			printf("Copying from %d:%d to %d:%d\n", copysrc_bank, copysrc_num, copydest_bank, copydest_num);
			printf("Receiving");
			fflush(stdout);

			size_t size = sysex.CombiParameterDumpRequest(message, copysrc_bank, copysrc_num);
			uint8_t* combiData = new uint8_t[64 * 1024];
			ReceiveContext context {};
			context.InputDevice = s_input;
			context.OutputDevice = s_output;
			context.BufferIn = combiData;
			context.BufferOut = message;
			context.cbBufferIn = 64 * 1024;
			context.cbBufferOut = size;
			context.expectedFunctionin = 0x73;
			context.Callback = &SysexProgress;

			SendAndReceive(&context);
			context.Event.Wait();

			switch (context.Status)
			{
				case ReceiveStatus::Finished:
					printf("OK\n");
					break;
				case ReceiveStatus::Overflow:
					printf("Data overflow error\n");
					goto CopynextError;
				case ReceiveStatus::Error:
					printf("Packet error\n");
					goto CopynextError;
			}

			printf("Saving");
			combiData[6] = copydest_bank;
			combiData[8] = copydest_num;

			{
				size_t bytesReceived = context.rxIndex;
				context.ResetState();
				context.FillSimple(combiData, bytesReceived, 0x24);
			}
			SendAndReceive(&context);
			context.Event.Wait();

			switch (context.Status)
			{
				case ReceiveStatus::Finished:
					printf("OK\n");
					break;
				case ReceiveStatus::Overflow:
					printf("Data overflow error\n");
					goto CopynextError;
				case ReceiveStatus::Error:
					printf("Packet error\n");
					goto CopynextError;
			}

			copysrc_num++;
			copydest_num++;
		CopynextError:
			continue;
		}
		else if (strcasecmp("exit", input) == 0 || strcasecmp("quit", input) == 0)
			break;
	}

	printf("Cleaning up . . .\n");
	s_input->Close();
	s_output->Close();

	delete s_input;
	delete s_output;

	return 0;
};


void OnReceived(void* context_, void* sender, MIDIEventArgs& e)
{
	if (e.Message.Status != 0xF0)
		return;

	size_t cbBuffer = e.Message.BufferSize;
	ReceiveContext* context = (ReceiveContext*)context_;

	if (context->Status == ReceiveStatus::Waiting)
	{
		if (e.Message.BufferSize < 6)
		{
			context->Status = ReceiveStatus::Error;
			context->InputDevice->RemoveCallback(OnReceived, context_);
			if (context->Callback)
				context->Callback(context);
			context->Event.Signal();
			return;
		}
		context->ReceivedFunction = e.Message.Buffer[4];
		if (context->ReceivedFunction != context->expectedFunctionin)
		{
			context->Status = ReceiveStatus::Error;
			context->InputDevice->RemoveCallback(OnReceived, context_);
			if (context->Callback)
				context->Callback(context);
			context->Event.Signal();
			return;
		}

		if (context->BufferIn == nullptr)
		{
			context->Status = ReceiveStatus::Finished;
			context->InputDevice->RemoveCallback(OnReceived, context_);
			if (context->Callback)
				context->Callback(context);
			context->Event.Signal();
			return;
		}

		context->Status = ReceiveStatus::Receiving;
	}


	if (cbBuffer > context->cbBufferIn - context->rxIndex)
		cbBuffer = context->cbBufferIn - context->rxIndex;
	memcpy(context->BufferIn + context->rxIndex, e.Message.Buffer, cbBuffer);
	context->rxIndex += cbBuffer;
	if (e.Message.BufferSize > 0 && e.Message.Buffer[e.Message.BufferSize - 1] == 0xF7)
	{
		context->Status = ReceiveStatus::Finished;
		context->InputDevice->RemoveCallback(OnReceived, context_);
	}
	else if (context->rxIndex == context->cbBufferIn)
	{
		context->Status = ReceiveStatus::Overflow;
		context->InputDevice->RemoveCallback(OnReceived, context_);
	}

	if (context->Callback)
		context->Callback(context);

	if (context->Status != ReceiveStatus::Receiving)
		context->Event.Signal();
};

void SendAndReceive(ReceiveContext* context)
{
	context->Status = ReceiveStatus::Waiting;
	context->InputDevice->AddCallback(OnReceived, context);
	context->OutputDevice->LongMessage(context->BufferOut, context->cbBufferOut);
};

void SysexProgress(ReceiveContext*)
{
	printf(".");
	fflush(stdout);
};

InputDevice* ChooseInputDevice()
{
	auto nInputDevices = InputDevice::Count();
	printf("%zu input devices:\n", nInputDevices);
	char name[32];
	DeviceEnumerator device {};
	int i = -1;
	while (InputDevice::EnumerateNext(&device))
	{
		++i;
		if (InputDevice::GetName(&device, name, sizeof(name)))
			printf("  [%zu]: %s\n", i+1, name);

	}
	InputDevice::StopEnumeration(&device);

	printf("Enter a device number: ");
	fflush(stdout);
	if (fgets(name, 32, stdin) == nullptr)
		return nullptr;

	long id;
	if ((id = strtoul(name, nullptr, 10)) <= 0)
		return nullptr;

	return nullptr;
	return InputDevice::GetByID(id - 1);
};

OutputDevice* ChooseOutputDevice()
{
	auto nOutputDevices = OutputDevice::Count();
	printf("%zu output devices:\n", nOutputDevices);
	char name[32];
	DeviceEnumerator device {};
	int i = -1;
	while (OutputDevice::EnumerateNext(&device))
	{
		++i;
		if (OutputDevice::GetName(&device, name, sizeof(name)))
			printf("  [%zu]: %s\n", i+1, name);
	}
	OutputDevice::StopEnumeration(&device);

	printf("Enter a device number: ");
	fflush(stdout);
	if (fgets(name, 32, stdin) == nullptr)
		return nullptr;

	long id;
	if ((id = strtoul(name, nullptr, 10)) <= 0)
		return nullptr;

	return OutputDevice::GetByID(id - 1);
};

void MessageReceived(void* context, void* sender, MIDIEventArgs& e)
{
	//if (e.Message.Type() != MessageType::System || (e.Message.SubType() != SystemMessageType::TimingClock && e.Message.SubType() != SystemMessageType::ActiveSensing))
	//	printf("Message received\n");
};

bool Wait(ReceiveContext* context)
{
	context->Event.Wait();
	switch (context->Status)
	{
	case ReceiveStatus::Finished:
		printf("OK\n");
		return true;
	case ReceiveStatus::Overflow:
		printf("Overflow error\n");
		return false;
	case ReceiveStatus::Error:
		printf("Packet error (Expected %02Xh, got %02Xh)\n", context->expectedFunctionin, context->ReceivedFunction);
		return false;
	default:
		printf("Unexpected state\n");
		return false;
	}
};

#ifdef _WIN32
BOOL WINAPI ControlHandler(DWORD fdwCtrlType)
{
	switch (fdwCtrlType)
	{
	case CTRL_C_EVENT:
		fputs("quit\n", stdout);
		return TRUE;
	default:
		return FALSE;
	}
};
#else
void ControlHandler(int signo)
{
	if (signo == SIGINT)
		fputs("quit\n", stdout);
};
static constexpr const char* sndtypename(snd_config_type_t type)
{
	switch (type)
	{
		case SND_CONFIG_TYPE_INTEGER  : return "int";
		case SND_CONFIG_TYPE_INTEGER64: return "long";
		case SND_CONFIG_TYPE_REAL     : return "float";
		case SND_CONFIG_TYPE_STRING   : return "string";
		case SND_CONFIG_TYPE_POINTER  : return "pointer";
		case SND_CONFIG_TYPE_COMPOUND : return "compound";
		default                       : return "unknown";
	}
};

void inspect(snd_config_t* config, int indent)
{
	const char* id;
	char* prefix = (char*) alloca(indent + 1);
	memset(prefix, ' ', indent);
	prefix[indent] = '\0';
	if (snd_config_get_id(config, &id) != 0)
		id = nullptr;
	printf("%s%s %s", prefix, sndtypename(snd_config_get_type(config)), id ? id : "(no id)");
	switch (snd_config_get_type(config))
	{
		case SND_CONFIG_TYPE_COMPOUND:
		{
			if (snd_config_is_array(config) > 0)
				printf("[%d]", snd_config_is_array(config));
			printf("\n");

			snd_config_iterator_t pos, next;
			snd_config_for_each(pos, next, config)
			{
				snd_config_t* entry = snd_config_iterator_entry(pos);
				inspect(entry, indent + 2);
			};
		} break;
		case SND_CONFIG_TYPE_INTEGER:
		{
			long value;
			if (snd_config_get_integer(config, &value) == 0)
				printf(" = %ld (0x%08lx)\n", value, value);
			else
				printf(" = (error)\n");
		} break;
		case SND_CONFIG_TYPE_INTEGER64:
		{
			long long value;
			if (snd_config_get_integer64(config, &value) == 0)
				printf(" = %lld (0x%08llx)\n", value, value);
			else
				printf(" = (error)\n");
		} break;
		case SND_CONFIG_TYPE_STRING:
		{
			const char* value;
			if (snd_config_get_string(config, &value) == 0)
				printf(" = \"%s\"\n", value);
			else
				printf(" = (error)\n");
		} break;
		case SND_CONFIG_TYPE_REAL:
		{
			double value;
			if (snd_config_get_real(config, &value) == 0)
				printf(" = %f\n", value);
			else
				printf(" = (error)\n");
		} break;
		case SND_CONFIG_TYPE_POINTER:
		{
			const void* value;
			if (snd_config_get_pointer(config, &value) == 0)
				printf(" = 0x%p\n", value);
			else
				printf(" = (error)\n");
		} break;
	}
};
static void rawmidi_list(void)
{
	snd_output_t *output;
	snd_config_t *config;
	int err;

	if ((err = snd_config_update()) < 0) {
		return;
	}
	//if ((err = snd_output_stdio_attach(&output, stdout, 0)) < 0) {
	//	return;
	//}
	if (snd_config_search(snd_config, "rawmidi", &config) >= 0) {
		inspect(config, 0);
	}
}
#endif
