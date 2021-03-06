#ifndef LowPass_h
#define LowPass_h
#ifdef ARDUINO
#include <Arduino.h>
#endif
#include "Num.h"
using namespace Fixie;
/*
simple resonant filter posted to musicdsp.org by Paul Kellett http://www.musicdsp.org/archive.php?classid=3#259
// set feedback amount given f and q between 0 and 1
fb = q + q/(1.0 - f);
// for each sample...
buf0 = buf0 + f * (in - buf0 + fb * (buf0 - buf1));
buf1 = buf1 + f * (buf0 - buf1);
out = buf1;
Taken from mozzi
*/

IRAM_ATTR class LowPass {
public:
    LowPass() {
        
    }
    ~LowPass(void) {
        
    }
    void SetParameters(Num f, Num q)
    {
        if(f==Num(1))
        {
          f = Num(0.999);
        }
        this->f = f;
        this->q = q;
        fb = q + q/(Num(1) - f);
    }
    
    Num Process(Num in)
    {
        
        buf0 = buf0 + f * (in - buf0 + fb * (buf0 - buf1));
        buf1 = buf1 + f * (buf0 - buf1);
        return buf1;    
    }
protected:
    Num q;
    Num f;
    Num fb;
    Num buf0;
    Num buf1;
    
    
};

#endif
