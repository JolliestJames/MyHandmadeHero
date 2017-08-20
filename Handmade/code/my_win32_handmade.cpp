/*------------------------------------------------------------------
	$File: $
	$Date: $
	$Revision: $
	$Creator: James Martinez
	$Notice: This is ambitious
------------------------------------------------------------------*/

/*
	This is not a final platform layer
	
	- Saved game locations
	- Getting a handle to our executable
	- Asset loading path
	- threading (launch a thread)
	- raw input (Support for multiple keyboards)
	- Sleep/timeBeginPeriod
	- ClipCursor() (for multimonitor support)
	- Fullscreen support
	- WM_SETCURSOR (control cursor visibility)
	- QueryCancelAutoplay
	- WM_ACTIVATEAPP (for when we are not the active app)
	- Blit speed improvements (BitBlt)
	- Hardware acceleration (OpenGL or Direct3D or BOTH)
	- GetKeyboardLayout (for french keyboards, international WASD support)
	
	Just a partial list of shit we need to do
*/

#include <stdint.h>
#include <math.h>
#include <malloc.h>

#define internal static
#define local_persist static
#define global_variable static

#define Pi 3.14159265359f

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float real32;
typedef double real64;

#include "Handmade.cpp"
#include <windows.h>
#include <xinput.h>
#include <dsound.h>
#include <stdio.h>

#include "my_win32_handmade.h"

global_variable bool32 GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackbuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(X_Input_Get_State);
X_INPUT_GET_STATE(XInputGetStateStub)
{
	return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable X_Input_Get_State *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(X_Input_Set_State);
X_INPUT_SET_STATE(XInputSetStateStub)
{
	return(ERROR_DEVICE_NOT_CONNECTED);
}

global_variable X_Input_Set_State *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal debug_read_file_result
DEBUGPlatformReadEntireFile(char *Filename)
{
	debug_read_file_result Result = {};
	
	HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	
	if(FileHandle != INVALID_HANDLE_VALUE)
	{
		LARGE_INTEGER FileSize;
		
		if(GetFileSizeEx(FileHandle, &FileSize))
		{
			//TODO: Defines for maximum values UInt32Max
			uint32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
			Result.Contents = VirtualAlloc(0, FileSize32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
			
			if(Result.Contents)
			{
				DWORD BytesRead;
				
				if(ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0) && (FileSize32 == BytesRead))
				{
					//NOTE: File read successfully
					Result.ContentsSize = FileSize32;
				}
				else
				{
					//TODO: Logging
					DEBUGPlatformFreeFileMemory(Result.Contents);
					Result.Contents = 0;
				}
			}
			else
			{
				//TODO: Logging
			}
		}
		
		CloseHandle(FileHandle);
		
	}
	
	return(Result);
	
}

internal void DEBUGPlatformFreeFileMemory(void *Memory)
{
	if(Memory)
	{
		VirtualFree(Memory, 0, MEM_RELEASE);
	}
}

internal bool32 
DEBUGPlatformWriteEntireFile(char *Filename, uint32 MemorySize, void *Memory)
{
	bool32 Result = false;
	
	HANDLE FileHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
	
	if(FileHandle != INVALID_HANDLE_VALUE)
	{
		DWORD BytesWritten;

		if(WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0))
		{
			//NOTE: File read successfully
			Result = (BytesWritten == MemorySize);
		}
		else
		{
			//TODO: Logging
		}
	
		CloseHandle(FileHandle);
	}	
	else
	{
		//TODO: Logging
	}
	
	return(Result);
	
}

internal void
Win32_Load_X_Input(void)
{
	HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
	
	if(!XInputLibrary)
	{
		XInputLibrary = LoadLibraryA("xinput9_1_0.dll");
	}
	
	if(!XInputLibrary)
	{
		XInputLibrary = LoadLibraryA("xinput1_3.dll");
	}
	
	if(XInputLibrary)
	{
		//we have to cast the GetProcAddress return value to the type we want
		XInputGetState = (X_Input_Get_State *)GetProcAddress(XInputLibrary, "XInputGetState");
		if(!XInputGetState){XInputGetState = XInputGetStateStub;}
		
		XInputSetState = (X_Input_Set_State *)GetProcAddress(XInputLibrary, "XInputSetState");
		if(!XInputSetState){XInputSetState = XInputSetStateStub;}
		
		//diagnostic
	
	}
	else
	{
		
	}
}

internal void
Win32InitializeDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
	//load the library
	HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
	
	if(DSoundLibrary)
	{
		direct_sound_create *DirectSoundCreate = (direct_sound_create *)
			GetProcAddress(DSoundLibrary, "DirectSoundCreate");	
		
		LPDIRECTSOUND DirectSound;
		
		if(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
		{
			//we're creating memory and writing int16 bit samples into it
			//LEFT RIGHT LEFT RIGHT LEFT RIGHT 
			//how big is [LEFT RIGHT] in bytes? 16+16/8 = 4
			WAVEFORMATEX WaveFormat = {};
			WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
			WaveFormat.nChannels = 2;
			WaveFormat.nSamplesPerSec = SamplesPerSecond;
			WaveFormat.wBitsPerSample = 16;
			WaveFormat.nBlockAlign = (WaveFormat.nChannels*WaveFormat.wBitsPerSample)/8;
			WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec*WaveFormat.nBlockAlign;
			WaveFormat.cbSize = 0;
			
			if(SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
			{
				//do we need DSBCAPS_GLOBALFOCUS?
				DSBUFFERDESC BufferDescription = {};
				BufferDescription.dwSize = sizeof(BufferDescription);
				BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
				
				//create primary buffer
				
				LPDIRECTSOUNDBUFFER PrimaryBuffer;
				
				if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
				{
					if(SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)))
					{
						//we've set the format of the primary of buffer. if everything went right..
						OutputDebugStringA("Primary buffer set\n");
					}
					else
					{
						//MORE DIAGNOSTICS
					}
				}
				else
				{
					//MORE DIAGNOSTICS
				}
			}
			else
			{
				//MORE DIAGNOSTICS
			}
			
			// DSCBCAPS_GETCURRENTPOSITION2
			DSBUFFERDESC BufferDescription = {};
			BufferDescription.dwSize = sizeof(BufferDescription);
			BufferDescription.dwFlags = 0;
			BufferDescription.dwBufferBytes = BufferSize;
			BufferDescription.lpwfxFormat = &WaveFormat;
			
			if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0)))
			{
				OutputDebugStringA("Secondary buffer created successfully\n");
			}
		
			//start playing
		}
		else
		{
			//diagnostic
		}
	}
	else
	{
		//diagnostic
	}
}

internal win32_window_dimension Win32GetWindowDimension(HWND Window)
{
	win32_window_dimension Result;
	
	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	Result.Width = ClientRect.right - ClientRect.left;
	Result.Height = ClientRect.bottom - ClientRect.top;

	return Result;
}

internal void 
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
	if(Buffer->Memory)
	{
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
	}
	
	Buffer->Width = Width;
	Buffer->Height = Height;
	
	int BytesPerPixel = 4;
	
	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->Width;
	Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;

	int BitmapMemorySize = (Buffer->Width*Buffer->Height)*BytesPerPixel;
	
	Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
	
	Buffer->Pitch = Width*BytesPerPixel;
}

internal void
Win32DisplayBufferInWindow(HDC DeviceContext, win32_offscreen_buffer *Buffer,
	int WindowWidth, int WindowHeight)
{
	//Correct aspect ratio
	StretchDIBits(DeviceContext, 0, 0, WindowWidth, WindowHeight, 
	0, 0, Buffer->Width, Buffer->Height, Buffer->Memory,
	&Buffer->Info, DIB_RGB_COLORS, SRCCOPY);
}

internal void
Win32ClearBuffer(win32_sound_output *SoundOutput)
{
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;
	if(SUCCEEDED(GlobalSecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize,
	&Region1, &Region1Size, &Region2, &Region2Size, 0)))
	{
		int8 *DestSample = (int8 *)Region1;
		for(DWORD SampleIndex = 0; SampleIndex < Region1Size; ++SampleIndex)
		{
			*DestSample++ = 0;
		}
		DestSample = (int8 *)Region2;
		for(DWORD SampleIndex = 0; SampleIndex < Region2Size; ++SampleIndex)
		{
			*DestSample++ = 0;
		}
	}
	
	GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
}

internal void
Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite,
	game_sound_output_buffer *SourceBuffer)
{
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;
	
	if(SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite,
	&Region1, &Region1Size, &Region2, &Region2Size, 0)))
	{	

		DWORD Region1SampleCount = Region1Size/SoundOutput->BytesPerSample;
		int16 *DestSample = (int16 *)Region1;
		
		int16 *SourceSample = SourceBuffer->Samples;
		
		for(DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
		{
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundOutput->RunningSampleIndex;
		}
		
		DWORD Region2SampleCount = Region2Size/SoundOutput->BytesPerSample;
		DestSample = (int16 *)Region2;
		
		for(DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
		{
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundOutput->RunningSampleIndex;
		}
	
		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	
	}
}

internal void 
Win32ProcessKeyboardMessage(game_button_state *NewState, bool32 IsDown)
{
	Assert(NewState->EndedDown != IsDown);
	
	NewState->EndedDown = IsDown;
	++NewState->HalfTransitionCount;
}

internal void 
Win32ProcessXInputDigitalButton(DWORD XInputButtonState, game_button_state *OldState,
	game_button_state *NewState, DWORD ButtonBit)
{
	NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
	NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

internal real32
Win32ProcessXInputStickValue(SHORT Value, SHORT DeadZoneThreshold)
{
	real32 Result = 0;
	
	if(Value < -DeadZoneThreshold)
	{
		Result = (real32)Value / 32768.0f;
	}
	else if(Value > DeadZoneThreshold)
	{
		Result = (real32)Value / 32767.0f;
	}
	
	return(Result);
}

internal void
Win32ProcessPendingMessages(game_controller_input *KeyboardController)
{
	MSG Message;
	
	while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
	{
		switch(Message.message)
		{
			
			case WM_QUIT:
			{
				GlobalRunning = false;
			} break;
			
			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP:
			case WM_KEYDOWN:
			case WM_KEYUP:
			{
				uint32 VKCode = (uint32)Message.wParam;
				//Either we get 1 << 30 or 0, we want 1 if the former and 0 if the latter
				bool32 WasDown = ((Message.lParam & (1 << 30)) != 0); 
				bool32 IsDown = 	((Message.lParam & (1 << 31)) == 0);
				
				if(WasDown != IsDown)
				{
					
					if(VKCode == 'W')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
					}
					else if(VKCode == 'A')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
					}
					else if(VKCode == 'S')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
					}
					else if(VKCode == 'D')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
					}
					else if(VKCode == 'Q')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
					}
					else if(VKCode == 'E')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
					}
					else if(VKCode == VK_UP)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
					}
					else if(VKCode == VK_DOWN)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
					}
					else if(VKCode == VK_LEFT)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown);
					}
					else if(VKCode == VK_RIGHT)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
					}
					else if(VKCode == VK_ESCAPE)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
					}
					else if(VKCode == VK_SPACE)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
					}
				}
		
				bool32 AltKeyWasDown = (Message.lParam & (1 << 29));
		
				if((VKCode == VK_F4) && AltKeyWasDown)
				{
					GlobalRunning = false;
				}
			} break;
			
			default:
			{
				TranslateMessage(&Message);
				DispatchMessageA(&Message);
			} break;	
		}
	}
}

LRESULT CALLBACK 
Win32MainWindowCallback(HWND   Window,
						UINT   Message,
						WPARAM wParam,
						LPARAM lParam)

{
	LRESULT Result = 0;

	switch(Message)
	{
		case WM_CLOSE:
		{
			// NOTE: Handle this with a message to user
			OutputDebugStringA("WM_CLOSE\n");
			GlobalRunning = false;
			
		} break;
		
		case WM_ACTIVATEAPP:
		{
			OutputDebugStringA("WM_ACTIVATEAPP\n");	
		} break;

		case WM_DESTROY:
		{
			// NOTE: Handle this as an error - recreate window
			OutputDebugStringA("WM_DESTROY\n");
			GlobalRunning = false;
		} break;
		
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			Assert(!"Keyboard input came in through a non-dispatch message!");
							
		} break;
		
		case WM_PAINT:
		{
			PAINTSTRUCT Paint;
			HDC DeviceContext = BeginPaint(Window, &Paint);
			
			win32_window_dimension Dimension = Win32GetWindowDimension(Window);
			Win32DisplayBufferInWindow(DeviceContext, &GlobalBackbuffer,
				Dimension.Height, Dimension.Width);
									
			EndPaint(Window, &Paint);
			
		} break;
		
		default:
		{
			//OutputDebugStringA("default\n");
			
			Result = DefWindowProcA(Window, Message, wParam, lParam); 
		
		} break;
	}
	return(Result);
}

int CALLBACK
WinMain(
  HINSTANCE Instance,
  HINSTANCE PrevInstance,
  LPSTR     CommandLine,
  int       ShowCode)
{
	LARGE_INTEGER PerfCountFrequencyResult;
	QueryPerformanceFrequency(&PerfCountFrequencyResult);
	int64 PerfCountFrequency = PerfCountFrequencyResult.QuadPart;
	
	Win32_Load_X_Input();

	WNDCLASSA WindowClassObject = {};

	Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);
	WindowClassObject.style = CS_HREDRAW|CS_VREDRAW;
	WindowClassObject.lpfnWndProc = Win32MainWindowCallback;
	WindowClassObject.hInstance = Instance;
	//WindowClass.hIcon;
	WindowClassObject.lpszClassName = "HandmadeHeroWindowClass";
	
	if(RegisterClassA(&WindowClassObject))
	{
		HWND Window = 
			CreateWindowExA(
				0,
				WindowClassObject.lpszClassName,
				"Handmade Hero",
				WS_OVERLAPPEDWINDOW|WS_VISIBLE,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				0,
				0,
				Instance,
				0);
		
		if(Window)
		{
			HDC DeviceContext = GetDC(Window);
			win32_sound_output SoundOutput = {};

			//make this so long that there's no way the game could ever pause for this long so we can see where the play cursor is?
			SoundOutput.SamplesPerSecond = 48000;
			SoundOutput.RunningSampleIndex = 0;
			SoundOutput.BytesPerSample = sizeof(int16)*2;
			SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample;
			SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;
			Win32InitializeDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
			Win32ClearBuffer(&SoundOutput);
			GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
		
			GlobalRunning = true;
	
			int16 *Samples = (int16 *)VirtualAlloc
			(	
				0,
				SoundOutput.SecondaryBufferSize,
				MEM_RESERVE|MEM_COMMIT, 
				PAGE_READWRITE
			);
			
#if HANDMADE_INTERNAL
		LPVOID BaseAddress = (LPVOID)Terabytes(2);
#else
		LPVOID BaseAddress = 0;
#endif	
			
			game_memory GameMemory = {};
			GameMemory.PermanentStorageSize = Megabytes(64);
			GameMemory.TransientStorageSize = Gigabytes(1);
			
			//Handle various memory footprints
			
			uint64 TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
			
			GameMemory.PermanentStorage = VirtualAlloc(BaseAddress, (size_t)TotalSize,
												MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
			
			GameMemory.TransientStorage = ((uint8 *)GameMemory.PermanentStorage + 
										GameMemory.PermanentStorageSize);
			
			if(Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage)
			{
		
				game_input Input[2] = {};
				game_input *NewInput = &Input[0];
				game_input *OldInput = &Input[1];

				LARGE_INTEGER LastCounter;
				QueryPerformanceCounter(&LastCounter);
				uint64 LastCycleCount = __rdtsc();
				
				while(GlobalRunning)
				{
					//TODO: Zeroing macro
					//TODO: We can't zero everything because the up/down state will be wrong
					game_controller_input *OldKeyboardController = GetController(OldInput, 0);
					game_controller_input *NewKeyboardController = GetController(NewInput, 0);
					game_controller_input ZeroController = {};
					*NewKeyboardController = ZeroController;
					NewKeyboardController->IsConnected = true;
					
					for(int ButtonIndex = 0;
						ButtonIndex < ArrayCount(NewKeyboardController->Buttons);
						++ButtonIndex)
					{
						NewKeyboardController->Buttons[ButtonIndex].EndedDown = 
							OldKeyboardController->Buttons[ButtonIndex].EndedDown;
					}
					
					Win32ProcessPendingMessages(NewKeyboardController);
					
					DWORD MaxControllerCount = XUSER_MAX_COUNT;
					
					if(MaxControllerCount > (ArrayCount(NewInput->Controllers) - 1))
					{
						MaxControllerCount = (ArrayCount(NewInput->Controllers) - 1);
					}
					
					//should we poll this more frequently
					for(DWORD ControllerIndex = 0;	
						ControllerIndex < MaxControllerCount; 
						++ControllerIndex)
					{
						DWORD OurControllerIndex = ControllerIndex + 1;
						
						game_controller_input *OldController = GetController(OldInput, OurControllerIndex);
						game_controller_input *NewController = GetController(NewInput, OurControllerIndex);

						XINPUT_STATE ControllerState;
						//when we call XInputGetState, if no controller is plugged in for an index, XINPUT will stall
						//for several millis. THIS IS REALLY BAD, WE ONLY WANT TO PULL THE CONNECTED CONTROLLERS
						if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
						{
							NewController->IsConnected = true;
							
							//TODO: see if controllerstate.dwpacketnumber increments too rapidly
							XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;
							
							NewController->IsAnalog = true;
							
							NewController->StickAverageX = Win32ProcessXInputStickValue
							(
								Pad->sThumbLX,
								XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE
							);
							
							NewController->StickAverageY = Win32ProcessXInputStickValue
							(	
								Pad->sThumbLY, 
								XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE
							);
							
							if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
							{
								NewController->StickAverageY = 1.0f;
							}
							
							if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
							{
								NewController->StickAverageY = -1.0f;
							}
							
							if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
							{
								NewController->StickAverageX = -1.0f;
							}
							
							if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
							{
								NewController->StickAverageX = 1.0f;
							}
														
							real32 Threshold = 0.5f;
							Win32ProcessXInputDigitalButton
							(
								(NewController->StickAverageX < -Threshold) ? 1 : 0, 
								&OldController->MoveLeft,	&NewController->MoveLeft, 1
							);							
							Win32ProcessXInputDigitalButton
							(
								(NewController->StickAverageX > Threshold) ? 1 : 0, 
								&OldController->MoveLeft, &NewController->MoveLeft, 1
							);
							Win32ProcessXInputDigitalButton
							(
								(NewController->StickAverageY < -Threshold) ? 1 : 0, 
								&OldController->MoveLeft, &NewController->MoveLeft, 1
							);							
							Win32ProcessXInputDigitalButton
							(
								(NewController->StickAverageY > Threshold) ? 1 : 0, 
								&OldController->MoveLeft, &NewController->MoveLeft, 1
							);							
							Win32ProcessXInputDigitalButton
							(
								Pad->wButtons, &OldController->ActionDown,
								&NewController->ActionDown, XINPUT_GAMEPAD_A
							);
							Win32ProcessXInputDigitalButton
							(
								Pad->wButtons, &OldController->ActionLeft,
								&NewController->ActionLeft, XINPUT_GAMEPAD_B
							);
							Win32ProcessXInputDigitalButton
							(
								Pad->wButtons, &OldController->ActionRight,
								&NewController->ActionRight, XINPUT_GAMEPAD_X
							);
							Win32ProcessXInputDigitalButton
							(
								Pad->wButtons, &OldController->ActionUp,
								&NewController->ActionUp, XINPUT_GAMEPAD_Y
							);
							Win32ProcessXInputDigitalButton
							(
								Pad->wButtons, &OldController->LeftShoulder,
								&NewController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER
							);
							Win32ProcessXInputDigitalButton
							(
								Pad->wButtons, &OldController->RightShoulder,
								&NewController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER
							);
							Win32ProcessXInputDigitalButton
							(
								Pad->wButtons, &OldController->Start,
								&NewController->Start, XINPUT_GAMEPAD_START
							);
							Win32ProcessXInputDigitalButton
							(
								Pad->wButtons, &OldController->Back,
								&NewController->Back, XINPUT_GAMEPAD_BACK
							);
							
						}
						else
						{
							//controller is not available
							NewController->IsConnected = false;
						}
					}

					//we need to make sure this is guarded entirely
					DWORD ByteToLock = 0;	
					DWORD TargetCursor = 0;
					DWORD BytesToWrite = 0;
					DWORD PlayCursor = 0;
					DWORD WriteCursor = 0;
					bool32 SoundIsValid = false;
					
					//todo: tighten sound logic so we know where we should be writing to and
					//can anticipate time spent in game update
					if(SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
					{
						ByteToLock = (SoundOutput.RunningSampleIndex*SoundOutput.BytesPerSample) %
							SoundOutput.SecondaryBufferSize;
					
						TargetCursor = ((PlayCursor + 
							(SoundOutput.LatencySampleCount*SoundOutput.BytesPerSample)) %
							SoundOutput.SecondaryBufferSize);
				
						if(ByteToLock > TargetCursor)
						{
							BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
							BytesToWrite += TargetCursor;
						}
						else
						{
							BytesToWrite = TargetCursor - ByteToLock;
						}
						
						SoundIsValid = true;
					}
					
					game_sound_output_buffer SoundBuffer = {};
					SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
					SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
					SoundBuffer.Samples = Samples;
					
					game_offscreen_buffer Buffer = {};
					Buffer.Memory = GlobalBackbuffer.Memory;
					Buffer.Pitch = GlobalBackbuffer.Pitch;
					Buffer.Height = GlobalBackbuffer.Height;
					Buffer.Width = GlobalBackbuffer.Width;
					
					GameUpdateAndRender(&GameMemory, NewInput, &Buffer, &SoundBuffer);
					
					if(SoundIsValid)
					{
						Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
					}
					
					win32_window_dimension Dimension = Win32GetWindowDimension(Window);
					Win32DisplayBufferInWindow(DeviceContext, &GlobalBackbuffer,
					Dimension.Width, Dimension.Height);
					
					uint64 EndCycleCount = __rdtsc();
					
					LARGE_INTEGER EndCounter;
					QueryPerformanceCounter(&EndCounter);
					
					uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
					int64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
					real32 MSPerFrame = ((1000.0f*(real32)CounterElapsed) / (real32)PerfCountFrequency);
					real32 FPS = (real32)PerfCountFrequency/(real32)CounterElapsed;
					real32 MCPF = ((real32)CyclesElapsed / 1000000.0f);

	#if 0				
					char Buffer[256];
					sprintf(Buffer,"%.02fms/f, / %.02ff/s, %.02fmc/f\n", MSPerFrame, FPS, MCPF);
					OutputDebugStringA(Buffer);
	#endif
					LastCounter = EndCounter;
					LastCycleCount = EndCycleCount;
					
					game_input *Temp = NewInput;
					NewInput = OldInput;
					OldInput = Temp;
					//should these be cleared?
				}
			}
			else
			{
				
			}
		}
		else
		{
			//logging
		}
	}
	else
	{
		//logging
	}
	return 0;
}