#if !defined(HANDMADE_H)

/*------------------------------------------------------------------
	$File: $
	$Date: $
	$Revision: $
	$Creator: James Martinez
	$Notice: This is ambitious
------------------------------------------------------------------*/

/*
	HANDMADE_INTERNAL:
		0 - Build for public release
		1 - Build for developer only
		
	HANDMADE_SLOW:
		0 - No slow code
		1 - Slow code		
*/

#include <math.h>
#include <stdint.h>

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

#if HANDMADE_SLOW
#define Assert(Expression) if(!(Expression)){*(int *) 0 = 0;}
#else
#define Assert(Expression)
#endif	

//should we always use 64-bit?
#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) (Kilobytes(Value)*1024LL)
#define Gigabytes(Value) (Megabytes(Value)*1024LL)
#define Terabytes(Value) (Gigabytes(Value)*1024LL)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

//TODO: Swap min, max, ... macros?

inline uint32
SafeTruncateUInt64(uint64 Value)
{
	Assert(Value <= 0xFFFFFFFF);
	uint32 Result = (uint32)Value;
	return(Result);
}

/* NOTE: Services that the platform layer provides to the game */

#if HANDMADE_INTERNAL

/*
	IMPORTANT:
		
	These are not for doing anything in the shipped product - they are blocking and 
	the write doesn't protect against lost data
	
	
*/

struct debug_read_file_result
{
	uint32 ContentsSize;
	void *Contents;
};

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(void *Memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) debug_read_file_result name(char *Filename)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool32 name(char *Filename, uint32 MemorySize, void *Memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);

#endif


/*
	NOTE: Services that the game provides to the platform layer 
	
	- this may expand in the future - sound on seprate thread, etc
*/


// FOUR THINGS - timing, controller/keyboard input, bitmap to output, sound buffer to output

//in the future, rendering will become a three-tiered abstraction
struct game_offscreen_buffer
{
	void *Memory;
	int Width;
	int Height;
	int Pitch;
	int BytesPerPixel;
};

struct game_sound_output_buffer
{
	int SamplesPerSecond;
	int SampleCount;
	int16 *Samples;
};

struct game_button_state
{
	int HalfTransitionCount;
	bool32 EndedDown;
};

struct game_controller_input
{
	bool32 IsConnected;
	bool32 IsAnalog;
	real32 StickAverageX;
	real32 StickAverageY;
	
	union
	{
		game_button_state Buttons[12];
	
		struct
		{
			game_button_state MoveUp;
			game_button_state MoveDown;
			game_button_state MoveLeft;
			game_button_state MoveRight;
			
			game_button_state ActionUp;
			game_button_state ActionDown;
			game_button_state ActionLeft;
			game_button_state ActionRight;
			
			game_button_state LeftShoulder;
			game_button_state RightShoulder;
			
			game_button_state Start;
			game_button_state Back;
			
			//NOTE: All buttons must be added above here
			
			game_button_state Terminator;
		};
	};
};

struct game_input
{
	//Insert clock value here
	game_controller_input Controllers[5];
};

inline game_controller_input *GetController(game_input *Input, int unsigned ControllerIndex)
{
	Assert(ControllerIndex < ArrayCount(Input->Controllers));
	
	game_controller_input *Result = &Input->Controllers[ControllerIndex];
	return(Result);
}

struct game_memory 
{
	bool32 IsInitialized;
	
	uint64 PermanentStorageSize;
	void *PermanentStorage; //required to be 0 on startup
	
	uint64 TransientStorageSize;
	void *TransientStorage; 
	
	debug_platform_free_file_memory *DEBUGPlatformFreeFileMemory;
	debug_platform_read_entire_file *DEBUGPlatformReadEntireFile;
	debug_platform_write_entire_file *DEBUGPlatformWriteEntireFile;
};

struct game_clocks
{
	//What do we want to pass here?
	real32 SecondsElapsed;
};

#define GAME_UPDATE_AND_RENDER(name) void name(game_memory *Memory, game_input *Input, game_offscreen_buffer *Buffer)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);
GAME_UPDATE_AND_RENDER(GameUpdateAndRenderStub)
{
}

//NOTE: At the moment, this has to be a ver fast function, it cannot be more than
//a millisecond or so
#define GAME_GET_SOUND_SAMPLES(name) void name(game_memory *Memory, game_sound_output_buffer *SoundBuffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);
GAME_GET_SOUND_SAMPLES(GameGetSoundSamplesStub)
{
}

//
//
//
	
struct game_state
{
	int ToneHz;
	int GreenOffset;
	int BlueOffset;
	
	real32 tSine;
	int PlayerX;
	int PlayerY;
};

#define HANDMADE_H
#endif