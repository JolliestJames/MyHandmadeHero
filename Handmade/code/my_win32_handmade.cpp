/*------------------------------------------------------------------
	$File: $
	$Date: $
	$Revision: $
	$Creator: James Martinez
	$Notice: This is ambitious
------------------------------------------------------------------*/

#include <windows.h>
#include <stdint.h>
#include <xinput.h>
#include <dsound.h>

//implement sine ourselves so we can get rid of math.h
#include <math.h>

#define internal static
#define local_persist static
#define global_variable static

#define Pi 3.14159265359

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

struct win32_offscreen_buffer
{
	BITMAPINFO Info;
	void *Memory;
	int Width;
	int Height;
	int Pitch;
	int BytesPerPixel;
};


struct win32_window_dimension
{
	int Width;
	int Height;
};

global_variable bool GlobalRunning;
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

internal void
Win32_Load_X_Input(void)
{
	HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
	if(!XInputLibrary)
	{
		//diagnostic
		HMODULE XInputLibrary = LoadLibraryA("xinput1_3.dll");
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
RenderWeirdGradient(win32_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
{
	uint8 *Row = (uint8 *)Buffer->Memory;
	
	for(int Y = 0; Y < Buffer->Height; ++Y)
	{
		uint32 *Pixel = (uint32 *)Row;
		
		for(int X = 0; X < Buffer->Width; ++X)
		{
			
			uint8 Blue = (X + BlueOffset);
			uint8 Green = (Y + GreenOffset);
		
			*Pixel++ = ((Green << 8) | Blue);
			
		}
		Row += Buffer->Pitch;
	}
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
	Buffer->BytesPerPixel = 4;
	
	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->Width;
	Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;

	int BitmapMemorySize = (Buffer->Width*Buffer->Height)*Buffer->BytesPerPixel;
	
	Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
	
	Buffer->Pitch = Width*Buffer->BytesPerPixel;
}

internal void
Win32DisplayBufferInWindow(HDC DeviceContext, win32_offscreen_buffer *Buffer,
						int WindowWidth, int WindowHeight)
{
	//Correct aspect ratio
	StretchDIBits(DeviceContext,
				  0, 0, WindowWidth, WindowHeight, //Window size changes,
				  0, 0, Buffer->Width, Buffer->Height, //but the back buffer size does not 
				  Buffer->Memory,
				  &Buffer->Info,
	              DIB_RGB_COLORS, SRCCOPY);
				  
}

struct win32_sound_output
{
	int SamplesPerSecond;
	int ToneHz;
	int ToneVolume;
	uint32 RunningSampleIndex;
	int WavePeriod;
	int BytesPerSample;
	int SecondaryBufferSize;
	real32 tSine;
	int LatencySampleCount;
};

void Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite)
{
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;
	
	if(SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite,
						 &Region1, &Region1Size,
						 &Region2, &Region2Size,
						 0)))
	{	

		DWORD Region1SampleCount = Region1Size/SoundOutput->BytesPerSample;
		int16 *SampleOut = (int16 *)Region1;
		
		for(DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
		{
			real32 SineValue = sinf(SoundOutput->tSine);
			int16 SampleValue = (int16)(SineValue * SoundOutput->ToneVolume);
			*SampleOut++ = SampleValue;
			*SampleOut++ = SampleValue;
			
			SoundOutput->tSine += 2.0f*Pi*1.0f/(real32)SoundOutput->WavePeriod;
			
			++SoundOutput->RunningSampleIndex;
		}
		
		DWORD Region2SampleCount = Region2Size/SoundOutput->BytesPerSample;
		SampleOut = (int16 *)Region2;
		
		for(DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
		{
			real32 SineValue = sinf(SoundOutput->tSine);
			int16 SampleValue = (int16)(SineValue * SoundOutput->ToneVolume);
			*SampleOut++ = SampleValue;
			*SampleOut++ = SampleValue;
			
			SoundOutput->tSine += 2.0f*Pi*1.0f/(real32)SoundOutput->WavePeriod;
			
			++SoundOutput->RunningSampleIndex;
		}
	
		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	
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
		case WM_DESTROY:
		{
			// NOTE: Handle this as an error - recreate window
			OutputDebugStringA("WM_DESTROY\n");
			
		} break;
		
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

		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			uint32 VKCode = wParam;
			//Either we get 1 << 30 or 0, we want 1 if the former and 0 if the latter
			bool WasDown = ((lParam & (1 << 30)) != 0); 
			bool IsDown = 	((lParam & (1 << 31)) == 0);
			
			if(WasDown != IsDown)
			{
				
				if(VKCode == 'W')
				{
					
				}
				else if(VKCode == 'A')
				{
					
				}
				else if(VKCode == 'S')
				{
					
				}
				else if(VKCode == 'D')
				{
					
				}
				else if(VKCode == 'Q')
				{
					
				}
				else if(VKCode == 'E')
				{
					
				}
				else if(VKCode == VK_UP)
				{
					
				}
				else if(VKCode == VK_DOWN)
				{
					
				}
				else if(VKCode == VK_LEFT)
				{
					
				}
				else if(VKCode == VK_RIGHT)
				{
					
				}
				else if(VKCode == VK_ESCAPE)
				{
					OutputDebugStringA("ESCAPE: ");
					if(IsDown)
					{
						OutputDebugStringA("IsDown ");
					}
					if(WasDown)
					{
						OutputDebugStringA("WasDown");
					}OutputDebugStringA("\n");
				}
				else if(VKCode == VK_SPACE)
				{
					
				}
			}
	
			bool32 AltKeyWasDown = (lParam & (1 << 29));
	
			if((VKCode == VK_F4) && AltKeyWasDown)
			{
				GlobalRunning = false;
			}
			
		}break;
		
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
	Win32_Load_X_Input();
	WNDCLASS WindowClassObject = {};
	Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);
	WindowClassObject.style = CS_HREDRAW|CS_VREDRAW; //repaint the whole window, not just the new section
	
	//POINTERS DO NOT UNIQUELY IDENTIY MEMORY, ONLY MEMORY IN ONE PROCESS
	WindowClassObject.lpfnWndProc = Win32MainWindowCallback;
	WindowClassObject.hInstance = Instance;
	//WindowClass.hIcon;
	
	WindowClassObject.lpszClassName = "HandmadeHeroWindowClass";
	
	if(RegisterClassA(&WindowClassObject))
	{
		//Pass window class so windows knows that our window is going to call Win32MainWindowCallback
		//every time it gets an event
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
			
			//Graphics test
			int XOffset = 0;
			int YOffset = 0;
			
			win32_sound_output SoundOutput = {};

			//make this so long that there's no way the game could ever pause for this long so we can see where the play cursor is?
			SoundOutput.SamplesPerSecond = 48000;
			SoundOutput.ToneHz = 256;
			SoundOutput.ToneVolume = 250;
			SoundOutput.RunningSampleIndex = 0;
			SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond/SoundOutput.ToneHz;
			SoundOutput.BytesPerSample = sizeof(int16)*2;
			SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample;
			SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;
			Win32InitializeDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
			Win32FillSoundBuffer(&SoundOutput, 0, SoundOutput.SecondaryBufferSize);
			GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
		
			GlobalRunning = true;
			while(GlobalRunning)
			{
				MSG Message;
				
				while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
				{
					if(Message.message == WM_QUIT)
					{
						GlobalRunning = false;
					}
					
					TranslateMessage(&Message);
					DispatchMessageA(&Message);
				}
				
				//should we poll this more frequently
				for(DWORD ControllerIndex = 0;	ControllerIndex < XUSER_MAX_COUNT; ++ControllerIndex)
				{
					XINPUT_STATE ControllerState;

					//when we call XInputGetState, if no controller is plugged in for an index, XINPUT will stall
					//for several millis. THIS IS REALLY BAD, WE ONLY WANT TO PULL THE CONNECTED CONTROLLERS
					if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
					{
						//controller is plugged in
						//see if controllerstate.dwpacketnumber increments too rapidly
						XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;
						
						bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
						bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
						bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
						bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
						bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
						bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
						bool LeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);         
						bool RightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
						bool AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
						bool BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);
						bool XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
						bool YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);
					
						int16 StickX = Pad->sThumbLX;
						int16 StickY = Pad->sThumbLY;
						
						XOffset += StickX >> 12;
						YOffset += StickY >> 12;
						
						SoundOutput.ToneHz = 512 + ((int)256.0f*((real32)StickY/30000.0f));
						SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond/SoundOutput.ToneHz;
						
					}
					else
					{
						//controller is not available
					}
				}
				
				RenderWeirdGradient(&GlobalBackbuffer, XOffset, YOffset);
			
				DWORD PlayCursor;
				DWORD WriteCursor;

				//directsound output test
				if(SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
				{
					//the byte in the buffer where we'll actually be
					DWORD ByteToLock = (SoundOutput.RunningSampleIndex*SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;
					DWORD BytesToWrite;
					
					//Change this to use a lower latency offset from the play cursor for when we add sound effects
					if(ByteToLock > PlayCursor)
					{
						BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
						BytesToWrite += PlayCursor;
					}
					else
					{
						BytesToWrite = PlayCursor - ByteToLock;
					}
	
					Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite);
				
				}
				
				win32_window_dimension Dimension = Win32GetWindowDimension(Window);
				Win32DisplayBufferInWindow(DeviceContext, &GlobalBackbuffer,
									Dimension.Width, Dimension.Height);
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
	return(0);
}