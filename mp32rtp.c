/* (c) 2006-2007 Barix AG Zurich Switzerland. http://www.barix.com,
 * Written by Karel Kulhavy.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdlib.h> /* NULL */
#include <stdio.h> /* fread */
#include <unistd.h> /* Sleep */
#include <sys/types.h> /* socket */
#include <sys/socket.h> /* socket */
#include <netinet/in.h> /* struct sockaddr_in */
#include <arpa/inet.h> /* struct sockaddr_in */
#include <errno.h> /* errno */
#include <math.h>
#include <string.h> /* memcpy */
#include <sys/time.h> /* gettimeofday */

#include "mp3parse.h"

/** Maximum. Evaluates parameters up to twice! */
#ifdef MAX
#undef MAX
#endif
#define MAX(x,y) ((x)>(y)?(x):(y))
/** Minimum. Evaluates params up to twice! */
#ifdef MIN
#undef MIN
#endif
#define MIN(x,y) ((x)<(y)?(x):(y))
/** Clip a value to boundary. Evaluates params up to twice! */
#define CLIP(x,y) {if ((x)>(y)) (x)=(y); }

#define S_START 0 /* The zero state is the starting state. */
#define S_FRAME 1
#define S_TAG   2 /* The old-style "TAG" tag */
#define S_ID3   3 /* ID3 tag. Actually written for parsing only Barix ID3 tags.
                     Other ID3 tags are ignored in a primitive way. */

#define PT_MPA 14 /* RTP Payload type MPEG I/II audio */

/* First index (0-4) is: 
 * 0 MPEG 1 Layer 1
 * 1 MPEG 1 Layer 2
 * 2 MPEG 1 Layer 3
 * 3 MPEG 2(.5) Layer 1
 * 4 MPEG 2(.5) Layer 2,3
 * Second index (0-15) is the number from the MPEG header.
 * Is used also by media.c, but is not in mp3parse.h
 */
#define MPEG_FREE 0
const unsigned char wMPEGBitRate[5][16] = {{MPEG_FREE/2, 32/2, 64/2, 96/2, 128/2,
160/2, 192/2, 224/2, 256/2, 288/2, 320/2, 352/2, 384/2, 416/2, 448/2,
MPEG_FREE/2/*MPEG_BAD/2*/},
                                           {MPEG_FREE/2, 32/2, 48/2, 56/2,  64/2,
80/2,  96/2, 112/2, 128/2, 160/2, 192/2, 224/2, 256/2, 320/2, 384/2,
MPEG_FREE/2/*MPEG_BAD/2*/},
                                           {MPEG_FREE/2, 32/2, 40/2, 48/2,  56/2,
64/2,  80/2,  96/2, 112/2, 128/2, 160/2, 192/2, 224/2, 256/2, 320/2,
MPEG_FREE/2/*MPEG_BAD/2*/},
                                           {MPEG_FREE/2, 32/2, 48/2, 56/2,  64/2,
80/2,  96/2, 112/2, 128/2, 144/2, 160/2, 176/2, 192/2, 224/2, 256/2,
MPEG_FREE/2/*MPEG_BAD/2*/},
                                           {MPEG_FREE/2,  8/2, 16/2, 24/2,  32/2,
40/2,  48/2,  56/2,  64/2,  80/2,  96/2, 112/2, 128/2, 144/2,160/2,
MPEG_FREE/2/*MPEG_BAD/2*/}};

#define RTP_HSIZE 16
unsigned char rtp_buf[2880+RTP_HSIZE]={
	/* Begin of RTP header here */
	0x80 /* V=2, P=0, X=0, CC=0 */, 0x80|PT_MPA /* M=1, PT=14 - MPA */,
	0,0 /* Sequence number big endian */,
	0,0,0,0 /* Timestamp big endian*/,
	0xbe,0xef,0xba,0xbe /* SSRC identifier big endian */,
	/* End of RTP header here, begin of MPA header */
	0,0 /* Reserved */,
	0,0 /* Fragment offset big endian*/
	/* End of MPA header here */
}
; /* MAX_MP3_FRAME long */
unsigned char *framebuf=rtp_buf+RTP_HSIZE;
static const unsigned samplerates[]={ 44100, 48000, 32000};
void (*id3_hook)(unsigned char *); /* Hook for processing the content of ID3 tags.
                                 Initialized to NULL which means no hook is
                                 called. */
/* The following 3 variables are reset to zero when mp3_parse_reset()
 * is called: */
static unsigned framebuf_len; /* Indicated number of valid bytes in framebuf,
                                 except in S_TAG state, when it indicates
                                 number of bytes consumed, but no bytes
                                 are actually fed into the frame buffer. */
static unsigned char state;
static unsigned frame_length; /* Length of mp3 frame calculated from header.
                                 framebuf_len<frame_length */
int output_socket; /* Output UDP output_socket */
char *progname="mp32rtp";

/* Target specification */
struct in_addr target_addr;
unsigned dest_port=1234;
unsigned char raw_mpeg=0; /* raw_mpeg=0 sends RTP. raw_mpeg=1 sends raw MPEG
                             UDP. */

/* Perturbation */
double speedup=1; /* Should be normally 1. >1 speeds up, <1 slows it down.
                     Only for testing purpose, causes frame skipping */
double jitter=0; /* Normally should be 0. Jitter in seconds */
static unsigned seqno;
static unsigned long timestamp;


/* Tries to feed as much data from the source buffer into the destination
 * buffer. If dest is NULL, data are actually discarded.
 * Return value (OR) flags: 1 = destination is full
 *                          2 = source is empty
 * Action: dest_level, dest_length, src, and src_len get updated.
 */
static unsigned transfer_data(unsigned char *dest, unsigned *dest_level
    , unsigned dest_length, unsigned char **src, unsigned *src_len)
{
  unsigned transfer=MIN(dest_length-*dest_level,*src_len);

  if (dest) memcpy(dest+*dest_level, *src, transfer);
  *dest_level+=transfer;
  *src+=transfer;
  *src_len-=transfer;
  return (*dest_level==dest_length)|((!*src_len)<<1);
}

/* 0 returned means that the frame is invalid.
 * This is a bit black magic but should basically calculate the
 * tables which are in the standard without using much lookup tables ;-)
 * units: 
 *        MPEG_BYTES = bytes (including header).
 *        MPEG_90KHZ = 90kHz timesteps (in this
 *                     case rounded to nearest integral number of timesteps)
 *        MPEG_SAMPLERATE = samplerate (Hz)
 *        MPEG_BITRATE = bitrate (kbps)
 *        MPEG_COMPRESS = compress header information that influences frame
 *                        length in bytes or frame time duration. The compressed
 *                        form is a single 16-bit number.
 *        MPEG_BYTES_DECOMPRESS - like MPEG_BYTES but works with compressed form
 *                                in the memory from
 *                                (unsigned *)(void *)header
 *        MPEG_90KHZ_DECOMPRESS - like MPEG_
 *
 */
double mpeg_frame_info(unsigned char *header, int units)
{
  unsigned char br_index;
  unsigned char sr_index;
  unsigned char padding;
  unsigned char mpeg; /* 0 MPEG 2.5
                         1 reserved
                         2 MPEG 2
                         3 MPEG 1
                       */
  unsigned char idx; /* Index 0-4 as first index in the bitrate table */
  unsigned char sr_shift=0; /* Samplerate shift 0-2 */
  unsigned char layer; /* 0 reserved
                          1 Layer III
                          2 Layer II
                          3 Layer I
                        */
  unsigned samplerate; /* Samplerate in Hz */
  unsigned bitrate; /* Bitrate in kbps */

  if (units==MPEG_BYTES_DECOMPRESS||units==MPEG_90KHZ_DECOMPRESS){
    /* Input is compressed MPEG header extract */
    unsigned c=header[0]+(header[1]<<8);

    mpeg=c&3;
    layer=(c>>2)&3;
    br_index=(c>>4)&0xf;
    padding=(c>>8)&1;
    sr_index=(c>>9)&3;
  }else{
    /* Treat the input as MPEG header */
    mpeg=(header[1]>>3)&3; /* MPEG version index 0-3 */
    layer=(header[1]>>1)&3; /* MPEG layer 0-3 */
    br_index=(header[2]>>4)&0xf; /* Bitrate index 0-15 */
    padding=(header[2]>>1)&1;
    sr_index=(header[2]>>2)&3; /* Samplerate index 0-3 */
  }

  if (((br_index+1)&0xf)<=1||sr_index==3||mpeg==1||layer==0) return 0;
  /* Not allowed values */

  if (units==MPEG_COMPRESS){
    /* bit  0- 1 mpeg  (2)
     *      2- 3 layer (2)
     *      4- 6 bitrate table row (3)
     *      7-10 bitrate table column (4)
     *     11    padding (1)
     *     12-13 samplerate index (2)
     */
    return mpeg|(layer<<2)|(br_index<<4)|(padding<<8)|(sr_index<<9);
  }

  if (!mpeg) sr_shift=2; /* MPEG 2.5 */
  else if (mpeg==2) sr_shift=1; /* MPEG 2 */

  if (layer==3) idx=3; else idx=4; /* MPEG 2 and 2.5 */
  if (mpeg==3) idx=3-layer; /* MPEG 1 */
 
  samplerate=samplerates[sr_index]>>sr_shift;
  bitrate=wMPEGBitRate[idx][br_index]<<1;
  
  switch(units){
    case MPEG_90KHZ:
    case MPEG_90KHZ_DECOMPRESS:
      /* 90 kHz */
      /* Round to nearest. */
      {
        unsigned long constant;
        if (layer==3){
            constant=384*90000L;
        }else{
          if (mpeg<=2&&layer==1){
            constant=576*90000L;
          }else{
            constant=1152*90000L;
          }
        }
        return (constant+(double)samplerate/2)/samplerate;
        /* Returns in a double precision, a patch for better timing performance
         */
      } 
    default: /* MPEG_BYTES and MPEG_BYTES_DECOMPRESS*/
    /* Bytes */
      {
        unsigned long constant;
        if (layer==3){
          padding<<=2;
            constant=384L*1000/8;
        }else{
          if (mpeg<=2&&layer==1){
            constant=576L*1000/8;
          }else{
            constant=1152L*1000/8;
          }
        }
        return(unsigned)(constant*bitrate/samplerate)+padding;
      }
    case MPEG_SAMPLERATE:
      return samplerate;
    case MPEG_BITRATE:
      return bitrate;
  }
}

/* If you feed the parser with mp3 data and suddenly your source crashes
 * and you need to restart it, call this function. The half-assembled frame
 * will be canceled. This way you prevent a frame with a valid header and
 * length, but incorrect content. */
void mp3_parse_reset(void)
{
  framebuf_len=0;
  state=0; /* S_START */
  frame_length=0;
}

/* Gets a block of mp3 data which it processes and returns. Remembers the
 * unprocessed data and passes the data already fit for processing into
 * the callback. Callback every time gets a whole single mp3 frame.
 * If there are data without a header between frames, they are skipped
 * until next valid frame header.
 *
 * Returns number of mp3 frames completed by the data block passed.
 *
 * Zero len is allowed.
 */
unsigned mp3_parse(unsigned char *data, unsigned len,
void(*callback)(unsigned, void *), void *context)
{
  unsigned completed=0; /* Counter of completed frames */

  /* Algorithm:
   * 0) S_START
   * 1) Read 4 bytes into buffer
   * 2) decide what is: a) mp3 frame. Go to S_FRAME and set remaining to
   *                       frame_lengt-4.
   *                    b) ID3 tag: like mp3 frame, but the state is
   *                       S_TAG.
   *                    c) something else: throw away one byte from buffer,
   *                       go to S_START and go to the beginning.
   *
   * S_FRAME: copy frame_length-framebuf_len bytes into mp3 frame buffer and
   * after that,
   * call callback with the buffer, go to S_START and go to the beginning
   * S_TAG: throw away frame_length-framebuf_len bytes, after that, go to
   * S_ID3: copy frame_length-framebuf_len bytes into mp3 frame buffer and after
   * that, call a function that interprets the mp3 data.
   * S_START and go
   * to the beginning.
   */

  while(len) switch(state)
  {
    case S_START:
      /* In this state, frame_length is ignored. This state sets frame_length
       * on exit. */
      if(transfer_data(framebuf, &framebuf_len, 4, &data, &len)&1){
        /* 4 bytes are read */
        if (framebuf[0]==0xff&&(framebuf[1]&0xe0)==0xe0){
          /* We also permit MPEG 2.5 because the Micronas codec supports it. */

          /* MPEG header */
          frame_length=mpeg_frame_info(framebuf,MPEG_BYTES);
          if (!frame_length) goto desync; /* Invalid frame header */
          state=S_FRAME;
        }else if (!memcmp(framebuf,"TAG",3)){
          frame_length=128;
          state=S_TAG;
          /* ID3 tag */
        }else if (!memcmp(framebuf,"ID3",3)){
          frame_length=24;
          state=S_ID3;
          /* ID3 tag */
        }else{
          /* Desync */
desync:

          /* Discard 1 byte */
          framebuf[0]=framebuf[1];
          framebuf[1]=framebuf[2];
          framebuf[2]=framebuf[3];
          framebuf_len--;

          /* Now try whether the rest of framebuf doesn't
           * contain 0xff at all so it can be thrown away completely */
          while (framebuf_len&&framebuf[0]!=0xff){
            framebuf[0]=framebuf[1];
            framebuf[1]=framebuf[2];
            framebuf_len--;
          }
          if (!framebuf_len){
            /* framebuf is empty because didn't contain 0xff
             * - we can try discard as much of non-0xff data from the
             *   input as possible */
            while(len&&*data!=0xff){
              data++;
              len--;
            }
          }
          
          /* state remains at S_START */
        }
      }
      break;
    case S_FRAME:
    case S_ID3:
      if (transfer_data(framebuf, &framebuf_len, frame_length
          , &data, &len)&1){
        /* We have complete frame / ID3 tag. */
        if (state==S_FRAME){
          /* It's an MPEG frame */
          /* Submit the frame to the callback */
          (*callback)(framebuf_len, context);
          completed++; /* Counter of MPEG frames completed by the particular
                          mp3_parse call */

        }else{
           /*  It's an ID3 tag */
          if (id3_hook) (*id3_hook)(framebuf);
        }
        /* Consume the MPEG frame / ID3 tag */
consume:
        framebuf_len=0;
        state=S_START;
      }
      break;
    case S_TAG:
        if (transfer_data(NULL, &framebuf_len, frame_length
          , &data, &len)&1) goto consume;
      break;
      
  }
  return completed;
}

/* Sends a packet to a single client. */
void send_packet(int fd, unsigned char *buf, size_t len, struct sockaddr_in *a)
{
	ssize_t rv;


retry:
	rv=sendto(fd, buf, len, 0, 
			(struct sockaddr *)(void *)a, 
			sizeof(*a));
	if (rv<0){
		if (errno==EINTR) goto retry;
		fprintf(stderr,
				"%s: cannot send a UDP"
				" packet %lu bytes long: "
        ,progname
        ,(unsigned long)len);
		perror("");
		exit(-1);
	}
}

/* Returns in seconds */
double double_time(void)
{
	struct timeval tv;
#ifdef DST_NONE
	struct timezone tz={0,DST_NONE};
#else
	struct timezone tz={0,0};
#endif

	if (gettimeofday(&tv, &tz)<0){
		fprintf(stderr,"%s: cannot get time of day.", progname);
    exit(-1);
  }
	return (double)tv.tv_usec/1000000+tv.tv_sec;
}


/* MPEG frame callback. Data are in mp3_buf. */
void callback(unsigned len, void * context){
	struct sockaddr_in sa={AF_INET};
	static double nexttime; /* Zero - 1970 */
	static char nexttime_valid;
  static double nexttime_jittered;
	double current_time;
	double khz_90;

	/* Is it already time to send the packet? If not, wait. */
	current_time=double_time();
	if (!nexttime_valid){
		nexttime=current_time;
		nexttime_valid=1;
	}

  nexttime_jittered=nexttime+(double)random()*jitter/0x7fffffff;
	if (current_time<nexttime_jittered){
		/* Wait */
		double difference=nexttime_jittered-current_time;

		sleep(floor(difference));
		difference-=floor(difference);
		usleep(floor(difference*1000000)); /* 1000000 is already an error
						      for usleep! */

	}
	khz_90=mpeg_frame_info(framebuf, MPEG_90KHZ);
	nexttime+=khz_90/90000/speedup;

	sa.sin_addr=target_addr;
	sa.sin_port=htons(dest_port);

	rtp_buf[2]=seqno>>8;
	rtp_buf[3]=seqno;
	rtp_buf[4]=timestamp>>24;
	rtp_buf[5]=timestamp>>16;
	rtp_buf[6]=timestamp>>8;
	rtp_buf[7]=timestamp;

  if (raw_mpeg)
    send_packet(output_socket, rtp_buf+RTP_HSIZE, len, &sa);
  else
    send_packet(output_socket, rtp_buf, len+RTP_HSIZE, &sa);
	rtp_buf[1]&=0x7f; /* Turn off the marker bit */

	timestamp+=khz_90;
	seqno++;
}

int my_socket(void)
{
	int fd;
  int opt=1;

	fd=socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (fd<0){
		fprintf(stderr, "%s: cannot get a UDP output_socket: ", progname);
		perror("");
		exit(-1);
	}

  if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(int))){
    fprintf(stderr, "%s: cannot set the UDP socket to support "
        "broadcast. You will not be able to transmit to broadcast "
        "IP addresses.", progname);
  }
	return fd;
}

void my_bind(int output_socket, unsigned port)
{

	struct sockaddr_in ina={sizeof(struct sockaddr_in)}; /* To satisfy BSD */

  ina.sin_family=AF_INET;
  ina.sin_port=htons(port);
  ina.sin_addr.s_addr=INADDR_ANY;
  
	if (bind(output_socket, (const struct sockaddr *)&ina, sizeof(ina))<0){
		fprintf(stderr, "%s: cannot bind the output_socket at port %u: "
				,progname, port);
		perror("");
		exit(-1);
	}
}

void parse_opts(int argc, char **argv)
{
  int c;
  while(1){
    switch(c=getopt(argc, argv, "i:p:s:t:j:f:uh")){
      case 'h':
      fprintf(stderr,"Usage: %s IP_address port <options>\n"
          "\n"
          "-i IP_address  set the target address (may be a broadcast). Default "
            "192.168.0.255.\n"
          "-p port        set the target port. Default 1234. Accepts also 0x\n"
          "-u             use raw MPEG in UDP instead of RTP.\n"
          "-s seqno       set the initial sequence number. Accepts "
            "also the 0x format.\n"
          "-t timestamp   set the initial timestamp. Accepts also the 0x "
            "format.\n"
          "-j jitter      set random timing jitter (in second, default 0).\n"
          "-f multiplier  send faster (>1) or slower (<1). Default 1.\n"
          "\n"
          "example: %s -i 192.168.2.255 -p 1234 < song.mp3\n"
          "mp32rtp will send song.mp3 in RTP format to UDP port 1234 and "
          "IP address 192.168.2.255. You can concatenate multiple songs "
          "and sent them on stdin as well.\n\n"
          "If the port is not specified, defaults to 1234.\n"
          "\n"
          , argv[0]
          , argv[0]);
      exit(0);

      case 'i':
      inet_aton(optarg, &target_addr);
      break;

      case 'p':
      dest_port=strtoul(optarg,NULL, 0);
      break;

      case 'u':
      raw_mpeg=1;
      break;

      case 's':
      seqno=strtoul(optarg, NULL, 0);
      break;

      case 't':
      timestamp=strtoul(optarg, NULL, 0);
      break;

      case 'j':
      jitter=strtod(optarg,NULL);
      break;

      case 'f':
      speedup=strtod(optarg,NULL);
      break;

      case -1:
      return;
    }
  }
}

unsigned long getrand(void)
{
  FILE *f;
  unsigned long retval;
  int nread;

  f=fopen("/dev/urandom","r");
  if (!f){
    fprintf(stderr,"%s: cannot open /dev/urandom: ", progname);
    perror("");
    exit(-1);
  }
  nread=fread((void *)&retval, sizeof(retval), 1, f);
  if (!nread){
    fprintf(stderr,"%s: cannot read /dev/urandom\n", progname);
    exit(-1);
  }
  fclose(f);

  return retval;
}

int main(int argc, char **argv)
{
	unsigned char buf[4096];
	size_t readbytes;

  /* Set progname for error messages */
  if (argc>=1) progname=argv[0];

  /* Initialize defaults */
  inet_aton("192.168.0.255", &target_addr);
  seqno=getrand();
  timestamp=getrand();

  parse_opts(argc, argv);
	output_socket=my_socket();

	while((readbytes=fread(buf, 1, sizeof(buf), stdin))){
		mp3_parse(buf, readbytes, &callback, NULL);
	}
	return 0;
}
