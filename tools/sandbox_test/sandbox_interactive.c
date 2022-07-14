#include <stdio.h>
#include <stdint.h>

#include "../hidapi.h"
#include "../hidapi.c"
#include <sys/stat.h>

#include <time.h>

#define VID 0x303a
#define PID 0x4004

hid_device * hd;
const int chunksize = 32;

// command-line arguments = file-list to watch.

struct timespec * file_timespecs;


int CheckTimespec( int argc, char ** argv )
{
	int i;
	int taint = 0;
	for( i = 1; i < argc; i++ )
	{
		struct stat sbuf;
		stat( argv[i], &sbuf);
		if( sbuf.st_mtim.tv_sec != file_timespecs[i].tv_sec || sbuf.st_mtim.tv_nsec != file_timespecs[i].tv_nsec  )
		{
			file_timespecs[i] = sbuf.st_mtim;
			taint = 1;
		}
	}

	//Note: file_timespecs[0] unused at the moment.
	return taint;
}

int main( int argc, char ** argv )
{
	hid_init();
	hd = hid_open( VID, PID, 0 );
	if( !hd ) { fprintf( stderr, "Could not open USB [interactive]\n" ); return -94; }
	file_timespecs = calloc( sizeof(struct timespec), argc );

	CheckTimespec( argc, argv );

	do
	{
		int r;

		// Disable tick.
		uint8_t rdata[128] = { 0 };
		rdata[0] = 171;
		r = hid_get_feature_report( hd, rdata, 128 );
		if( r < 0 )
		{
			do
			{
				hd = hid_open( VID, PID, 0 );
				if( !hd )
					fprintf( stderr, "Could not open USB\n" );
				else
					fprintf( stderr, "Error: hid_get_feature_report failed with error %d\n", r );

				usleep( 100000 );
			} while( !hd );

			continue;
			
		}
		int toprint = r - 2;
		write( 1, rdata + 2, toprint );

		// Check whatever else.
		int taint = CheckTimespec( argc, argv );
		if( taint )
		{
			struct timespec spec_start, spec_end;
		    clock_gettime(CLOCK_REALTIME, &spec_start );
			system( "make run" );
		    clock_gettime(CLOCK_REALTIME, &spec_end );
			uint64_t ns_start = ((uint64_t)spec_start.tv_nsec) + ((uint64_t)spec_start.tv_sec)*1000000000LL;
			uint64_t ns_end = ((uint64_t)spec_end.tv_nsec) + ((uint64_t)spec_end.tv_sec)*1000000000LL;
			printf( "Elapsed: %.3f\n", (ns_end-ns_start)/1000000000.0 );
		}
		
		usleep( 2000 );
	} while( 1 );

	hid_close( hd );
}

