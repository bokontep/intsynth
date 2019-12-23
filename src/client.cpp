/** @file simple_client.c
 *
 * @brief This simple client demonstrates the most basic features of JACK
 * as they would be used by many applications.
 */
#include "rtaudio/RtAudio.h"
#include "rtmidi/RtMidi.h"
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>


#ifndef ESP32
#define IRAM_ATTR 
#endif
#include "SynthVoice.h"

#define AUDIOBUFSIZE 64000
#define SAMPLE_RATE 44100
#define NUM_VOICES 16
#define NUM_DRUMS 0
#define WTLEN 256

volatile long t = 0;
volatile bool running = true;
RtMidiIn *midiin = 0;
RtMidiOut *midiout = 0;
RtAudio dac;



int8_t fp_sinWaveTable[WTLEN];
int8_t fp_sawWaveTable[WTLEN];
int8_t fp_triWaveTable[WTLEN];
int8_t fp_squWaveTable[WTLEN];
int8_t fp_plsWaveTable[WTLEN];
int8_t fp_rndWaveTable[WTLEN];
uint8_t dwfbuf_l[128];
uint8_t dwfbuf_r[128];

char line1[17];
char line2[17];
uint8_t dwfidx=0;
uint8_t audio_buffer[AUDIOBUFSIZE];
double f = 22.5;
int notes[] = {0,3,5,12,15,17,24,27,29};
int notelen = 9;
int noteidx = 0;
LowPass lowpass;
int voices_notes[NUM_VOICES];
int drums_notes[NUM_DRUMS];
enum controller_states{CS_OSC=0, CS_ENV,CS_AMP,CS_FIL};
int controller_state = CS_OSC;
uint8_t knob_values[4][8]; //first is state, second is knob; 
int value_pickup[8] = {0,0,0,0,0,0,0,0};
uint8_t ffreq = 127;
uint8_t fq = 0;
unsigned long lastUpdateScreenTime;
uint8_t commandByte;
uint8_t  noteByte;
uint8_t  velocityByte;
uint8_t  noteOn = 144;
int serialData;
uint8_t  command;
uint8_t  channel;
int data1;
int data2;
enum midistate{WAIT_COMMAND,WAIT_DATA1,WAIT_DATA2};
bool firsttime = true;
midistate mstate=WAIT_COMMAND;
uint8_t rotaries[4][8];
#define PI 3.1415923
void initFpSin()
{
  for(int i=0;i<WTLEN;i++)
  {
    fp_sinWaveTable[i] = (int8_t)(127*(sin(2*(PI/(float)WTLEN)*i)));
  }
  return;
}

void initFpTri()
{
  for(int i=0;i<(WTLEN/2);i++)
  {
    fp_triWaveTable[i] = (int8_t)(127.0*(-1.0+i*(1.0/((double)WTLEN/2.0))));
  }
  for(int i=(WTLEN/2);i<WTLEN;i++)
  {
    fp_triWaveTable[i] = (int8_t)(127.0*(1.0 - i*(1.0/((double)WTLEN/2.0))));
  }
  
}
void initFpSqu()
{
  for(int i=0;i<WTLEN;i++)
  {
    fp_squWaveTable[i] = (i<(WTLEN/2)?127:-127);
  }
  
}
void initFpSaw()
{
  for(int i = 0;i<WTLEN;i++)
  {
    fp_sawWaveTable[i] = (int8_t)(127*(-1.0 + (2.0/WTLEN)*i));
  }
  
}
void initFpRnd()
{
  for(int i=0;i<WTLEN;i++)
  {
    fp_rndWaveTable[i] = (int8_t)((random()*255)-127);
  }
}
void initFpPls()
{
  for(int i=0;i<WTLEN;i++)
  {
    if(i ==WTLEN/4)
    {
      fp_plsWaveTable[i] = 127;
    }
    else if(i==WTLEN-(WTLEN/4))
    {
      fp_plsWaveTable[i] = -127;
    }
    else
    {
      fp_plsWaveTable[i] = 0;
    }
  }
}

double beatlen(double bpm)
{
  return (double)60000.0/(bpm*4);
}
SynthVoice voices[NUM_VOICES];
SynthVoice drums[NUM_DRUMS];

volatile bool play = false;
volatile unsigned long t_start;
volatile unsigned long t_end;
volatile unsigned long t_diff;
volatile unsigned long t_counter = 0;
volatile double avg_time_micros = 0;
volatile int shift = 0;

void setup()
{
	for(int i=0;i<NUM_VOICES;i++)
  {
    voices_notes[i] = -1;
  }
  for(int i=0;i<NUM_DRUMS;i++)
  {
    drums_notes[i] = -1;
  }
  initFpSin();
  initFpSaw();
  initFpSqu();
  initFpTri();  
  initFpRnd();
  initFpPls();
  printf("BOKONTEP INTSYNTH (C) 2019\n");
  
  
  
  
  for(int i =0;i<NUM_VOICES;i++)
  {
    voices[i] = SynthVoice(SAMPLE_RATE);
    voices[i].AddOsc1SharedWaveTable(WTLEN,&fp_sinWaveTable[0]);
    voices[i].AddOsc1SharedWaveTable(WTLEN,&fp_sawWaveTable[0]);
    voices[i].AddOsc1SharedWaveTable(WTLEN,&fp_triWaveTable[0]);
    voices[i].AddOsc1SharedWaveTable(WTLEN,&fp_squWaveTable[0]);
    voices[i].AddOsc1SharedWaveTable(WTLEN,&fp_plsWaveTable[0]);
    voices[i].AddOsc1SharedWaveTable(WTLEN,&fp_rndWaveTable[0]);
    voices[i].SetOsc1ADSR(10,1,1.0,1000);
    
    voices[i].AddOsc2SharedWaveTable(WTLEN,&fp_sinWaveTable[0]);
    voices[i].AddOsc2SharedWaveTable(WTLEN,&fp_sawWaveTable[0]);
    voices[i].AddOsc2SharedWaveTable(WTLEN,&fp_triWaveTable[0]);
    voices[i].AddOsc2SharedWaveTable(WTLEN,&fp_squWaveTable[0]);
    voices[i].AddOsc2SharedWaveTable(WTLEN,&fp_plsWaveTable[0]);
    voices[i].AddOsc2SharedWaveTable(WTLEN,&fp_rndWaveTable[0]);
    voices[i].SetOsc2ADSR(10,1,1.0,1000);
    
  }
  
}

float getSample()
{
	  
  int64_t s = 0;
  uint8_t data=0;
  
  for(int i=0;i<NUM_VOICES;i++)
  {
    s = s + (int32_t)(voices[i].Process() + Num(127)); 
  }
  data = (s/(NUM_VOICES));
  dwfbuf_l[dwfidx]=data;
  
    //s = ((int32_t)nsinosc.Process())+128;
     //s = s+(fpsinosc[i].Process())>>16;
     //s = nsinosc.Process()>>10;
  s = 0;
  for(int i=0;i<NUM_DRUMS;i++)
  {
    s = s + (int32_t)(drums[i].Process() + Num(127));
  }
  
  t_counter++;
  return data/255.0;
}
void handleNoteOn(uint8_t channel, uint8_t note, uint8_t velocity)
{
  char buf[17];
  bool found=false;
  int maxnote = -1;
  int maxnoteidx = -1;
  if(channel !=10)
  {
    for(int i=0;i<NUM_VOICES;i++)
    {
      if(voices_notes[i]==-1)
      {
        voices_notes[i]=note;
        sprintf(buf, "note:%d vel:%d",note, velocity);
        strcpy(line1,buf);
        strcpy(line2,"");
        
        voices[i].MidiNoteOn(note,velocity);
        found = true;
        return;
      }
      if(voices_notes[i]>maxnote)
      {
        maxnote = voices_notes[i];
        maxnoteidx = i;
      }
    }
    voices_notes[maxnoteidx]=note;
    voices[maxnoteidx].MidiNoteOn(note,velocity);
  }
  else
  {
    for(int i=0;i<NUM_DRUMS;i++)
    {
      if(drums_notes[i]==-1)
      {
        drums_notes[i]=note;
        drums[i].MidiNoteOn(note,velocity);
        found = true;
        return;
      }
      if(drums_notes[i]>maxnote)
      {
        maxnote = drums_notes[i];
        maxnoteidx = i;
      }
    }
    drums_notes[maxnoteidx]=note;
    drums[maxnoteidx].MidiNoteOn(note,velocity);
  }
  
}
void handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity)
{
  if(channel!=10)
  {
    
    for(int i=0;i<NUM_VOICES;i++)
    {
      if(voices_notes[i]==note)
      {
        voices_notes[i]=-1;
        voices[i].MidiNoteOff();
        //break;
      }
    }
  }
  else
  {
    
    for(int i=0;i<NUM_DRUMS;i++)
    {
      if(drums_notes[i]==note)
      {
        drums_notes[i]=-1;
        drums[i].MidiNoteOff();
        //break;
      }
    }
  }
}

void handleRotaryData(int rotary, int state, uint8_t data, int* value_pickup)
{
  int diff = knob_values[state][rotary]-data;
  if((diff>3 || diff<-3) && value_pickup[rotary] == 1)
  {
    return;
  }
  else
  {
    value_pickup[rotary] = 0;  
  }
  knob_values[state][rotary] = data;
  switch(state)
  {
    
    case CS_OSC:
      switch(rotary)
      {
        case 0:
          for(int i=0;i<NUM_VOICES;i++)
          {
            int divisor = 127/voices[i].GetOsc1WaveTableCount();
            voices[i].MidiOsc1Wave(data/divisor);
          }
        break;
        case 1:
          for(int i=0;i<NUM_VOICES;i++)
          {
            voices[i].SetFmod1(data);
          }
        break;
        case 2:
          for(int i=0;i<NUM_VOICES;i++)
          {
            voices[i].SetOsc1PhaseOffset(data);
          }
          break;
        case 4:
          for(int i=0;i<NUM_VOICES;i++)
          {
            int divisor = 127/voices[i].GetOsc2WaveTableCount();
            voices[i].MidiOsc2Wave(data/divisor);
          }
          break;
        case 5:

        {
          for(int i=0;i<NUM_VOICES;i++)
          {
        
            voices[i].SetFmod2(data);
            
          }
        }
        break;
        case 6:
          for(int i=0;i<NUM_VOICES;i++)
          {
            voices[i].SetOsc2PhaseOffset(data);
          }
          break;
        case 7:
          for(int i=0;i<NUM_VOICES;i++)
          {
            voices[i].SetFmod3(data);
          }
        break;
      }
    break;
    case CS_ENV:
      switch(rotary)
      {
      case 0:
        for(int i=0;i<NUM_VOICES;i++)
          {
            voices[i].SetOsc1ADSR(knob_values[CS_ENV][0],knob_values[CS_ENV][1],knob_values[CS_ENV][2],knob_values[CS_ENV][3]);
          }
      break;
      case 1:
          for(int i=0;i<NUM_VOICES;i++)
          {
            voices[i].SetOsc1ADSR(knob_values[CS_ENV][0],knob_values[CS_ENV][1],knob_values[CS_ENV][2],knob_values[CS_ENV][3]);
          }
      break;
      case 2:
          for(int i=0;i<NUM_VOICES;i++)
          {
            voices[i].SetOsc1ADSR(knob_values[CS_ENV][0],knob_values[CS_ENV][1],knob_values[CS_ENV][2],knob_values[CS_ENV][3]);
          }
      break;
      case 3:
          for(int i=0;i<NUM_VOICES;i++)
          {
            voices[i].SetOsc1ADSR(knob_values[CS_ENV][0],knob_values[CS_ENV][1],knob_values[CS_ENV][2],knob_values[CS_ENV][3]);
          }
      break;
      case 4:
          for(int i=0;i<NUM_VOICES;i++)
          {
            voices[i].SetOsc2ADSR(knob_values[CS_ENV][4],knob_values[CS_ENV][5],knob_values[CS_ENV][6],knob_values[CS_ENV][7]);
          }
      break;
      case 5:
          for(int i=0;i<NUM_VOICES;i++)
          {
            voices[i].SetOsc2ADSR(knob_values[CS_ENV][4],knob_values[CS_ENV][5],knob_values[CS_ENV][6],knob_values[CS_ENV][7]);
          }
      break;
      case 6:
          for(int i=0;i<NUM_VOICES;i++)
          {
            voices[i].SetOsc2ADSR(knob_values[CS_ENV][4],knob_values[CS_ENV][5],knob_values[CS_ENV][6],knob_values[CS_ENV][7]);
          }
      break;
      case 7:
          for(int i=0;i<NUM_VOICES;i++)
          {
            voices[i].SetOsc2ADSR(knob_values[CS_ENV][4],knob_values[CS_ENV][5],knob_values[CS_ENV][6],knob_values[CS_ENV][7]);
          }
      break;
      }
    break;
    case CS_AMP:
    break;
    case CS_FIL:
      switch(rotary)
      {
        case 0: // freq
          ffreq = data;
          for(int i=0;i<NUM_VOICES;i++)
          {
            voices[i].SetFilterParameters(ffreq, fq);  
          }
        break;
        case 1: // q
          fq = data ;
          for(int i=0;i<NUM_VOICES;i++)
          {
            voices[i].SetFilterParameters(ffreq, fq);
          }
        break;
      }
    break;
  }
}

void handlePitchBend(uint8_t channel, uint8_t bendlsb, uint8_t bendmsb)
{
  if(channel!=10)
  {
    uint16_t bend = bendmsb<<7 | bendlsb;
    for(int i=0;i<NUM_VOICES;i++)
    {
      voices[i].MidiBend(bend);
    }
  }
}
void handleCC(uint8_t channel, uint8_t cc, uint8_t data, int* vpickup)
{
  switch(cc)
  {
    case 1: //Modulation
      for(int i=0;i<NUM_VOICES;i++)
      {
        voices[i].MidiMod(data);
      }
    break;
    case 2: //PWM
      for(int i=0;i<NUM_VOICES;i++)
      {
        voices[i].MidiPwm(data);
      }
     
      
    break;
    case 64: //pedal osc1 waveform
      for(int i=0;i<NUM_VOICES;i++)
      {
        voices[i].MidiOsc1Wave(data);
      }
    break;
    case 65: //portamento osc2 waveform
      for(int i=0;i<NUM_VOICES;i++)
      {
        voices[i].MidiOsc1Wave(data);
      }
    break;
    case 91: //ROTARY 1 ON UMX490
      handleRotaryData(0, controller_state,data,vpickup); 
    break;
    case 93: //ROTARY 2 ON UMX490
      handleRotaryData(1, controller_state,data,vpickup);
    break;
    case 74: //ROTARY 3 ON UMX490
      handleRotaryData(2, controller_state,data,vpickup);
    break;
    case 71: //ROTARY 4 ON UMX490
      handleRotaryData(3, controller_state,data,vpickup);
    break;
    case 73: //ROTARY 5 ON UMX490
      handleRotaryData(4, controller_state,data,vpickup);
    break;
    case 75: //ROTARY 6 ON UMX490
      handleRotaryData(5, controller_state,data,vpickup);
    break;
    case 72: //ROTARY 7 ON UMX490
      handleRotaryData(6, controller_state,data,vpickup);
    break;
    case 10: //ROTARY 8 ON UMX490
      handleRotaryData(7, controller_state,data,vpickup);
    break;
    case 97: //button 1 on UMX490
      controller_state = CS_OSC;
      for(int i=0;i<8;i++)
      {
        vpickup[i] = 1;  
      }
      
    break;
    case 96: //button 2 on UMX490
      controller_state = CS_ENV;
      for(int i=0;i<8;i++)
      {
        vpickup[i] = 1;  
      }
    break;
    case 66: //button 3 on UMX490
      controller_state = CS_AMP;
      for(int i=0;i<8;i++)
      {
        vpickup[i] = 1;  
      }
    break;
    case 67: //button 4 on UMX490
      controller_state = CS_FIL;
      for(int i=0;i<8;i++)
      {
        vpickup[i] = 1;  
      }
    break;
  }
    
  
}

/*
void processMidi(jack_midi_data_t* serialData, int len )
{
  
	commandByte = serialData[0];
	command = (commandByte>>4)&7;
	channel = commandByte & 15;
	if(len>1)
	{
		data1 = serialData[1];
	}
	if(len>2)
	{
		data2 = serialData[2];
	}
	switch (command)
	{
	
          case 0:
          //printMidiMessage(command,data1,data2);
		  printf("NOTEOFF   CH:%02d note:%03d vel:%03d\n",channel,data1,data2);
          handleNoteOff(channel,data1,data2);
          mstate = WAIT_COMMAND;
          break;
          case 1:
		  printf("NOTEON    CH:%02d note:%03d vel:%03d\n",channel,data1,data2);
          //printMidiMessage(command>,data1,data2);
          handleNoteOn(channel,data1,data2);
          mstate = WAIT_COMMAND;
          break;
          case 3:
		  printf("CC        CH:%02d note:%03d vel:%03d",channel,data1,data2);
          handleCC(channel, data1, data2,&value_pickup[0]);
          mstate = WAIT_COMMAND;
          break;
          case 6:
		  printf("PITCHBEND CH:%02d data1:%03d data2:%03d",channel,data1,data2);
          handlePitchBend(channel,data1,data2);
          mstate = WAIT_COMMAND;
          break;
		default:
		break;

    }
        
    
  }
 */
void onMidiIn(double deltatime, std::vector< unsigned char> *message, void *userData)
{
  unsigned int mSize = message->size();
  if(mSize>0)
  commandByte = message->at(0);
	command = (commandByte>>4)&7;
	channel = commandByte & 15;
	if(mSize>1)
	{
		data1 = message->at(1);
	}
	if(mSize>2)
	{
		data2 = message->at(2);
	}
	switch (command)
	{
	
          case 0:
          //printMidiMessage(command,data1,data2);
		  printf("NOTEOFF   CH:%02d note:%03d vel:%03d\n",channel,data1,data2);
          handleNoteOff(channel,data1,data2);
          mstate = WAIT_COMMAND;
          break;
          case 1:
		  printf("NOTEON    CH:%02d note:%03d vel:%03d\n",channel,data1,data2);
          //printMidiMessage(command>,data1,data2);
          handleNoteOn(channel,data1,data2);
          mstate = WAIT_COMMAND;
          break;
          case 3:
		  printf("CC        CH:%02d note:%03d vel:%03d\n",channel,data1,data2);
          handleCC(channel, data1, data2,&value_pickup[0]);
          mstate = WAIT_COMMAND;
          break;
          case 6:
		  printf("PITCHBEND CH:%02d data1:%03d data2:%03d\n",channel,data1,data2);
          handlePitchBend(channel,data1,data2);
          mstate = WAIT_COMMAND;
          break;
		default:
		break;
  }
}

int renderAudio(void* outputBuffer, void *inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void* userData)
{
  double*buffer = (double *) outputBuffer;
  for(int i=0;i<nBufferFrames;i++)
  {
    float f = getSample();
    *buffer++=f;
    
  }

  return 0;
}
void initRtAudio()
{

  int audiodevcount = dac.getDeviceCount();
  if(audiodevcount<1)
  {
    std::cout << "\nNo audio devices found!\n";
    exit(0);
  }
  else
  {
    std::cout << "\nFound " << audiodevcount << " devices!\n";
  }
  RtAudio::StreamParameters parameters;
  parameters.deviceId = dac.getDefaultOutputDevice();
  parameters.nChannels = 1;
  parameters.firstChannel = 0;
  unsigned int bufferFrames = 256;
  double data[2];
  try
  {
    dac.openStream(&parameters, NULL, RTAUDIO_FLOAT64, SAMPLE_RATE,&bufferFrames, &renderAudio, (void* )&data);
    dac.startStream();
  }
  catch(const std::exception& e)
  {
    std::cerr << e.what() << '\n';
  }
  
}
void initRtMidi(int midiport)
{
  try {
    midiin = new RtMidiIn();
  }
  catch ( RtMidiError &error ) {
    error.printMessage();
    exit( EXIT_FAILURE );
  }
  // Check inputs.
  unsigned int nPorts = midiin->getPortCount();
  std::cout << "\nThere are " << nPorts << " MIDI input sources available.\n";
  std::string portName;
  for ( unsigned int i=0; i<nPorts; i++ ) {
    try {
      portName = midiin->getPortName(i);
    }
    catch ( RtMidiError &error ) {
      error.printMessage();
      
    }
    std::cout << "  Input Port #" << i << ": " << portName << '\n';
  }
  midiin->openPort(midiport);
  midiin->setCallback(&onMidiIn);
  midiin->ignoreTypes(false,false,false);
    
}
int
main (int argc, char *argv[])
{
  int midiportnum = 0;
	setup();
  printf("%d argument(s)\n",argc);
  for(int i=0;i<argc;i++)
  {
    printf("argument %d:%s\n",i,argv[i]);
    
    if(strstr(argv[i],"/midiport=")>0)
    {
      char* midiportarg = strtok(argv[i],"=");
      midiportarg = strtok(NULL,"=");
      midiportnum = atoi(midiportarg);

    }
    printf("midiport to use = %d\n",midiportnum);




  }
  initRtAudio();
  initRtMidi(midiportnum);
  
  

  while(running)
  {
    sleep(1);
  }
  return 0;
}

