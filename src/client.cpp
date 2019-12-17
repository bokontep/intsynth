/** @file simple_client.c
 *
 * @brief This simple client demonstrates the most basic features of JACK
 * as they would be used by many applications.
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <jack/jack.h>
#include <jack/midiport.h>
#include <pthread.h>
#ifndef ESP32
#define IRAM_ATTR 
#endif
#include "SynthVoice.h"

#define AUDIOBUFSIZE 64000
#define SAMPLE_RATE 44100
#define NUM_VOICES 4
#define NUM_DRUMS 0
#define WTLEN 256

volatile long t = 0;
volatile bool running = true;
jack_port_t *input_port;
jack_port_t *output_port;
jack_port_t *midi_input_port;
jack_client_t *client;
void* port_buf;

pthread_t thread;

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
  for(int i=0;i<128;i++)
  {
    fp_triWaveTable[i] = (int8_t)(127.0*(-1.0+i*(1.0/((double)WTLEN/2.0))));
  }
  for(int i=128;i<256;i++)
  {
    fp_triWaveTable[i] = (int8_t)(127.0*(1.0 - i*(1.0/((double)WTLEN/2.0))));
  }
  
}
void initFpSqu()
{
  for(int i=0;i<256;i++)
  {
    fp_squWaveTable[i] = (i<(WTLEN/2)?127:-127);
  }
  
}
void initFpSaw()
{
  for(int i = 0;i<256;i++)
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
/**
 * The process callback for this JACK application is called in a
 * special realtime thread once for each audio cycle.
 *
 * This client does nothing more than copy data from its input
 * port to its output port. It will exit when stopped by 
 * the user (e.g. using Ctrl-C on a unix-ish operating system)
 */
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
  initFpSaw();
  initFpSin();
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

    
    //voices[i].AddOsc1WaveTable(WTLEN,&fp_plsWaveTable[0]);
    voices[i].SetOsc1ADSR(10,1,1.0,1000);
    voices[i].AddOsc2WaveTable(WTLEN,&fp_sinWaveTable[0]);
    //voices[i].AddOsc1WaveTable(WTLEN,&fp_plsWaveTable[0]);
    
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
        maxnote = voices_notes[i];
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
        case 5:
        {
          for(int i=0;i<NUM_VOICES;i++)
          {
        
            voices[i].SetFmod2(data);
            
          }
        }
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
          //printMidiMessage(command,data1,data2);
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
  

int
process (jack_nframes_t nframes, void *arg)
{
	void* port_buf = jack_port_get_buffer(midi_input_port,nframes);
	jack_midi_event_t midiEvent;
	jack_nframes_t event_count = jack_midi_get_event_count(port_buf);
	if(event_count>0)
	{
		for(int i=0;i<event_count;i++)
		{
			jack_midi_event_get(&midiEvent,port_buf,i);
			
			processMidi(midiEvent.buffer,midiEvent.size);
		}
		
	}
	jack_default_audio_sample_t* out;
	out = (jack_default_audio_sample_t*)jack_port_get_buffer(output_port,nframes);
	for(int i=0;i<nframes;i++)
	{
		out[i] = getSample();	
		
	}
	return 0;      
}


/**
 * JACK calls this shutdown_callback if the server ever shuts down or
 * decides to disconnect the client.
 */
void
jack_shutdown (void *arg)
{
	exit (1);
}



int
main (int argc, char *argv[])
{
	const char **ports;
	const char *client_name = "intsynth";
	const char *server_name = NULL;
	jack_options_t options = JackNullOption;
	jack_status_t status;
	setup();
	/* open a client connection to the JACK server */

	client = jack_client_open (client_name, options, &status, server_name);
	if (client == NULL) {
		fprintf (stderr, "jack_client_open() failed, "
			 "status = 0x%2.0x\n", status);
		if (status & JackServerFailed) {
			fprintf (stderr, "Unable to connect to JACK server\n");
		}
		exit (1);
	}
	if (status & JackServerStarted) {
		fprintf (stderr, "JACK server started\n");
	}
	if (status & JackNameNotUnique) {
		client_name = jack_get_client_name(client);
		fprintf (stderr, "unique name `%s' assigned\n", client_name);
	}

	/* tell the JACK server to call `process()' whenever
	   there is work to be done.
	*/

	jack_set_process_callback (client, process, 0);

	/* tell the JACK server to call `jack_shutdown()' if
	   it ever shuts down, either entirely, or if it
	   just decides to stop calling us.
	*/

	jack_on_shutdown (client, jack_shutdown, 0);

	/* display the current sample rate. 
	 */
	
	printf ("engine sample rate: %" PRIu32 "\n",
		jack_get_sample_rate (client));

	/* create two ports */

	input_port = jack_port_register (client, "input",
					 JACK_DEFAULT_AUDIO_TYPE,
					 JackPortIsInput, 0);
	output_port = jack_port_register (client, "output",
					  JACK_DEFAULT_AUDIO_TYPE,
					  JackPortIsOutput, 0);
	midi_input_port = jack_port_register(client, "midi_in",JACK_DEFAULT_MIDI_TYPE,JackPortIsInput,0);
	if ((input_port == NULL) || (output_port == NULL) || midi_input_port==NULL) {
		fprintf(stderr, "no more JACK ports available\n");
		exit (1);
	}

	/* Tell the JACK server that we are ready to roll.  Our
	 * process() callback will start running now. */

	if (jack_activate (client)) {
		fprintf (stderr, "cannot activate client");
		exit (1);
	}

	/* Connect the ports.  You can't do this before the client is
	 * activated, because we can't make connections to clients
	 * that aren't running.  Note the confusing (but necessary)
	 * orientation of the driver backend ports: playback ports are
	 * "input" to the backend, and capture ports are "output" from
	 * it.
	 */

	ports = jack_get_ports (client, NULL, NULL,
				JackPortIsPhysical|JackPortIsOutput);
	if (ports == NULL) {
		fprintf(stderr, "no physical capture ports\n");
		exit (1);
	}

	if (jack_connect (client, ports[0], jack_port_name (input_port))) {
		fprintf (stderr, "cannot connect input ports\n");
	}

	free (ports);
	
	ports = jack_get_ports (client, NULL, NULL,
				JackPortIsPhysical|JackPortIsInput);
	if (ports == NULL) {
		fprintf(stderr, "no physical playback ports\n");
		exit (1);
	}

	if (jack_connect (client, jack_port_name (output_port), ports[0])) {
		fprintf (stderr, "cannot connect output ports\n");
	}

	free (ports);

	/* keep running until stopped by the user */

	sleep (-1);

	/* this is never reached but if the program
	   had some other way to exit besides being killed,
	   they would be important to call.
	*/

	jack_client_close (client);
	exit (0);
}

