/*
  This takes in bits over 433mhz from a La Crosse TX7NU temp sensor and prints on the humidity and temp. 
  It does not currently validate the parity bit, nor the repetition of the ten and one parts of the BCD data, nor the checksum
*/

#include <wiringPi.h>
#include <time.h>
#include <iostream>
#include <bitset>

using namespace std;

#define PACKET_LENGTH 44

#define LOW_PULSE 550 // microseconds
#define HIGH_PULSE 1300 // microseconds
#define LOCK 5000 // Anything > this is considered a lock

#define TOLERANCE 100 // LOW/HIGH +/- TOLERANCE, LOCK +/- TOLERANCE*10
#define HIGH_FAIL 2000 // Fail the lock if duration > HIGH_FAIL
#define LOW_FAIL 200 // Fail the lock if duration < LOW_FAIL

static bitset<PACKET_LENGTH> bits; // The bits we care about

struct Measure {
  bitset<8> init;
  bitset<4> measureID;
  bitset<7> deviceID;
  bool par;

  bitset<4> ten;
  bitset<4> one;
  bitset<4> pnt;
  
  bitset<4> checksum;

  void reset() {
    init.reset();
    measureID.reset();
    deviceID.reset();
    par = false;
    ten.reset();
    one.reset();
    pnt.reset();
    checksum.reset();
  }
};

void handleInterrupt() {
 
  static unsigned int duration;
  static unsigned int changeCount;
  static unsigned long lastTime;
  static int lockPassed;

  long time = micros();
  duration = time - lastTime;


  //wait for lock
  if (duration > LOCK && lockPassed == 0) { 
    changeCount = 0;
    lockPassed = 1;

    bits.reset();
  } 
  else if (lockPassed != 0) { // REST OF DATA

    // We're done! Print the packet
    if (changeCount >= PACKET_LENGTH || duration > HIGH_FAIL || duration < LOW_FAIL) { 

      if (changeCount >= PACKET_LENGTH) {
        // Print the message

        
        Measure reading;
        float resultValue;

        for (unsigned int i = 0; i < PACKET_LENGTH; i++) {
          // set(PART_LENGTH - 1 - i - MIN_POSITION)
          // Basically reads it in "backwards"
          if (i < 8)         reading.init.set(8 - 1 - (i - 0),  bits[i]);
          else if (i < 12)   reading.measureID.set(4 - 1 - (i - 8), bits[i]);
          else if (i < 19)   reading.deviceID.set(7 - 1 - (i - 12), bits[i]);
          else if (i < 20)   reading.par = bits[i];
          else if (i < 24)   reading.ten.set(4 - 1 - (i - 20), bits[i]);
          else if (i < 28)   reading.one.set(4 - 1 - (i - 24), bits[i]);
          else if (i < 32)   reading.pnt.set(4 - 1 - (i - 28), bits[i]);
          else if (i >= 40 && i < 44) reading.checksum.set(4 - 1 - (i - 40), bits[i]);
        }
        


        resultValue = (reading.ten.to_ulong() * 10) + (reading.one.to_ulong()) + ( (float)(reading.pnt.to_ulong()) / 10);

        // Temp: 0000
        // Humidity: 1110

        if (reading.measureID.to_ulong() == 0) {
          // Need to subtract 50 to get C value of temp, then convert to F
          resultValue = ((resultValue - 50) * 9)/5 + 32;
          cout << resultValue << 'F';
        }

        if (reading.measureID.to_ulong() == 14) {
          cout << resultValue << '%';
        }

        // This will print the message "in reverse", bitsets, low order, etc. Use a loop to print it "the right way"
        //cout << bits << '\n';
        cout << '\n';
      }

      lockPassed = 0;

    // Otherwise, store the bit  
    } else {
      if ((duration > LOW_PULSE - TOLERANCE && duration < LOW_PULSE + TOLERANCE) || (duration > HIGH_PULSE - TOLERANCE && duration < HIGH_PULSE + TOLERANCE) ) {
        // HIGH
        if (digitalRead(1) == 0 && (duration > HIGH_PULSE - TOLERANCE && duration < HIGH_PULSE + TOLERANCE)) {
          // UNDO all this minus crap
          bits.set(changeCount++, false);
        }
        // LOW
        if (digitalRead(1) == 0 && (duration > LOW_PULSE - TOLERANCE && duration < LOW_PULSE + TOLERANCE)) {
          bits.set(changeCount++, true);
        }
      }
    }
  }

  lastTime = time;  
}



int main(void) {
  if(wiringPiSetup() == -1)
       return 0;

  //attach interrupt to changes on the pin
  if ( wiringPiISR (1, INT_EDGE_BOTH, &handleInterrupt) < 0 ) {
      return 1;
  }

  //interrupt will handle changes
  while(true);
}
