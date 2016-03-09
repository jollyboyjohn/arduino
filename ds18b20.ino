/*
 Bare-bones program for reading a DS18B20 temperature sensor
 The DS18 needs a pull-up resistor, so the default logic level is HIGH, not LOW.
 
 Raspio Duino details:
   * each command is about 5us
   * the delayMicroseconds() is actually 1.5 times more than required - 10 = 15us

 Reset Process:
           __________             __
|_________/          |___________/
 <-480us-><-15-60us-><-60-240us->
   User     RELEASE     DS18B20
   
Read and write bits - result sampled at ~14us: 
                ___
0: |___________/
       ____________
1: |__/
    <-15-><---45--->
*/

int ledPin = 13;
int dsPin = 2;
long start;

void setup() {
  // put your setup code here, to run once:
  pinMode(ledPin, OUTPUT);
  
  Serial.begin(9600);
}

void pause(int usecs) {
  int i;
  for (i=0;i<usecs;i++);
}

boolean owreset(int pin) {
  boolean state;
  
  // Hold the signal LOW for minimum of 480us
  pinMode(pin, OUTPUT); // 5us
  digitalWrite(pin, LOW);  // 5us
  delayMicroseconds(310); // 3 x 5us + 465us = 480us

  // 15us-60us later the DS18B20 will respond by pulling LOW for 60us-240us
  pinMode(pin, INPUT);
  delayMicroseconds(40); // 60us 
  state = digitalRead(pin);
  delayMicroseconds(670); // 1ms
  
  return state;
}


void write_bit(int pin, boolean level) {
  // Sensor looks for a HIGH or LOW at ~14us - window is 60-120us
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delayMicroseconds(1); 
  
  if (level) {
    // Write 1 - Release
    pinMode(pin, INPUT);
    delayMicroseconds(27);
  } else {
    // Write 0 - Hold
    delayMicroseconds(27); 
    pinMode(pin, INPUT);
  } 
  
  delayMicroseconds(1);
}

void write_byte(int pin, byte value) {
  int i;
  byte temp;

  for (i=0; i<8; i++) {
    temp = value>>i;
    temp &= 0x01;
    write_bit(pin, temp);
  }
}

boolean read_bit(int pin) {
  boolean level;
  
  // Pull the signal LOW for >1us, then release to HIGH
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW); // 5 usecs
  pinMode(pin, INPUT); // 5 usecs
  level = digitalRead(pin); // 5usecs
  
  delayMicroseconds(30); // 45usecs
  return level;
}

byte read_byte(int pin) {
  int i;
  byte value = 0;
  
  for (i=0; i<8; i++) {
    if ( read_bit(pin) )
      value |= 0x01<<i;
  }
  
  return value;
}

void parse_temperature(byte data[9]) {
  float t, h;
  
  if (!data[7])
    return;
    
  if (data[1] == 0)
    t = 1000 * ((long)data[0] >> 1);
  else
    t = 1000 * (-1 * (long)(0x100-data[0]) >> 1);
  
  t -= 250;
  h = 1000 * ((long)data[7] - (long)data[6]);
  h /= (long)data[7];
  t += h;
  t /= 1000;
  
  Serial.print("Temp: ");
  Serial.println(t, 1);
}  


void loop() {
  byte data[10];
  int i;
  
  if ( owreset(dsPin) ) {
    digitalWrite(ledPin, LOW);
    Serial.println("Reset 1: Unsuccessful");
  } else {
    digitalWrite(ledPin, HIGH);
    write_byte(dsPin, 0xCC); // Skip ROM
    write_byte(dsPin, 0x44); // Start Conversion
    delayMicroseconds(34);
    if ( owreset(dsPin) ) {
      digitalWrite(ledPin, LOW);
      Serial.println("Reset 2: Unsuccessful");
    } else {
      write_byte(dsPin, 0xCC); // Skip ROM
      write_byte(dsPin, 0xBE); // Read Scratch Pad
    
      for (i=0;i<9;i++) {
        data[i] = read_byte(dsPin);
      }
      parse_temperature(data);
    
      /* DEBUG - or for 0x33 (ROM print)
      for (i=0;i<9;i++) {
        Serial.print(" 0x");      
        Serial.print(data[i], HEX);
      } */
     
    }  
  }

  delay(5000);
}
