#include <stdint.h>
#include <stdio.h>

#include "../../hidapi.h"
#include "../../hidapi.c"

#define VID 0x303a
#define PID 0x4004

hid_device * hd;

int main()
{
	hid_init();
	hd = hid_open( VID, PID, L"420690" );
	if( !hd ) { fprintf( stderr, "Could not open USB\n" ); return -94; }
	uint8_t data[257];
	uint8_t rdata[257];
	int survey;
	for( survey = -20; survey < 120; survey+=5 )
	{
		data[0] = 173;
		data[1] = 5;
		data[2] = survey;
		int r = hid_send_feature_report( hd, data, 4 );
		if( r <= 0 ) { fprintf( stderr, "Error: Could not create survey %d for IGI %d\n", survey,r ); }
		fprintf( stderr, "Surveying %d\n", survey );

		int lastone = 0;
		int to = 0;
		int prx = 0;
		int pigi = 0;
		int tas = 0;
		int tss = 0;
		for( to = 0; to < 200; to++ )
		{
			usleep(100000);
			rdata[0] = 173;
			memset( rdata+1, 0, sizeof(rdata)-1 );
			r = hid_get_feature_report( hd, rdata, 257 );
			if( r <= 0 ) { fprintf( stderr, "Error: Could not collect survey %d for IGI %d\n", survey, r ); }

			// Format is:
			// Packets Received (16-bit number)
			// Total RX RSSI (16-bit number)
			// Total Attempt Send (8-bit)
			// Total Send Success (8-bit)
			// int16_t QTY 100 pacet send times.  Or 255 if send unsuccessful.  Time Unit: 0.1ms
			prx = rdata[2] | (rdata[3]<<8);
			pigi = rdata[4] | (rdata[5]<<8);

			tas = rdata[6];
			tss = rdata[7];

			int i;
			//for( i = 0; i < 220; i++ ) printf( "%02x ", rdata[i] ); printf( "\n" );

			fprintf( stderr, "PRX: %d PIGI: %d TAS: %d TSS: %d\n", prx, pigi, tas, tss );
			if( lastone ) break;
			if( tas == 100 ) lastone = 1;
		}
		printf( "%d\t%d\t%d\t%d\t%d", survey, prx, pigi, tas, tss );
		int i;
		for( i = 0; i < tas; i++ )
		{
			printf( "%d%c", rdata[8+i*2] | (rdata[9+i*2]<<8), (i == tas-1)?'\n':'\t' );
		}

	}
	return 0;
}


