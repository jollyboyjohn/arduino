/* 
   Title: maverick.ino
   Desc: Using an Aurel MID RX 3V (433MHz), this reads the temperature from a Maverick Radio digital thermometer
   This was written on a 12MHz Rasp.io Duino
*/

// Global variables
int rcvPin = 2;  // Use pin 2, as it has an interrupt
int rcvInt = 0;  // Interrupt 0 is wired to pin 2
int ledPin = 13; // Handy to see if a beacon has been detected
int i, x;

// Beacon variables
int bMin = 4000;
int bMax = 6250; // Beacon is 8 pulses of about 4.5ms
int bCount = 8;

// Edge variables
volatile boolean edgeseen = 0;  // Call back variable for interrupt when edge is seen
int firstadj = 400;             // The first pulse is LONG, so set as 400us
long lastedge, thisedge;        // Used to compare the last and current reasons
int eThres = 325;               // <325us = SHORT, >325us = LONG
int eMax = 600;                 // Edge >600us = not data

// Data
int nibbles = 18;               // The data is 18 nibbles (18 x 4 bits = 72 bits)

void setup() {
  pinMode(rcvPin, INPUT);
  pinMode(ledPin, OUTPUT);
  Serial.begin(9600);
}

void detectEdge() {
  edgeseen = true;                             // Very short callback to indicate a signal change
} 

int detectBeacon() {
  i = 0;
  while (pulseIn(rcvPin,LOW,bMax) > bMin)      // Look for a beacon pulse...
    i++;                                       // ... if we see one in succession, count it
  if (i == bCount)
    return 1;                                  // return true if we receive enough beacons
  else
    return 0;                                  // exit if the beacons are too less
}  

void recordEdges(int* edgetimes) {
  edgetimes[0] = firstadj;                     // Set the edge length to predefined 
  lastedge = micros() - firstadj;              // fake the last edge
  i = 1;
  
  edgeseen = false;
  attachInterrupt(rcvInt, detectEdge, CHANGE); // Initialise callback on CHANGE
  
  while (edgetimes[i] < eMax) {                // Loop until we see a callback               
    if (edgeseen == true) {                    // If we see an edge from the callback...
      thisedge = micros();                     // 1) record the time the callback was received
      edgetimes[i++] = thisedge - lastedge;    // 2) record the length of the pulse
      lastedge = thisedge;                     // 3) save time for next pass
      edgeseen = false;                        // 4) reset callback value
    }
  }
  
  detachInterrupt(rcvInt);                      // Destroy callback
}
  
void edgeToHex(int* edgetimes, char* data) {
  boolean skipnext = false;                     // This is used to track the "reset" pulse between 00 or 11
  int b = 0;                                    // keeps track of bits
  int n = 0;                                    // keeps track of nibble
  int e = 0;                                    // keeps track of edge array
  boolean last = 1;                             // keeps track of the signal state - CHANGE callback cannot
  data[n] = last;      
 
  while (n < nibbles) {
    if (skipnext == true) {          // Second, phantom short pulse
      skipnext = false;              // -- Don't skip next pulse
    } else {
      data[n] <<= 1;
      if (edgetimes[e] > eThres) {   // Long pulse - 01 or 10
        last = !last;                // -- Flip the bit
        skipnext = false;            // -- Don't skip next pulse
      } else {                       // Short pulse - 00 or 11
        skipnext = true;             // -- Skip next pulse
      }
      data[n] += last;               // Add the bit to the stack
      b++;                           // Count the bits
      if (b % 4 == 3)                // If it's the last bit of a nibble...
        data[++n] = 0;               // -- move to next nibble and initialise
    }
    e++;                             // move to next edge
  }
}

void printTemperature(char* data) {
  int valuea = 0;
  int valueb = 0;

  /*   Temperature data is represented by 5 x 4 bits, each with a different value:
       0x5 (0101) => 0, 
       0x6 (0110) => 1, 
       0x9 (1001) => 2, 
       0xA (1010) => 3 */
  static const char deq[11] = { 0, 0, 0, 0, 0, 0, 1, 0, 0, 2, 3 }; // 5=0, 6=1, 9=2, A=3
  
  /*   Each bit is multiplied by a factor of 4 bit, depending on position:
       0x9 => 2 * 4^4 (256) = 512
       0x5 => 0 * 4^3 (64)  =   0
       0x9 => 2 * 4^2 (16)  =  32
       0x9 => 2 * 4^1 (4)   =   8
       0x6 => 1 * 4^0 (1)   =   1 
                      Total = 553 */
  x = 4;
  for (i=8; i<13; i++)
    valuea += (deq[data[i]]) << (2 * x--);  
  x = 4;
  for (i=13; i<19; i++)
    valueb += (deq[data[i]]) << (2 * x--);
  
  /*   Then minus the "absolute zero" - 532:
       e.g. 553 - 532 = 21 celcius */
  Serial.print("Probe A: ");
  Serial.print(valuea - 532);
  Serial.print(", Probe B: ");
  Serial.println(valueb - 532);
}


void loop() {
  char data[18];                 // 72 bits, divided into 4-bit nibbles
  int edgetimes[150];            // Very approximate!
  
  if (detectBeacon()) {          // If we detect 8 x 4.5ms pulses
    digitalWrite(ledPin, HIGH);  // Switch on the LED
    recordEdges(edgetimes);      // Record all the signal edges
    edgeToHex(edgetimes, data);  // Translate all the edges to binary data
    printTemperature(data);      // Convert and print the data 
    digitalWrite(ledPin, LOW);   // Switch off the LED
    delay(11100);                 // Snooze until next beacon
  }
  delay(5);                      // Cycle until a beacon is seen
}
