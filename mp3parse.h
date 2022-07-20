/* (c) 2006-2007 Barix AG Zurich Switzerland. http://www.barix.com
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

#ifndef __MP3_PARSE_H_
#define __MP3_PARSE_H_

#define MPEG_BYTES            0 /* In bytes (whole frame incl. header) */
#define MPEG_90KHZ            1 /* In 90kHz units */
#define MPEG_SAMPLERATE       2 /* In Hz */
#define MPEG_BITRATE          3 /* In kbps */
#define MPEG_COMPRESS         4 /* Extract and compress header into 16 bits */
#define MPEG_BYTES_DECOMPRESS 5 /* Like MPEG_BYTES, but from compressed
                                   extract form */
#define MPEG_90KHZ_DECOMPRESS 6 /* Like MPEG_90KHZ, but from compressed
                                   extract form */
#define MAX_MP3_FRAME 2880 /* This occurs in MPEG 2.5 Layer 2,3 at 160kbps
                              and 8kHz */

extern unsigned char *framebuf;		/* 2880 bytes buffer for MP3 frame
						 * by default initialized to linear address 0x10000
						 *
						 * you can change this value to your own buffer,
						 * but do it BEFORE using the code
						 */

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
extern unsigned mp3_parse(unsigned char *data, unsigned len,
    void(*callback)(unsigned, void *), void *context);

/* Give it a MPEG frame header and it tells you information about it. The information
 * told is selected by the "units" parameter passed. 
 * 0 returned means that the frame is invalid.
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
extern double mpeg_frame_info(unsigned char *header, int units);
extern const unsigned char wMPEGBitRate[5][16];
extern void (*id3_hook)(unsigned char *); /* Hook for processing the content of ID3 tags.
                                   Initialized to NULL which means no hook is
                                   called. Set the address of your own function
                                   here. */
/* If you feed the parser with mp3 data and suddenly your source crashes
 * and you need to restart it, call this function. The half-assembled frame
 * will be canceled. This way you prevent a frame with a valid header and
 * length, but incorrect content. */
extern void mp3_parse_reset(void);

#endif /* __MP3_PARSE_H_ */
