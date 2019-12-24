This is a port of the ESP32 synth to linux. I wanted to port this as a simple
no-frills synth to use under linux platforms like raspberry-pi. In order to
build this you will need cmake. You also need librtaudio and librtmidi.
To build:
1.
git clone https://github.com/bokontep/intsynth.git
cd intsynth
cmake .
make
./intsynth
To specify midi input port and audio device use /midiport=X and /audiodevice=Y
where X and Y are your midiport id and audiodevice id respectively. On startup
the app displays all midiports and audiodevices so write them down.
Enjoy!
BOKONTEP 
