/*
 Copyright (c) 2014, OpenEmu Team

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
     * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
     * Neither the name of the OpenEmu Team nor the
       names of its contributors may be used to endorse or promote products
       derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY OpenEmu Team ''AS IS'' AND ANY
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL OpenEmu Team BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "BlissGameCore.h"
#import <IOKit/hid/IOHIDLib.h>
#import <OpenEmuBase/OERingBuffer.h>
#import <OpenGL/gl.h>
#import "OEIntellivisionSystemResponderClient.h"

#import "core/Emulator.h"
#import "core/rip/Rip.h"
#import "core/audio/AudioMixer.h"
#import "core/video/VideoBus.h"
#import "drivers/intv/HandController.h"

#define INTV_IMAGE_WIDTH	(160)
#define INTV_IMAGE_HEIGHT	(192)

#define KEYBOARD_OBJECT_COUNT 256
#define AUDIO_SAMPLE_RATE 48000

#define INTY_TEST(bits, flag) (bits &   flag)
#define INTY_ON(bits, flag)   (bits |=  flag)
#define INTY_OFF(bits, flag)  (bits &= ~flag)

typedef struct
{
	UINT16	keypad;
	UINT16	action;
	UINT16	disc;
} BlissController;

class BlissInputProducer : public InputProducer
{
public:
	BlissInputProducer();

	const CHAR* getName() { return "Bliss Input"; }
	void poll() {}
	INT32 getInputCount() { return 23; }
	const CHAR* getInputName(INT32) { return "Bliss Input Name"; }

	float getValue(INT32 enumeration);

	BOOL isKeyboardDevice() { return FALSE; }

	void setPlayer(CHAR playerIndex) {
		player = playerIndex;
	}

private:
	CHAR player;
};

class BlissAudioMixer : public AudioMixer
{
public:
	void		init(UINT32 sampleRate);
	void		release();
	void		flushAudio();
};

class BlissVideoBus : public VideoBus
{
public:
	void		init(UINT32 width, UINT32 height);
	void		release();
	void		render();
};

@interface BlissGameCore () <OEIntellivisionSystemResponderClient>
{
	NSLock			*_bufferLock;
	OERingBuffer		*_audioBuffer;
	unsigned char	*_videoBuffer;
	BlissAudioMixer	*_audioMixer;
	BlissVideoBus	*_videoBus;

    NSString			*_ROMName;
	Emulator			*currentEmu;
	Rip				*currentRip;
}
- (int)blissButtonForIntellivisionButton:(OEIntellivisionButton)button player:(NSUInteger)player;
@end

@implementation BlissGameCore

// Global variables because the callbacks need to access them...
static BlissGameCore *_currentCore;
static BlissController _controller[2] = {0};

#pragma mark - OpenEmu Core

/*
 OpenEmu Core internal functions
 */

- (id)init
{
    self = [super init];
    if(self != nil)
    {
        _bufferLock = [[NSLock alloc] init];

		_currentCore = self;

		_audioMixer = new BlissAudioMixer;
		_videoBus = new BlissVideoBus;
    }

    return self;
}

- (void)dealloc
{
    DLog(@"releasing/deallocating Bliss memory");

	delete _videoBus;
	_videoBus = NULL;
	delete _audioMixer;
	_audioMixer = NULL;

	_currentCore = NULL;
}

- (void)executeFrame
{
    //DLog(@"Executing");

	// run the emulation
	currentEmu->Run();

	// render and display the video
	currentEmu->Render();

	// flush the audio
	currentEmu->FlushAudio();
}

#pragma mark - Bliss Core Helpers

- (BOOL)LoadRip:(const char*)filename
{
	char cfgFilename[PATH_MAX] = {0};
	NSString *cfgString = [[NSBundle bundleForClass:[self class]] pathForResource:@"knowncarts" ofType:@"cfg" inDirectory:@""];
	strncpy( cfgFilename, cfgString.UTF8String, sizeof(cfgFilename) );

	if (!cfgFilename || !cfgFilename[0])
		return FALSE;

	if (strlen(filename) < 5)
		return FALSE;

	const CHAR* extStart = filename + strlen(filename) - 4;
	if (strcmpi(extStart, ".intv") == 0 || strcmpi(extStart, ".int") == 0 || strcmpi(extStart, ".bin") == 0)
	{
		//load the bin file as a Rip
		currentRip = Rip::LoadBin(filename, cfgFilename);
		if (currentRip == NULL)
			return FALSE;
	}
	else if (strcmpi(extStart, ".a52") == 0)
	{
		//load the bin file as a Rip
		currentRip = Rip::LoadA52(filename);
		if (currentRip == NULL)
			return FALSE;

		CHAR fileSubname[MAX_PATH];
		CHAR* filenameStart = strrchr(filename, '/')+1;
		strncpy(fileSubname, filenameStart, strlen(filenameStart)-4);
		*(fileSubname+strlen(filenameStart)-4) = NULL;
	}
	else if (strcmpi(extStart, ".irom") == 0 || strcmpi(extStart, ".rom") == 0)
	{
		//load the rom file as a Rip
		currentRip = Rip::LoadRom(filename);
		if (currentRip == NULL)
			return FALSE;

		CHAR fileSubname[MAX_PATH];
		CHAR* filenameStart = strrchr(filename, '/')+1;
		strncpy(fileSubname, filenameStart, strlen(filenameStart)-4);
		*(fileSubname+strlen(filenameStart)-4) = NULL;
	}
	else if (strcmpi(extStart, ".zip") == 0)
	{
		//load the zip file as a Rip
		currentRip = Rip::LoadZip(filename, cfgFilename);
		if (currentRip == NULL)
			return FALSE;

		CHAR fileSubname[MAX_PATH];
		CHAR* filenameStart = strrchr(filename, '/')+1;
		strncpy(fileSubname, filenameStart, strlen(filenameStart)-4);
		*(fileSubname+strlen(filenameStart)-4) = NULL;
	}
	else {
		//load the designated Rip
		currentRip = Rip::LoadRip(filename);
		if (currentRip == NULL)
			return FALSE;
	}

	return TRUE;
}

- (BOOL)loadROMForPeripheral:(Peripheral*)peripheral
{
	BOOL didLoadROMs = NO;
	NSString *BIOSPath = nil;
	UINT16 count = peripheral->GetROMCount();

	for (UINT16 i = 0; i < count; i++) {
		ROM* r = peripheral->GetROM(i);
		if (r->isLoaded()) {
			didLoadROMs = YES;
			continue;
		}

		BIOSPath = [[self biosDirectoryPath] stringByAppendingString:[NSString stringWithFormat:@"/%s", r->getDefaultFileName()]];

		if ( r->load( [BIOSPath fileSystemRepresentation], r->getDefaultFileOffset() ) ) {
			didLoadROMs = YES;
		} else {
			didLoadROMs = NO;
			break;
		}
	}

	return didLoadROMs;
}

- (void)ReleasePeripheralInputs:(Peripheral*)periph
{
	UINT16 count = periph->GetInputConsumerCount();

	for (UINT16 i = 0; i < count; i++) {
		InputConsumer* nextInputConsumer = periph->GetInputConsumer(i);

		//iterate through each object on this consumer (buttons, keys, etc.)
		int iccount = nextInputConsumer->getInputConsumerObjectCount();

		for (int j = 0; j < iccount; j++) {
			InputConsumerObject* nextObject = nextInputConsumer->getInputConsumerObject(j);

			if (nextObject) {
				nextObject->clearBindings();
			}
		}
	}
}

- (void)ReleaseEmulatorInputs
{
	if ( !currentEmu )
		return;

	[self ReleasePeripheralInputs:currentEmu];
	UINT32 count = currentEmu->GetPeripheralCount();

	for (UINT32 i = 0; i < count; i++) {
		[self ReleasePeripheralInputs:currentEmu->GetPeripheral(i)];
	}
}

- (void)InitializePeripheralInputs:(Peripheral*)periph
{
	//iterate through all the emulated input consumers in the current emulator.
	//these consumers represent the emulated joysticks, keyboards, etc. that were
	//originally used to provide input to the emulated system
	UINT16 count = periph->GetInputConsumerCount();
	for (UINT16 i = 0; i < count; i++) {
		InputConsumer* nextInputConsumer = periph->GetInputConsumer(i);

		//iterate through each object on this consumer (buttons, keys, etc.)
		int iccount = nextInputConsumer->getInputConsumerObjectCount();
		for (int j = 0; j < iccount; j++) {
			InputConsumerObject* nextObject = nextInputConsumer->getInputConsumerObject(j);

			if ( nextObject ) {
				INT32 _objectids[1] = {nextObject->getDefaultEnum()};
				INT32 *objectids = _objectids;
				InputProducer** producerList = new InputProducer*[0];
				BlissInputProducer *producer = new BlissInputProducer;

				producer->setPlayer( i );
				producerList[0] = producer;
				nextObject->addBinding(producerList, objectids, 1);
				delete[] producerList;
			}
		}
	}
}

- (void)InitializeEmulatorInputs
{
	[self ReleaseEmulatorInputs];

	[self InitializePeripheralInputs:currentEmu];

	UINT32 count = currentEmu->GetPeripheralCount();

	for (UINT32 i = 0; i < count; i++) {
		[self InitializePeripheralInputs:currentEmu->GetPeripheral(i)];
	}
}

#pragma mark - OpenEmu Core

- (BOOL)loadFileAtPath:(NSString *)path error:(NSError **)error
{
    _ROMName = [path copy];

	[self stopEmulation];

	if (![self LoadRip:[path UTF8String]]) {
		return FALSE;
	}

	DLog(@"Loaded File");

	// find the currentEmulator required to run this RIP
	currentEmu = Emulator::GetEmulatorByID(currentRip->GetTargetSystemID());

	// load emulator ROMs
	if (![self loadROMForPeripheral:currentEmu]) {
		return NO;
	}

	// load peripheral ROMs
	INT32 count = currentEmu->GetPeripheralCount();
	for (INT32 i = 0; i < count; i++) {
		Peripheral* p = currentEmu->GetPeripheral(i);
		PeripheralCompatibility usage = currentRip->GetPeripheralUsage(p->GetShortName());
		if (usage == PERIPH_INCOMPATIBLE || usage == PERIPH_COMPATIBLE) {
			currentEmu->UsePeripheral(i, FALSE);
			continue;
		}

		BOOL loaded = [self loadROMForPeripheral:p];
		if (loaded) {
			//peripheral loaded, might as well use it.
			currentEmu->UsePeripheral(i, TRUE);
		}
		else if (usage == PERIPH_OPTIONAL) {
			//didn't load, but the peripheral is optional, so just skip it
			currentEmu->UsePeripheral(i, FALSE);
		}
		else {
			//usage == PERIPH_REQUIRED, but it didn't load
			return NO;
		}
	}

	[self InitializeEmulatorInputs];

	// hook up the audio and video
	currentEmu->InitVideo(_videoBus, currentEmu->GetVideoWidth(), currentEmu->GetVideoHeight());
	currentEmu->InitAudio(_audioMixer, AUDIO_SAMPLE_RATE);

	// put the RIP in the currentEmulator
	currentEmu->SetRip(currentRip);

	// finally, run everything
	currentEmu->Reset();

    return YES;
}

- (void)resetEmulation
{
	currentEmu->Reset();
}

- (void)stopEmulation
{
	if (currentEmu) {
		currentEmu->SetRip(NULL);
		currentEmu->ReleaseAudio();
		currentEmu->ReleaseVideo();
		currentEmu = NULL;
	}

	if (currentRip) {
		delete currentRip;
		currentRip = NULL;
	}

    [super stopEmulation];
}

- (IBAction)pauseEmulation:(id)sender
{
	// is the lack of -executeFrame being called good enough?
}

- (OEIntSize)bufferSize
{
    return OEIntSizeMake(INTV_IMAGE_WIDTH, INTV_IMAGE_HEIGHT);
}

- (OEIntRect)screenRect
{
    return OEIntRectMake(0, 0, INTV_IMAGE_WIDTH, INTV_IMAGE_HEIGHT);
}

- (OEIntSize)aspectSize
{
    return OEIntSizeMake(4, 3);
}

- (const void *)videoBuffer
{
    return _videoBuffer;
}

- (GLenum)pixelFormat
{
    return GL_BGRA;
}

- (GLenum)pixelType
{
    return GL_UNSIGNED_INT_8_8_8_8_REV;
}

- (GLenum)internalPixelFormat
{
    return GL_RGB8;
}

- (NSTimeInterval)frameInterval
{
	// http://spatula-city.org/~im14u2c/intv/tech/master.html
	// Actual Effective Frame Rate
	return 59.92;
}

- (NSUInteger)channelCount
{
	return 1;
}

- (double)audioSampleRate
{
    return AUDIO_SAMPLE_RATE;
}

- (NSUInteger)audioBitDepth
{
	return 16;
}

- (BOOL)saveStateToFileAtPath:(NSString *)fileName
{
	return currentEmu->SaveState( [fileName fileSystemRepresentation] );
}

- (BOOL)loadStateFromFileAtPath:(NSString *)fileName
{
	return currentEmu->LoadState( [fileName fileSystemRepresentation] );
}

- (NSData *)serializeStateWithError:(NSError **)outError
{
    NSUInteger length = currentEmu->StateSize();
    void *data = malloc(length);
    if(currentEmu->SerializeState(data, length))
    {
        return [NSData dataWithBytesNoCopy:data length:length];
    }
    else
    {
        if(outError)
        {
            *outError = [NSError errorWithDomain:OEGameCoreErrorDomain
                                            code:OEGameCoreCouldNotSaveStateError
                                        userInfo:@{
                                                   NSLocalizedDescriptionKey : @"Save state data could not be written",
                                                   NSLocalizedRecoverySuggestionErrorKey : @"The emulator could not write the state data."
                                                   }];
        }
        
        return nil;
    }
}

- (BOOL)deserializeState:(NSData *)state withError:(NSError **)outError
{
    if(currentEmu->DeserializeState([state bytes], [state length]))
    {
        return YES;
    }
    else
    {
        if(outError)
        {
            *outError = [NSError errorWithDomain:OEGameCoreErrorDomain
                                            code:OEGameCoreCouldNotLoadStateError
                                        userInfo:@{
                                                   NSLocalizedDescriptionKey : @"The save state data could not be read"
                                                   }];
        }
        return NO;
    }
}


#pragma mark -

/*
 Bliss callbacks
 */

#pragma mark Bliss Audio Mixer

void BlissAudioMixer::init(UINT32 sampleRate)
{
	int sampleInterval = (sampleRate / [_currentCore frameInterval]);

	// initialize the sampleBuffer
	AudioMixer::init( sampleRate );

	_currentCore->_audioBuffer = [_currentCore ringBufferAtIndex:0];

	if ( _currentCore->_audioBuffer ) {
		[_currentCore->_audioBuffer setLength:(sizeof(INT16) * sampleInterval * 8)];
	}
}

void BlissAudioMixer::release()
{
	AudioMixer::release();
}

void BlissAudioMixer::flushAudio()
{
	NSUInteger bytesPerSample = sizeof(INT16);
	NSUInteger bytesToWrite = sampleCount * bytesPerSample;

	[_currentCore->_bufferLock lock];
	[_currentCore->_audioBuffer write:this->sampleBuffer maxLength:bytesToWrite];
	[_currentCore->_bufferLock unlock];

	// updates buffer write position and sample count
	AudioMixer::flushAudio();
}

#pragma mark Bliss Video Bus

void BlissVideoBus::init(UINT32 width, UINT32 height)
{
	VideoBus::init( width, height );

	_currentCore->_videoBuffer = new unsigned char[256 * 256 * 4];
}

void BlissVideoBus::release()
{
	delete[] _currentCore->_videoBuffer;
	_currentCore->_videoBuffer = NULL;

	VideoBus::release();
}

void BlissVideoBus::render()
{
	VideoBus::render();

	[_currentCore->_bufferLock lock];
	memcpy( _currentCore->_videoBuffer, this->pixelBuffer, this->pixelBufferSize );
	[_currentCore->_bufferLock unlock];
}

#pragma mark Bliss Input Producer

BlissInputProducer::BlissInputProducer()
: InputProducer(GUID_SysKeyboard)
{
}

float BlissInputProducer::getValue(INT32 enumeration)
{
	char player = this->player;
	float value = 0.0f;

	if ( enumeration >= CONTROLLER_DISC_DOWN && enumeration <= CONTROLLER_DISC_UP_LEFT ) {
		value = INTY_TEST( _controller[player].disc, enumeration ) == enumeration ? 1.0f : 0.0f;
	} else if ( enumeration == CONTROLLER_ACTION_TOP || enumeration == CONTROLLER_ACTION_BOTTOM_LEFT || enumeration == CONTROLLER_ACTION_BOTTOM_RIGHT ) {
		value = INTY_TEST( _controller[player].action, enumeration ) == enumeration ? 1.0f : 0.0f;
	} else if ( enumeration >= CONTROLLER_KEYPAD_THREE && enumeration <= CONTROLLER_KEYPAD_CLEAR ) {
		value = INTY_TEST( _controller[player].keypad, enumeration ) == enumeration ? 1.0f : 0.0f;
	}

	return value;
}

#pragma mark - OEIntellivisionSystemResponderClient

- (int)blissButtonForIntellivisionButton:(OEIntellivisionButton)button player:(NSUInteger)player;
{
    int btn = -1;
	static int OEBlissIntellivisionButton[] =
	{
		CONTROLLER_DISC_UP,
		CONTROLLER_DISC_DOWN,
		CONTROLLER_DISC_LEFT,
		CONTROLLER_DISC_RIGHT,
		CONTROLLER_ACTION_TOP,
		CONTROLLER_ACTION_BOTTOM_LEFT,
		CONTROLLER_ACTION_BOTTOM_RIGHT,
		CONTROLLER_KEYPAD_ONE,
		CONTROLLER_KEYPAD_TWO,
		CONTROLLER_KEYPAD_THREE,
		CONTROLLER_KEYPAD_FOUR,
		CONTROLLER_KEYPAD_FIVE,
		CONTROLLER_KEYPAD_SIX,
		CONTROLLER_KEYPAD_SEVEN,
		CONTROLLER_KEYPAD_EIGHT,
		CONTROLLER_KEYPAD_NINE,
		CONTROLLER_KEYPAD_ZERO,
		CONTROLLER_KEYPAD_CLEAR,
		CONTROLLER_KEYPAD_ENTER
	};

	if (button < OEIntellivisionButtonCount && button >= OEIntellivisionButtonUp)
	{
		btn = OEBlissIntellivisionButton[button];
	}

	return btn;
}

- (void)setIntellivisionButton:(int)btn isDown:(BOOL)down forPlayer:(NSUInteger)player
{
	switch(btn)
	{
		case CONTROLLER_DISC_DOWN:
		case CONTROLLER_DISC_RIGHT:
		case CONTROLLER_DISC_UP:
		case CONTROLLER_DISC_LEFT: {
			_controller[player-1].disc = down ?
				INTY_ON( _controller[player-1].disc, btn ) :
				INTY_OFF( _controller[player-1].disc, btn );
			// if both horizontal + vertical disc directions are active,
			// turn on the wide bit flag for 45-degree angles
			if ( (_controller[player-1].disc & (CONTROLLER_DISC_LEFT|CONTROLLER_DISC_RIGHT) ) &&
				( _controller[player-1].disc & (CONTROLLER_DISC_UP|CONTROLLER_DISC_DOWN) ) ) {
				INTY_ON( _controller[player-1].disc, CONTROLLER_DISC_WIDE );
			} else {
				INTY_OFF( _controller[player-1].disc, CONTROLLER_DISC_WIDE );
			}
			break;
		}
		case CONTROLLER_KEYPAD_ONE:
		case CONTROLLER_KEYPAD_TWO:
		case CONTROLLER_KEYPAD_THREE:
		case CONTROLLER_KEYPAD_FOUR:
		case CONTROLLER_KEYPAD_FIVE:
		case CONTROLLER_KEYPAD_SIX:
		case CONTROLLER_KEYPAD_SEVEN:
		case CONTROLLER_KEYPAD_EIGHT:
		case CONTROLLER_KEYPAD_NINE:
		case CONTROLLER_KEYPAD_CLEAR:
		case CONTROLLER_KEYPAD_ZERO:
		case CONTROLLER_KEYPAD_ENTER:
			_controller[player-1].keypad = down ?
				INTY_ON(_controller[player-1].keypad, btn) :
				INTY_OFF(_controller[player-1].keypad, btn);
			break;
		case CONTROLLER_ACTION_TOP:
		case CONTROLLER_ACTION_BOTTOM_LEFT:
		case CONTROLLER_ACTION_BOTTOM_RIGHT:
			_controller[player-1].action = down ?
				INTY_ON(_controller[player-1].action, btn) :
				INTY_OFF(_controller[player-1].action, btn);
			break;
		default: break;
	}
}

- (oneway void)didPushIntellivisionButton:(OEIntellivisionButton)button forPlayer:(NSUInteger)player;
{
    int btn = [self blissButtonForIntellivisionButton:button player:player];
    
	if(btn > -1) {
		[self setIntellivisionButton:btn isDown:YES forPlayer:player];
	}
}

- (oneway void)didReleaseIntellivisionButton:(OEIntellivisionButton)button forPlayer:(NSUInteger)player;
{
    int btn = [self blissButtonForIntellivisionButton:button player:player];
    
	if(btn > -1) {
		[self setIntellivisionButton:btn isDown:NO forPlayer:player];
	}
}

@end
