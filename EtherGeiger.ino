/*
 *
 * Original code based on the "Pachube_WickedNode" code:
 *======================================================
 * Arduino + Wicked Node / Reciever Posted to Pachube 
 *          Author: Victor Aprea
 *   Documentation: http://wickeddevice.com
 * Licensed under Creative Commons Attribution-Noncommercial-Share Alike 3.0
 *
 *
 * Geiger counter code from:
 * http://mightyohm.com/blog/products/geiger-counter/
 *
 * http://mightyohm.com/files/geiger/geiger_counter_src.zip
 * License: GPLv3
 *
 */

#include "EtherCard.h"
#include "net.h"

// i hate typing
#define gPB ether.buffer

#define UINT32_MAX  ( 4294967295U )

#include "coap.h"

#define COAP_PORT 5683

unsigned long timer;

static uint16_t coapPort = COAP_PORT;
byte coapLow = coapPort ;
byte coapHigh = coapPort >> 8;


// geiger counted stuff
#define SERIAL_SPEED 19200
#define GEIGER_PIN 3
#define GEIGER_INT 1
#define GEIGER_LEVEL LOW
#define CPM_RATIO 175.43
#define LONG_PERIOD 60
#define SHORT_PERIOD 5



//
#define THRESHOLD 1000
#define SCALE_FACTOR 57

// incremented in ISR
volatile unsigned long clicks;
volatile uint16_t slowcpm;
volatile uint16_t fastcpm;
volatile uint16_t cps;
volatile uint32_t cpm;
volatile byte overFlow;


volatile uint32_t buffer[LONG_PERIOD];
volatile byte idx;
volatile byte eventflag;
volatile byte tick;

byte mode;

void sendReport(void);

#define NUMBER_BUFFER_SIZE 20
static char numberBuffer[20];

// mac and ip (if not using DHCP) have to be unique
// in your local area network. You can not have the same numbers in
// two devices:
static uint8_t netMAC[6] = { 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

#define BUFFER_SIZE 550
byte Ethernet::buffer[BUFFER_SIZE];

// Magic numbers bad
// we build the reply here for now, then copy for send
// TODO: probably better to build in the ethernet buffer(maybe...)
#define REPLY_BUFFER_SIZE 150
static char replyBuffer[REPLY_BUFFER_SIZE];


void(* resetFunc) (void) = 0; //declare reset function @ address 0

void setup(){

  Serial.begin(SERIAL_SPEED);
  Serial.println("ethergeiger: pee@erkkila.org");

  noInterrupts();
  unio_standby();  
  unio_start_header(); // start header, wakes up the chip and syncs clock
  unio_sendByte(0xA0); // Address A0 (the chip address)
  unio_sendByte(0x03); // Read instruction
  unio_sendByte(0x00); // word address MSB 0x00
  unio_sendByte(0xFA); // word addres LSB 0xFA
  unio_readBytes( netMAC , 6); // read 6 bytes
  unio_standby(); // back to standby
  interrupts();

  printMAC( netMAC );

  pinMode(GEIGER_PIN, INPUT);

  // interrupt on rising signal
  attachInterrupt( GEIGER_INT, geigerISR, RISING);

  if (ether.begin(sizeof Ethernet::buffer, netMAC) == 0) {
    Serial.println( "Failed to access Ethernet controller");
  }

  Serial.println("Starting DHCP");
  if (!ether.dhcpSetup()) {
    Serial.println( "DHCP failed");
    resetFunc();  //call reset
  }
  ether.printIp("IP: ", ether.myip);

  while (ether.clientWaitingGw()) {
    ether.packetLoop(ether.packetReceive());
  }

  noInterrupts();
    
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;

  OCR1A = 62500;            // compare match register 16MHz/256/2Hz = 31250, i had to make this 62500 to get 1sec 
  TCCR1B |= (1 << WGM12);   // CTC mode
  TCCR1B |= (1 << CS12);    // 256 prescaler 
  TIMSK1 |= (1 << OCIE1A);  // enable timer compare interrupt
  
  interrupts();

}



void loop(){
 
  uint16_t len = ether.packetReceive();

  uint16_t pos = ether.packetLoop(len);

  if ( pos != 0 ) {
    Serial.print("pos:");
    Serial.println(pos);
  }

  if ( len != 0 ) {
    if ( gPB[IP_PROTO_P] == IP_PROTO_UDP_V) {
      //Serial.println("UDP Packet found");

      if ( (gPB[UDP_DST_PORT_L_P] == coapLow)  && (gPB[UDP_DST_PORT_H_P] == coapHigh) ) {
        //Serial.println("Found coap req");
        coapStart();
      }

    }
  }

  if ( tick == 1 ) {
    sendReport();
  }  

  // last thing we care about 
  if (ether.dhcpExpired()) {
    ether.dhcpSetup();
  }

}

//
//
//
void printMAC( uint8_t *buf ) {

  Serial.print("MAC:");
  for ( int i = 0; i < 6; i++ ) {
    Serial.print( buf[i], DEC );
    if ( i < 5 ) {
      Serial.print( ":" );
    }
  }
  Serial.println("");

}

// original from geiger.c MIGHTYHOHM!
//
ISR(TIMER1_COMPA_vect) {
  
  byte i;  // index for fast mode
  tick = 1; // update flag
  
  cps = clicks;
  slowcpm -= buffer[idx]; // subtract oldest sample in sample buffer
  
  if ( clicks > UINT32_MAX ) {   // overflow
    clicks = UINT32_MAX;
    overFlow = 1;
  }
  
  slowcpm += clicks;   // add current sample
  buffer[idx] = clicks;  // save current sample to buffer
  
  // Compute CPM base on the last SHORT_PERIOD samples
  fastcpm = 0;
  for ( i = 0; i < SHORT_PERIOD; i++ ) {
    
    int8_t x = idx - i;
    if ( x < 0 ) {
      x = LONG_PERIOD + x;
    }
    fastcpm += buffer[x]; // sum up the last 5 CPS values
        
  }
  fastcpm = fastcpm * ( LONG_PERIOD/SHORT_PERIOD); // convert to CPM
  
  // move to the next entry in the sample buffer
  idx++;
  if ( idx >= LONG_PERIOD ) {
    idx = 0;
  }
  clicks = 0;  // reset counter
  
}

//
//
void sendReport(void) {
  
  tick = 0;
  
  if ( overFlow ) {
    
    cpm = cps * 60UL;
    mode = 2;
    overFlow = 0;
    
  } else if ( fastcpm > THRESHOLD ) {
    mode = 1;
    cpm = fastcpm;
  } else {
    mode = 0;
    cpm = slowcpm;
  }    
  
  Serial.print("CPS, ");
  Serial.print( cps, DEC );
  
  Serial.print(", CPM, ");
  Serial.print( cpm, DEC );
  
  Serial.print(", uSv/hr, ");
  
  // calculate uSv/hr based on scaling factor, and multiply result by 100
  // so we can easily seperate the integer and factional component (  2 decimal places )
//  uint32_t usvScaled = (uint32_t)(cpm*SCALE_FACTOR); // scale and truncate the interger part
//  
//  uint16_t u = (uint16_t)(usvScaled/10000);
//  Serial.print(u, DEC);
//  Serial.print(".");
//  
//  byte fraction = ( usvScaled/100)%100;
//  if ( fraction < 10 ) {
//    Serial.print('0');
//  }
//  Serial.print(fraction);

  double usv = cpm / CPM_RATIO;
  dtostrf(usv, 2, 4, numberBuffer);
  
  Serial.print(numberBuffer);
  
  if ( mode == 2 ) {
    Serial.println(", INST");
  } else if ( mode == 1 ) {
    Serial.println(", FAST");
  } else {
    Serial.println(", SLOW");
  }
  
  //Serial.println(millis(),DEC);
   
  
}


//
// Called on geiger counter probe interrupt
//
void geigerISR() {
  
  if ( clicks < UINT32_MAX ) {
    clicks++;
  }
  
}




