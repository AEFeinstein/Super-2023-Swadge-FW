//Copyright 2015-2020 <>< Charles Lohr under the MIT/x11, NewBSD or ColorChord License.  You choose.

#include "sound.h"
#include "os_generic.h"
#include <string.h>

#ifdef __linux__

#include <alsa/asoundlib.h>

#define BUFFERSETS 4

struct SoundDriverAlsa
{
	void (*CloseFn)( void * object );
	int (*SoundStateFn)( void * object );
	SoundCBType callback;
	short channelsPlay;
	short channelsRec;
	int sps;
	void * opaque;

	char * devRec;
	char * devPlay;

	snd_pcm_uframes_t bufsize;
	og_thread_t threadPlay;
	og_thread_t threadRec;
	snd_pcm_t *playback_handle;
	snd_pcm_t *record_handle;

	char playing;
	char recording;
};

static struct SoundDriverAlsa* InitASound( struct SoundDriverAlsa * r );

int SoundStateAlsa( void * v )
{
	struct SoundDriverAlsa * r = (struct SoundDriverAlsa *)v;
	return ((r->playing)?2:0) | ((r->recording)?1:0);
}

void CloseSoundAlsa( void * v )
{
	struct SoundDriverAlsa * r = (struct SoundDriverAlsa *)v;
	if( r )
	{
		if( r->playback_handle ) snd_pcm_close (r->playback_handle);
		if( r->record_handle ) snd_pcm_close (r->record_handle);

		if( r->threadPlay ) OGJoinThread( r->threadPlay );
		if( r->threadRec ) OGJoinThread( r->threadRec );

		OGUSleep(2000);
		if( r->devRec ) free( r->devRec );
		if( r->devPlay ) free( r->devPlay );
		free( r );
	}
}


static int SetHWParams( snd_pcm_t * handle, int * samplerate, short * channels, snd_pcm_uframes_t * bufsize, struct SoundDriverAlsa * a )
{
	int err;
	snd_pcm_hw_params_t *hw_params;
	if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
		fprintf (stderr, "cannot allocate hardware parameter structure (%s)\n",
			 snd_strerror (err));
		return -1;
	}

	if ((err = snd_pcm_hw_params_any (handle, hw_params)) < 0) {
		fprintf (stderr, "cannot initialize hardware parameter structure (%s)\n",
			 snd_strerror (err));
		goto fail;
	}

	if ((err = snd_pcm_hw_params_set_access (handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		fprintf (stderr, "cannot set access type (%s)\n",
			 snd_strerror (err));
		goto fail;
	}

	if ((err = snd_pcm_hw_params_set_format (handle, hw_params,  SND_PCM_FORMAT_S16_LE )) < 0) {
		fprintf (stderr, "cannot set sample format (%s)\n",
			 snd_strerror (err));
		goto fail;
	}

	if ((err = snd_pcm_hw_params_set_rate_near (handle, hw_params, (unsigned int*)samplerate, 0)) < 0) {
		fprintf (stderr, "cannot set sample rate (%s)\n",
			 snd_strerror (err));
		goto fail;
	}

	if ((err = snd_pcm_hw_params_set_channels (handle, hw_params, *channels)) < 0) {
		fprintf (stderr, "cannot set channel count (%s)\n",
			 snd_strerror (err));
		goto fail;
	}

	int dir = 0;
	if( (err = snd_pcm_hw_params_set_period_size_near(handle, hw_params, bufsize, &dir)) < 0 )
	{
		fprintf( stderr, "cannot set period size. (%s)\n",
			snd_strerror(err) );
		goto fail;
	}

	//NOTE: This step is critical for low-latency sound.
	int bufs = *bufsize*3;
	if( (err = snd_pcm_hw_params_set_buffer_size(handle, hw_params, bufs)) < 0 )
	{
		fprintf( stderr, "cannot set snd_pcm_hw_params_set_buffer_size size. (%s)\n",
			snd_strerror(err) );
		goto fail;
	}


	if ((err = snd_pcm_hw_params (handle, hw_params)) < 0) {
		fprintf (stderr, "cannot set parameters (%s)\n",
			 snd_strerror (err));
		goto fail;
	}

	snd_pcm_hw_params_free (hw_params);
	return 0;
fail:
	snd_pcm_hw_params_free (hw_params);
	return -2;
}


static int SetSWParams( struct SoundDriverAlsa * d, snd_pcm_t * handle, int isrec )
{
	snd_pcm_sw_params_t *sw_params;
	int err;
	//Time for software parameters:

	if( !isrec )
	{
		if ((err = snd_pcm_sw_params_malloc (&sw_params)) < 0) {
			fprintf (stderr, "cannot allocate software parameters structure (%s)\n",
				 snd_strerror (err));
			goto failhard;
		}
		if ((err = snd_pcm_sw_params_current (handle, sw_params)) < 0) {
			fprintf (stderr, "cannot initialize software parameters structure (%s) (%p)\n", 
				 snd_strerror (err), handle);
			goto fail;
		}

		int buffer_size = d->bufsize*3;
		int period_size = d->bufsize;
		printf( "PERIOD: %d  BUFFER: %d\n", period_size, buffer_size );

		if ((err = snd_pcm_sw_params_set_avail_min (handle, sw_params, period_size )) < 0) {
			fprintf (stderr, "cannot set minimum available count (%s)\n",
				 snd_strerror (err));
			goto fail;
		}
		//if ((err = snd_pcm_sw_params_set_stop_threshold(handle, sw_params, 512 )) < 0) {
		//	fprintf (stderr, "cannot set minimum available count (%s)\n",
		//		 snd_strerror (err));
		//	goto fail;
		//}
		if ((err = snd_pcm_sw_params_set_start_threshold(handle, sw_params, buffer_size - period_size )) < 0) {
			fprintf (stderr, "cannot set minimum available count (%s)\n",
				 snd_strerror (err));
			goto fail;
		}
		if ((err = snd_pcm_sw_params (handle, sw_params)) < 0) {
			fprintf (stderr, "cannot set software parameters (%s)\n",
				 snd_strerror (err));
			goto fail;
		}

		

	}

	if ((err = snd_pcm_prepare (handle)) < 0) {
		fprintf (stderr, "cannot prepare audio interface for use (%s)\n",
			 snd_strerror (err));
		goto fail;
	}

	return 0;
fail:
	if( !isrec )
	{
		snd_pcm_sw_params_free (sw_params);
	}
failhard:
	return -1;
}

#if 0
static void record_callback (snd_async_handler_t *ahandler)
{
	snd_pcm_t *handle = snd_async_handler_get_pcm(ahandler);
	struct SoundDriverAlsa *data = (struct SoundDriverAlsa*)snd_async_handler_get_callback_private(ahandler);
	int err;
	snd_pcm_sframes_t avail;
	avail = snd_pcm_avail_update(handle);
	short samples[avail];

printf( "READ: %d %d\n", 0, avail );
	err = snd_pcm_readi(handle, samples, avail);
printf( "READ: %d %d\n", err, avail );
	if (err < 0) {
		printf("Read error: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}
	if (err != avail) {
		printf("Read error: written %i expected %li\n", err, avail);
		exit(EXIT_FAILURE);
	}
	data->recording = 1;
	data->callback( (struct SoundDriver *)data, samples, 0, avail, 0 );
}
#endif
void * RecThread( void * v )
{
	struct SoundDriverAlsa * r = (struct SoundDriverAlsa *)v;
	short samples[r->bufsize * r->channelsRec];
	snd_pcm_start(r->record_handle);
	do
	{
		int err = snd_pcm_readi( r->record_handle, samples, r->bufsize );	
		if( err < 0 )
		{
			fprintf( stderr, "Warning: Sound Recording Failed\n" );
			break;
		}
		if( err != r->bufsize )
		{
			fprintf( stderr, "Warning: Sound Recording Underflow\n" );
		}
		r->recording = 1;
		r->callback( (struct SoundDriver *)r, samples, 0, err, 0 );
	} while( 1 );
	r->recording = 0;
	return 0;
}


#if 0

static void playback_callback (snd_async_handler_t *ahandler)
{
	snd_pcm_t *handle = snd_async_handler_get_pcm(ahandler);
	struct SoundDriverAlsa *data = (struct SoundDriverAlsa*)snd_async_handler_get_callback_private(ahandler);
	short samples[data->bufsize];
	int err;
	snd_pcm_sframes_t avail;

	avail = snd_pcm_avail_update(handle);
printf( "WRITE: %d %d\n", 0, avail );
	while (avail >= data->bufsize)
	{
		data->callback( (struct SoundDriver *)data, 0, samples, 0, data->bufsize );
		err = snd_pcm_writei(handle, samples, data->bufsize);
		if (err < 0) {
			printf("Write error: %s\n", snd_strerror(err));
			exit(EXIT_FAILURE);
		}
		if (err != data->bufsize) {
			printf("Write error: written %i expected %li\n", err, data->bufsize);
			exit(EXIT_FAILURE);
		}
		avail = snd_pcm_avail_update(handle);
		data->playing = 1;
    }
}
#endif

void * PlayThread( void * v )
{
	struct SoundDriverAlsa * r = (struct SoundDriverAlsa *)v;
	short samples[r->bufsize * r->channelsPlay];
	int err;
	//int total_avail = snd_pcm_avail(r->playback_handle);

	snd_pcm_start(r->playback_handle);
	r->callback( (struct SoundDriver *)r, 0, samples, 0, r->bufsize );
	err = snd_pcm_writei(r->playback_handle, samples, r->bufsize);


	while( err >= 0 )
	{
	//	int avail = snd_pcm_avail(r->playback_handle);
	//	printf( "avail: %d\n", avail );
		r->callback( (struct SoundDriver *)r, 0, samples, 0, r->bufsize );
		err = snd_pcm_writei(r->playback_handle, samples, r->bufsize);
		if( err != r->bufsize )
		{
			fprintf( stderr, "Warning: Sound Playback Overflow\n" );
		}
		r->playing = 1;
	}
	r->playing = 0;
	fprintf( stderr, "Warning: Sound Playback Stopped\n" );
	return 0;
}

static struct SoundDriverAlsa * InitASound( struct SoundDriverAlsa * r )
{
	printf( "...%p %p   %d %d %d\n", r->playback_handle, r->record_handle, r->sps, r->channelsRec, r->channelsPlay );

	int err;
	if( r->channelsPlay )
	{
		if ((err = snd_pcm_open (&r->playback_handle, r->devPlay?r->devPlay:"default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
			fprintf (stderr, "cannot open output audio device (%s)\n", 
				 snd_strerror (err));
			goto fail;
		}
	}

	if( r->channelsRec )
	{
		if ((err = snd_pcm_open (&r->record_handle, r->devRec?r->devRec:"default", SND_PCM_STREAM_CAPTURE, 0)) < 0) {
			fprintf (stderr, "cannot open input audio device (%s)\n", 
				 snd_strerror (err));
			goto fail;
		}
	}

	printf( "%p %p\n", r->playback_handle, r->record_handle );

	if( r->playback_handle )
	{
		if( SetHWParams( r->playback_handle, &r->sps, &r->channelsPlay, &r->bufsize, r ) < 0 ) 
			goto fail;
		if( SetSWParams( r, r->playback_handle, 0 ) < 0 )
			goto fail;
	}

	if( r->record_handle )
	{
		if( SetHWParams( r->record_handle, &r->sps, &r->channelsRec, &r->bufsize, r ) < 0 )
			goto fail;
		if( SetSWParams( r, r->record_handle, 1 ) < 0 )
			goto fail;
	}

#if 0
	if( r->playback_handle )
	{
		snd_async_handler_t *pcm_callback;
		//Handle automatically cleaned up when stream closed.
		err = snd_async_add_pcm_handler(&pcm_callback, r->playback_handle, playback_callback, r);
		if(err < 0)
		{
			printf("Playback callback handler error: %s\n", snd_strerror(err));
		}
	}

	if( r->record_handle )
	{
		snd_async_handler_t *pcm_callback;
		//Handle automatically cleaned up when stream closed.
		err = snd_async_add_pcm_handler(&pcm_callback, r->record_handle, record_callback, r);
		if(err < 0)
		{
			printf("Record callback handler error: %s\n", snd_strerror(err));
		}
	}
#endif

	if( r->playback_handle && r->record_handle )
	{
		err = snd_pcm_link ( r->playback_handle, r->record_handle );
		if(err < 0)
		{
			printf("snd_pcm_link error: %s\n", snd_strerror(err));
		}
	}

	if( r->playback_handle )
	{
		r->threadPlay = OGCreateThread( PlayThread, r );
	}

	if( r->record_handle )
	{
		r->threadRec = OGCreateThread( RecThread, r );
	}

	printf( ".2.%p %p   %d %d %d\n", r->playback_handle, r->record_handle, r->sps, r->channelsRec, r->channelsPlay );

	return r;

fail:
	if( r )
	{
		if( r->playback_handle ) snd_pcm_close (r->playback_handle);
		if( r->record_handle ) snd_pcm_close (r->record_handle);
		free( r );
	}
	fprintf( stderr, "Error: Sound failed to start.\n" );
	return 0;
}



void * InitSoundAlsa( SoundCBType cb, int reqSPS, int reqChannelsRec, int reqChannelsPlay, int sugBufferSize, const char * inputSelect, const char * outputSelect )
{
	struct SoundDriverAlsa * r = malloc( sizeof( struct SoundDriverAlsa ) );

	r->CloseFn = CloseSoundAlsa;
	r->SoundStateFn = SoundStateAlsa;
	r->callback = cb;

	r->sps = reqSPS;
	r->channelsPlay = reqChannelsPlay;
	r->channelsRec = reqChannelsPlay;

	r->devRec = (inputSelect)?strdup(inputSelect):0;
	r->devPlay = (outputSelect)?strdup(outputSelect):0;

	r->playback_handle = 0;
	r->record_handle = 0;
	r->bufsize = sugBufferSize;

	return InitASound(r);
}

REGISTER_SOUND( AlsaSound, 10, "ALSA", InitSoundAlsa );

#endif
