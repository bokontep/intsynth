//
//  WaveTableOsc.h
//
//  Created by Nigel Redmon on 2018-10-05
//  EarLevel Engineering: earlevel.com
//  Copyright 2018 Nigel Redmon
//
//  For a complete explanation of the wavetable oscillator and code,
//  read the series of articles by the author, starting here:
//  www.earlevel.com/main/2012/05/03/a-wavetable-oscillator—introduction/
//
//  This version has optimizations described here:
//  www.earlevel.com/main/2019/04/28/wavetableosc-optimized/
//
//  License:
//
//  This source code is provided as is, without warranty.
//  You may copy and distribute verbatim copies of this document.
//  You may modify and use this source code to create binary code for your own purposes, free or commercial.
//

#ifndef NumWaveTableOsc_h
#define NumWaveTableOsc_h
#ifdef ARDUINO
#include <Arduino.h>
#endif
#include "Num.h"
#include "Util.h"
using namespace Fixie;

class IRAM_ATTR NumWaveTableOsc {
public:
    NumWaveTableOsc(void) {
        for (int idx = 0; idx < numWaveTableSlots; idx++) {
            
            mWaveTables[idx].waveTableLen = 0;
            mWaveTables[idx].waveTable = 0;
        }
    }
    ~NumWaveTableOsc(void) {
        /*
        for (int idx = 0; idx < numWaveTableSlots; idx++) {
            int8_t *temp = mWaveTables[idx].waveTable;
            if (temp != 0)
                delete [] temp;
        }
        */
    }



    
    void SetFrequency(double freq, double sampleRate)
    {
      SetFrequency (Num(freq/sampleRate));
    }
    //
    // SetFrequency: Set normalized frequency, typically 0-0.5 (must be positive and less than 1!)
    //
    void SetFrequency(Num inc) {
        mPhaseInc = inc;

        // update the current wave table selector

    }

    //
    // SetPhaseOffset: Phase offset for PWM, 0-1
    //
    void SetPhaseOffset(Num offset) {
        mPhaseOfs = offset;
    }

    //
    // UpdatePhase: Call once per sample
    //
    void UpdatePhase(void) {
        mPhasor += mPhaseInc;

        if (mPhasor >= Num(1.0))
            mPhasor -= Num(1.0);
    }

    //
    // Process: Update phase and get output
    //
    Num Process(void) {
        UpdatePhase();
        if(mPhaseOfs!=Num(0))
        {
          return GetOutputMinusOffset(); 
        }
        else
        {
          return GetOutput();
        }
    }

    //
    // GetOutput: Returns the current oscillator output
    //
    Num GetOutput(void) {
        waveTable *waveTable = &mWaveTables[mCurWaveTable];

        // linear interpolation
        Num temp = mPhasor *  (waveTable->waveTableLen);
        Num intPart = Fixie::Util::floor(temp);
        Num fracPart = temp - intPart;
        Num samp0 = Num(waveTable->waveTable[(int)intPart]);
        Num samp1 = Num(waveTable->waveTable[(int)intPart + (int)Num(1)]);
        return samp0 + (samp1 - samp0) * fracPart;
    }

    //
    // getOutputMinusOffset
    //
    // for variable pulse width: initialize to sawtooth,
    // set phaseOfs to duty cycle, use this for osc output
    //
    // returns the current oscillator output
    //
    Num GetOutputMinusOffset() {
        waveTable *waveTable = &mWaveTables[mCurWaveTable];
        Num len = waveTable->waveTableLen;
        int8_t *wave = waveTable->waveTable;



        Num temp = mPhasor * len;
        Num intPart = Fixie::Util::floor(temp);
        Num fracPart = temp - intPart;
        Num samp0 = Num(wave[(int)intPart]);
        Num samp1 = Num(wave[(int)intPart+(int)Num(1)]);
        Num samp = samp0 + (samp1 - samp0) * fracPart;

        // and linear again for the offset part
        Fixie::Num offsetPhasor = mPhasor + mPhaseOfs;
        if (offsetPhasor > Num(1.0))
            offsetPhasor -= Num(1.0);
        temp = offsetPhasor * Num(len);
        intPart = temp;
        fracPart = temp - intPart;
        samp0 = wave[(int)intPart];
        samp1 = wave[(int)intPart+(int)Num(1)];
        return samp - (samp0 + (samp1 - samp0) * fracPart);
    }

    //
    // AddWaveTable
    //
    // add wavetables
    // wavetables within an oscillator can be different lengths
    //
    // returns 0 upon success, or the number of wavetables if no more room is available
    //
    

    int AddSharedWaveTable(int len, int8_t *waveTableIn) {
        if (mNumWaveTables < numWaveTableSlots) {
            mWaveTables[mNumWaveTables].waveTable = waveTableIn;
            mWaveTables[mNumWaveTables].waveTableLen = len-1;
            
            ++mNumWaveTables;

            

            return 0;
        }
        return mNumWaveTables;
    }

    int AddWaveTable(int len, int8_t *waveTableIn) {
        if (mNumWaveTables < numWaveTableSlots) {
            int8_t *waveTable = mWaveTables[mNumWaveTables].waveTable = new int8_t[len + 1];
            mWaveTables[mNumWaveTables].waveTableLen = len;
            
            ++mNumWaveTables;

            // fill in wave
            for (long idx = 0; idx < len; idx++)
                waveTable[idx] = waveTableIn[idx];
            waveTable[len] = waveTable[0];  // duplicate for interpolation wraparound

            return 0;
        }
        return mNumWaveTables;
    }

    void ResetPhase()
    {
      mPhasor = Num(0.0);
    }
    void SetWaveTable(int waveTableIdx)
    {
      this->mCurWaveTable = waveTableIdx%mNumWaveTables;
    }
    int GetWaveTableCount()
    {
      return this->mNumWaveTables;
    }
protected:
    Num mPhasor = Num(0.0);       // phase accumulator
    Num mPhaseInc = Num(0.0);     // phase increment
    Num mPhaseOfs = Num(0);     // phase offset for PWM

    // array of wavetables
    int mCurWaveTable = 0;      // current table, based on current frequency
    int mNumWaveTables = 0;     // number of wavetable slots in use
    struct waveTable {
      
        Num waveTableLen;
        int8_t *waveTable;
    };
    static constexpr int numWaveTableSlots = 256;    // simplify allocation with reasonable maximum
    waveTable mWaveTables[numWaveTableSlots];
};

#endif
