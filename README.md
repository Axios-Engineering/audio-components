audio-components
================

Audio Components for the REDHAWK framework.

![ScreenShot](https://github.com/Axios-Engineering/audio-components/raw/master/images/audio_example_blue_noise.png)

**GStreamer**

Many of these components use the GStreamer framework.  On
Ubuntu, the minimally necessary packages can be installed
using the command:

    sudo apt-get install libgstreamer0.10-dev \
                         gstreamer0.10-plugins-base \
                         gstreamer0.10-plugins-good
                         
AudioTestSource
---------------
Provides the GStreamer `audiotestsrc` plugin as a REDHAWK component.  Allows
for the generation of various waveforms: sine, square, saw, triangle, silence,
white noise, pink noise, ticks, gaussian white noise, red noise, blue noise,
and violet noise.

Produces 16-bit linear mono samples via a BULKIO dataShort port.

AudioSource
---------------
Uses the GStreamer `playbin` to read pretty much any audio source that
your system can support (i.e. CD, DVD, mp3, ogg, speex) including extracting
audio from video sources.

The `audio-uri` property defines the source of the audio.  For example:

* `file:///path/to/somefile.mp3` read from a file
* `cdda://4` plays track 4 from the CD

AudioSink
---------------
Renders 16-bit linear mono samples via a BULKIO dataShort port to the sound card.

Provides 3-band equalizer and volume control via properties.
