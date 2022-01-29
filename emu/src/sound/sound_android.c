//Copyright 2019-2020 <>< Charles Lohr under the ColorChord License, MIT/x11 license or NewBSD Licenses.
// This was originally to be used with rawdrawandroid

#include "sound.h"
#include "os_generic.h"
#include <pthread.h> //Using android threads not os_generic threads.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __ANDROID__

//based on https://github.com/android/ndk-samples/blob/master/native-audio/app/src/main/cpp/native-audio-jni.c

// for native audio
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <android_native_app_glue.h>
#include <android/log.h>
#include <jni.h>
#include <native_activity.h>

#define LOGI(...)  ((void)__android_log_print(ANDROID_LOG_INFO, APPNAME, __VA_ARGS__))
#define printf( x...) LOGI( x )

struct SoundDriverAndroid
{
	//Standard header - must remain.
	void (*CloseFn)( void * object );
	int (*SoundStateFn)( void * object );
	SoundCBType callback;
	short channelsPlay;
	short channelsRec;
	int sps;
	void * opaque;

	int buffsz;

	SLObjectItf engineObject;
	SLEngineItf engineEngine;
	SLRecordItf recorderRecord;
	SLObjectItf recorderObject;

	SLPlayItf playerPlay;
	SLObjectItf playerObject;
	SLObjectItf outputMixObject;
 
	SLAndroidSimpleBufferQueueItf recorderBufferQueue;
	SLAndroidSimpleBufferQueueItf playerBufferQueue;
	//unsigned recorderSize;

	int recorderBufferSizeBytes;
	int playerBufferSizeBytes;
	short * recorderBuffer;
	short * playerBuffer;
};


void bqRecorderCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
	struct SoundDriverAndroid * r = (struct SoundDriverAndroid*)context;
	r->callback( r, r->recorderBuffer, 0, r->buffsz, 0 );
	(*r->recorderBufferQueue)->Enqueue( r->recorderBufferQueue, r->recorderBuffer, r->recorderBufferSizeBytes);
}

void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
	struct SoundDriverAndroid * r = (struct SoundDriverAndroid*)context;
	r->callback( r, 0, r->playerBuffer, 0, r->buffsz );
	(*r->playerBufferQueue)->Enqueue( r->playerBufferQueue, r->playerBuffer, r->playerBufferSizeBytes);
}

static struct SoundDriverAndroid* InitAndroidSound( struct SoundDriverAndroid * r )
{
	SLresult result;
	LOGI( "Starting InitAndroidSound\n" );
	
	// create engine
	result = slCreateEngine(&r->engineObject, 0, NULL, 0, NULL, NULL);
	assert(SL_RESULT_SUCCESS == result);
	(void)result;

	// realize the engine
	result = (*r->engineObject)->Realize(r->engineObject, SL_BOOLEAN_FALSE);
	assert(SL_RESULT_SUCCESS == result);
	(void)result;

	// get the engine interface, which is needed in order to create other objects
	result = (*r->engineObject)->GetInterface(r->engineObject, SL_IID_ENGINE, &r->engineEngine);
	assert(SL_RESULT_SUCCESS == result);
	(void)result;


	///////////////////////////////////////////////////////////////////////////////////////////////////////
	if( r->channelsPlay )
	{
		LOGI("create output mix");

		SLDataFormat_PCM format_pcm ={
			SL_DATAFORMAT_PCM,
			r->channelsPlay,
			SL_SAMPLINGRATE_16,
			SL_PCMSAMPLEFORMAT_FIXED_16,
			SL_PCMSAMPLEFORMAT_FIXED_16,
			(r->channelsPlay==1)?SL_SPEAKER_FRONT_CENTER:3,
			SL_BYTEORDER_LITTLEENDIAN,
		};
		SLDataLocator_AndroidSimpleBufferQueue loc_bq_play = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
		SLDataSink source = {&loc_bq_play, &format_pcm};
		const SLInterfaceID ids[1] = {SL_IID_VOLUME};
		const SLboolean req[1] = {SL_BOOLEAN_TRUE};
		const SLInterfaceID id[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};

		result = (*r->engineEngine)->CreateOutputMix(r->engineEngine, &r->outputMixObject, 0, ids, req);
		result = (*r->outputMixObject)->Realize(r->outputMixObject, SL_BOOLEAN_FALSE);

		SLDataLocator_OutputMix loc_outmix = { SL_DATALOCATOR_OUTPUTMIX, r->outputMixObject };
		SLDataSink sink;
		sink.pFormat = &format_pcm;
		sink.pLocator = &loc_outmix;

		// create audio player
		result = (*r->engineEngine)->CreateAudioPlayer(r->engineEngine, &r->playerObject, &source, &sink, 1, id, req);
		if (SL_RESULT_SUCCESS != result) {
			LOGI( "CreateAudioPlayer failed\n" );
			return JNI_FALSE;
		}


		// realize the audio player
		result = (*r->playerObject)->Realize(r->playerObject, SL_BOOLEAN_FALSE);
		if (SL_RESULT_SUCCESS != result) {
			LOGI( "AudioPlayer Realize failed: %d\n", result );
			return JNI_FALSE;
		}

		// get the player interface
		result = (*r->playerObject)->GetInterface(r->playerObject, SL_IID_PLAY, &r->playerPlay);
		assert(SL_RESULT_SUCCESS == result);
		(void)result;

		// get the buffer queue interface
		result = (*r->playerObject)->GetInterface(r->playerObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &r->playerBufferQueue);
		assert(SL_RESULT_SUCCESS == result);
		(void)result;

		// register callback on the buffer queue
		result = (*r->playerBufferQueue)->RegisterCallback(r->playerBufferQueue, bqPlayerCallback, r);
		assert(SL_RESULT_SUCCESS == result);
		(void)result;

		LOGI( "===================== Player init ok.\n" );
	}

	if( r->channelsRec )
	{
		// configure audio source
		SLDataLocator_IODevice loc_devI = {SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT, SL_DEFAULTDEVICEID_AUDIOINPUT, NULL};
		SLDataSource audioSrc = {&loc_devI, NULL};

		// configure audio sink
		SLDataFormat_PCM format_pcm ={
			SL_DATAFORMAT_PCM,
			r->channelsRec, 
			r->sps*1000,
			SL_PCMSAMPLEFORMAT_FIXED_16,
			SL_PCMSAMPLEFORMAT_FIXED_16,
			(r->channelsRec==1)?SL_SPEAKER_FRONT_CENTER:3,
			SL_BYTEORDER_LITTLEENDIAN,
		};
		SLDataLocator_AndroidSimpleBufferQueue loc_bq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
		SLDataSink audioSnk = {&loc_bq, &format_pcm};


		const SLInterfaceID id[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
		const SLboolean req[1] = {SL_BOOLEAN_TRUE};

		result = (*r->engineEngine)->CreateAudioRecorder(r->engineEngine, &r->recorderObject, &audioSrc, &audioSnk, 1, id, req);
		if (SL_RESULT_SUCCESS != result) {
			LOGI( "CreateAudioRecorder failed\n" );
			return JNI_FALSE;
		}

		// realize the audio recorder
		result = (*r->recorderObject)->Realize(r->recorderObject, SL_BOOLEAN_FALSE);
		if (SL_RESULT_SUCCESS != result) {
			LOGI( "AudioRecorder Realize failed: %d\n", result );
			return JNI_FALSE;
		}

		// get the record interface
		result = (*r->recorderObject)->GetInterface(r->recorderObject, SL_IID_RECORD, &r->recorderRecord);
		assert(SL_RESULT_SUCCESS == result);
		(void)result;

		// get the buffer queue interface
		result = (*r->recorderObject)->GetInterface(r->recorderObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,	&r->recorderBufferQueue);
		assert(SL_RESULT_SUCCESS == result);
		(void)result;

		// register callback on the buffer queue
		result = (*r->recorderBufferQueue)->RegisterCallback(r->recorderBufferQueue, bqRecorderCallback, r);
		assert(SL_RESULT_SUCCESS == result);
		(void)result;
	}


	if( r->playerPlay )
	{
		result = (*r->playerPlay)->SetPlayState(r->playerPlay, SL_PLAYSTATE_STOPPED);
		assert(SL_RESULT_SUCCESS == result); (void)result;
		result = (*r->playerBufferQueue)->Clear(r->playerBufferQueue);
		assert(SL_RESULT_SUCCESS == result); (void)result;
		r->playerBuffer = malloc( r->playerBufferSizeBytes );
		result = (*r->playerBufferQueue)->Enqueue(r->playerBufferQueue, r->playerBuffer, r->playerBufferSizeBytes );
		assert(SL_RESULT_SUCCESS == result); (void)result;
		result = (*r->playerPlay)->SetPlayState(r->playerPlay, SL_PLAYSTATE_PLAYING);
		assert(SL_RESULT_SUCCESS == result); (void)result;
	}


	if( r->recorderRecord )
	{
		result = (*r->recorderRecord)->SetRecordState(r->recorderRecord, SL_RECORDSTATE_STOPPED);
		assert(SL_RESULT_SUCCESS == result); (void)result;
		result = (*r->recorderBufferQueue)->Clear(r->recorderBufferQueue);
		assert(SL_RESULT_SUCCESS == result); (void)result;
		// the buffer is not valid for playback yet

		r->recorderBuffer = malloc( r->recorderBufferSizeBytes );

		// enqueue an empty buffer to be filled by the recorder
		// (for streaming recording, we would enqueue at least 2 empty buffers to start things off)
		result = (*r->recorderBufferQueue)->Enqueue(r->recorderBufferQueue, r->recorderBuffer, r->recorderBufferSizeBytes );
		// the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
		// which for this code example would indicate a programming error
		assert(SL_RESULT_SUCCESS == result); (void)result;

		// start recording
		result = (*r->recorderRecord)->SetRecordState(r->recorderRecord, SL_RECORDSTATE_RECORDING);
		assert(SL_RESULT_SUCCESS == result); (void)result;
	}


	LOGI( "Complete Init Sound Android\n" );
	return r;
}

int SoundStateAndroid( void * v )
{
	struct SoundDriverAndroid * soundobject = (struct SoundDriverAndroid *)v;
	return ((soundobject->recorderObject)?1:0) | ((soundobject->playerObject)?2:0);
}

void CloseSoundAndroid( void * v )
{
	struct SoundDriverAndroid * r = (struct SoundDriverAndroid *)v;
    // destroy audio recorder object, and invalidate all associated interfaces
    if (r->recorderObject != NULL) {
        (*r->recorderObject)->Destroy(r->recorderObject);
        r->recorderObject = NULL;
        r->recorderRecord = NULL;
        r->recorderBufferQueue = NULL;
		if( r->recorderBuffer ) free( r->recorderBuffer );
    }


    if (r->playerObject != NULL) {
        (*r->playerObject)->Destroy(r->playerObject);
        r->playerObject = NULL;
        r->playerPlay = NULL;
        r->playerBufferQueue = NULL;
		if( r->playerBuffer ) free( r->playerBuffer );
    }


    // destroy engine object, and invalidate all associated interfaces
    if (r->engineObject != NULL) {
        (*r->engineObject)->Destroy(r->engineObject);
        r->engineObject = NULL;
        r->engineEngine = NULL;
    }

}


int AndroidHasPermissions(const char* perm_name);
void AndroidRequestAppPermissions(const char * perm);


void * InitSoundAndroid( SoundCBType cb, int reqSPS, int reqChannelsRec, int reqChannelsPlay, int sugBufferSize, const char * inputSelect, const char * outputSelect )
{
	struct SoundDriverAndroid * r = (struct SoundDriverAndroid *)malloc( sizeof( struct SoundDriverAndroid ) );
	memset( r, 0, sizeof( *r) );
	r->CloseFn = CloseSoundAndroid;
	r->SoundStateFn = SoundStateAndroid;
	r->callback = cb;
	r->channelsPlay = reqChannelsPlay;
	r->channelsRec = reqChannelsRec;
	r->sps = reqSPS;
	
	r->recorderBufferSizeBytes = sugBufferSize * 2 * r->channelsRec;
	r->playerBufferSizeBytes = sugBufferSize * 2 * r->channelsPlay;

	int hasperm = AndroidHasPermissions( "RECORD_AUDIO" );
	if( !hasperm )
	{
		AndroidRequestAppPermissions( "RECORD_AUDIO" );
	}
	
	r->buffsz = sugBufferSize;

	return InitAndroidSound(r);
}

//Tricky: On Android, this can't actually run before main.  Have to manually execute it.

REGISTER_SOUND( AndroidSound, 10, "ANDROID", InitSoundAndroid );

#endif