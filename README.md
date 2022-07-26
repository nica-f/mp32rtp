# mp32rtp

This is an import of the (now lost) CVS repository that I still had on my
harddisk - including the original CVS/ subdir. I put this here for
horistorical puposes and also because it still works! The code here is from
2007 and still compiles flawlessly on 2022 Linux.

 0.   About

 1.   Usage Overview

 2.   Environment supported

 3.   Usage

3.1. Compiling and installing

3.2. Setting up RTP receiver

3.3. Playing ordinary files

3.4. Playing Internet radios

3.5. Playing folders

3.6. Setting up a server with multiple streams

3.7. Sending raw UDP streams to Exstreamer

3.8. Options for RTP receiver testing

 4. Licensing

## 0. About

mp32rtp by Barix AG, Switzerland is a Free Software which takes any MPEG audio
stream on the input and transmits it as RTP to unicast or broadcast IP address.
It's possible to transmit files, whole folders and Internet radios this way.

## 1. Usage overview

mp32rtp is best used with the Barix Exstreamer products, see 
http://www.barix.com
Several RTP streams, from local music folders and Internet radios can be
rebroadcast this way on home or office LAN and individual users can switch
channels on their Exstreamers with remote control without need for a running
client PC.

## 2. Environment supported

Works on Linux, OpenBSD. Under MS Windows it works under Cygwin 
(http://cygwin.com). Other Unix operating systems should work, but are not
tested. If you want to stream an Internet radio, you need wget installed.
(http://www.gnu.org/software/wget/)

## 3. Usage

### 3.1. Compiling and installing

After unpacking, type "make". Then become root (`su -`) and type `make install`.
If it doesn't work, ask karel@barix.com

### 3.2. Setting up RTP receiver

Make sure you have a RTP listener at hand. To configure Barix Exstreamer as a
RTP listener, go to Config - Streaming. Set Mode to 4, RTP receive port to the
port you choose to transmit the RTP on (even number) and we recommend to set
UDP Start Threshold to 20 000 (influences delay and jitter tolerance).

Or you can use any software for PC that has a RTP receiver capability.

Select an IP address. If you want to send to only one listener, use the IP
address of the listener. If you want to broadcast to the whole LAN, use
the LAN broadcast address (e. g. 192.168.2.255)

### 3.3. Playing ordinary files
```
mp32rtp -i IP_address -p port < song.mp3
```
example: `mp32rtp -i 192.168.2.255 -p 1234 < antibazz.mp3`

### 3.4. Playing Internet radios

If you have an Internet radio, make sure it's in MP3 format. Make sure the
URL you have points to the stream itself (i. e. Content-type: audio/mpeg), not
to a playlist (.m3u file, Content-type: audio/x-mpegurl). Make sure you
have wget installed (http://www.gnu.org/software/wget/) Then run:
```
mp3radio2rtp URL IP_address port
```
example: `mp3radio2rtp http://divbyzero.de:8000/va 192.168.2.255 1234`

Quick list of some streams to test:
http://dir.xiph.org/index.php?sgenre=&stype=MP3+audio&search=

### 3.5. Playing folders
```
mp3dir2rtp directory IP_address port
```
example: `mp3dir2rtp /home/clock/music 192.168.2.255 1234`

### 3.6. Setting up a server with multiple streams

Just run several instances at a time. Say you want to setup an office streaming
server with 10 channels: Rock, Pop, Dance Electro, Metal, Funk, Classical, and
3 Internet radios, at ports 11001-11010, which are pre-configured by default
for the Exstreamer remote control. This script will do the job:
```
mp3dir2rtp /home/clock/music/rock 192.168.2.255 11001 &
mp3dir2rtp /home/clock/music/pop 192.168.2.255 11002 &
mp3dir2rtp /home/clock/music/dance 192.168.2.255 11003 &
mp3dir2rtp /home/clock/music/electro 192.168.2.255 11004 &
mp3dir2rtp /home/clock/music/metal 192.168.2.255 11005 &
mp3dir2rtp /home/clock/music/funk 192.168.2.255 11006 &
mp3dir2rtp /home/clock/music/classical 192.168.2.255 11007 &
mp3radio2rtp http://128.40.249.115:8000/stream 192.168.2.255 11008 &
mp3radio2rtp http://87.74.1.170:8000/live 192.168.2.255 11009 &
mp3radio2rtp http://mp3.batanga.com:80/Norteno 192.168.2.255 11010 &
```
Good to put the commands into the startup script of the server so the streams
run every time the server boots up.

With an Exstreamer, you can now switch the channels just by pressing 0-9 on
the remote control. With a PC it depends on the RTP playback software you have
installed.

### 3.7 Sending UDP streams to Exstreamer

Maybe you need to send an UDP stream to Exstreamer's UDP listen port or UDP
priority listen port. In that case use mp32rtp's -u option that sends it in
a raw format suitable for this situation.

### 3.8. Options for RTP receiver testing

mp32rtp has couple testing options `- -s, -t, -j, -f`. See `mp32rtp -h` for their
explanation

### 4. Licensing

mp32rtp is (c) 2006-2007 Barix AG, Switzerland.

mp32rtp is licensed under GNU General Public License, see the COPYING file.
