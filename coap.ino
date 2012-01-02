//
//
// FIXME: Include license/link info for
// Parser from: http://openwsn.berkeley.edu/browser/trunk/firmware/openos/openwsn/04-TRAN/opencoap.c?rev=1327
//
//
//     0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |Ver| T |  OC   |      Code     |          Message ID           |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |   Options (if any) ...
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |   Payload (if any) ...
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//
//#define COAP_METHOD_NOT_ALLOWED 133
//
//     0   1   2   3   4   5   6   7
//   +---+---+---+---+---+---+---+---+
//   | Option Delta  |    Length     | for 0..14
//   +---+---+---+---+---+---+---+---+
//   |   Option Value ...
//   +---+---+---+---+---+---+---+---+
//                                               for 15..270:
//   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
//   | Option Delta  | 1   1   1   1 |          Length - 15          |
//   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
//   |   Option Value ...
//   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
//
//                          Figure 8: Option Format

static uint16_t c_udpLen;

static coap_header_iht coapHeader;
static coap_option_iht coapOptions[MAX_COAP_OPTIONS];


//
//
//
void coapStart() {

  c_udpLen = gPB[UDP_LEN_H_P];
  c_udpLen = c_udpLen << 8;
  c_udpLen = c_udpLen | gPB[UDP_LEN_L_P];
  
  Serial.print("udplen:");
  Serial.println(c_udpLen);
  
  if ( c_udpLen < 12 ) {
    Serial.println("UDP Packet too short");
    return;
  }

  if ( parseCoap() < 0 ) {
    return;
  }

  if ( coapHeader.Code != COAP_CODE_REQ_GET ) {
    // not a GET request
    coapError(COAP_CODE_RESP_METHODNOTALLOWED); // Method not allowed
    return;
  }

  if ( (coapHeader.T < COAP_TYPE_CON) || (coapHeader.T > COAP_TYPE_NON) ) {
    Serial.print("Invalid type: ");
    Serial.println(coapHeader.T , HEX);
    return;
  }

  byte rindex = 0;
  byte lastOption = 0;
  
  memset( replyBuffer, 0, REPLY_BUFFER_SIZE);

  if (coapHeader.Code==COAP_CODE_REQ_GET && coapHeader.OC==2 &&

    memcmp(coapOptions[0].pValue,&coapWellKnown0,sizeof(coapWellKnown0)-1)==0 &&
    memcmp(coapOptions[1].pValue,&coapWellKnown1,sizeof(coapWellKnown1)-1)==0) {
   
    replyBuffer[rindex++] = ( 1 << 6 ) | ( COAP_TYPE_ACK << 4 ) | 1;

    replyBuffer[rindex++] = COAP_CODE_RESP_CONTENT;

    replyBuffer[rindex++] = coapHeader.MessageId[0];
    replyBuffer[rindex++] = coapHeader.MessageId[1];

    replyBuffer[rindex++] = COAP_OPTION_CONTENTTYPE << 4 | 1;
    replyBuffer[rindex++] = COAP_MEDTYPE_APPLINKFORMAT;
    
    memcpy( &replyBuffer[rindex], coapwellknown_resp_payload,sizeof(coapwellknown_resp_payload)-1);
    rindex += sizeof(coapwellknown_resp_payload)-1;

    ether.makeUdpReply(replyBuffer, rindex, coapPort);
    
    return;

  }  
  else if ( coapHeader.Code==COAP_CODE_REQ_GET && 
    coapHeader.OC==1 &&
    memcmp(coapOptions[0].pValue,&coapGeigerIndexOption0,sizeof(coapGeigerIndexOption0)-1)==0) {

    memset( replyBuffer, 0, REPLY_BUFFER_SIZE);

    replyBuffer[rindex++] = ( 1 << 6 ) | ( COAP_TYPE_ACK << 4 ) | 1;

    replyBuffer[rindex++] = COAP_CODE_RESP_CONTENT;

    replyBuffer[rindex++] = coapHeader.MessageId[0];
    replyBuffer[rindex++] = coapHeader.MessageId[1];

    // TODO: actually reply with a list.. oh and define the list ;p
    replyBuffer[rindex++] = COAP_OPTION_CONTENTTYPE << 4 | 1;
    replyBuffer[rindex++] = COAP_MEDTYPE_APPLINKFORMAT;
    memcpy( &replyBuffer[rindex], coapGeigerIndex,sizeof(coapGeigerIndex)-1);
    rindex += sizeof(coapGeigerIndex)-1;
    
    ether.makeUdpReply(replyBuffer, rindex, coapPort);
    
    return;
    
  } 
  else if ( coapHeader.Code==COAP_CODE_REQ_GET && 
    coapHeader.OC==2 &&
    memcmp(coapOptions[0].pValue,&coapGeigerIndexOption0,sizeof(coapGeigerIndexOption0)-1)==0) {

      
      // zero out the reply buffer
      memset( replyBuffer, 0, REPLY_BUFFER_SIZE);

      // version | reply type | option count
      replyBuffer[rindex++] = ( 1 << 6 ) | ( COAP_TYPE_ACK << 4 ) | 2;

      replyBuffer[rindex++] = COAP_CODE_RESP_CONTENT;

      replyBuffer[rindex++] = coapHeader.MessageId[0];
      replyBuffer[rindex++] = coapHeader.MessageId[1];

      // TODO: actually reply with a list.. oh and define the list ;p
      replyBuffer[rindex++] = COAP_OPTION_CONTENTTYPE << 4 | 1;
      lastOption = COAP_OPTION_CONTENTTYPE;
      replyBuffer[rindex++] = COAP_MEDTYPE_TEXTPLAIN;
      
      // option | length (for < 14)
      replyBuffer[rindex++] = (COAP_OPTION_MAXAGE - lastOption) << 4 | 1;
      lastOption = COAP_OPTION_MAXAGE;
      replyBuffer[rindex++] = 1;

            
      memset(numberBuffer, 0, NUMBER_BUFFER_SIZE);
      
    if ( memcmp(coapOptions[1].pValue,&coapGeigerIndexOption1,sizeof(coapGeigerIndexOption1)-1)==0 ) {
            
      itoa(cps, numberBuffer, 10);
      
      byte isize = strnlen( numberBuffer, NUMBER_BUFFER_SIZE);
      
      memcpy( &replyBuffer[rindex], numberBuffer, isize);
      rindex += isize;
            
    } 
    else if ( memcmp(coapOptions[1].pValue,&coapGeigerIndexOption2,sizeof(coapGeigerIndexOption2)-1)==0 ) {
       
      itoa(cpm, numberBuffer, 10);
      
      byte isize = strnlen( numberBuffer, NUMBER_BUFFER_SIZE);
 
      memcpy( &replyBuffer[rindex], numberBuffer, isize);
      rindex += isize;              

    } 
    else if ( memcmp(coapOptions[1].pValue,&coapGeigerIndexOption3,sizeof(coapGeigerIndexOption3)-1)==0) {
            
      double usv = cpm / CPM_RATIO;
      dtostrf(usv, 2, 4, numberBuffer);
      
      byte isize = strnlen( numberBuffer, NUMBER_BUFFER_SIZE);
 
      memcpy( &replyBuffer[rindex], numberBuffer, isize);
      rindex += isize;
      
    } 
    else {
      // send error
      coapError(COAP_CODE_RESP_NOTFOUND);
      return;
    }

    ether.makeUdpReply(replyBuffer, rindex, coapPort);
    return;

  } 
  else {

    // any other request
    Serial.println("*** Message not processed ***");
    coapError(COAP_CODE_RESP_SERVERERROR);
    return;

  } 

  // not sure the request should have payload. need to check

}

//
//
//
void coapError(coap_code_t code) {

  Serial.print("Sending error code: ");
  Serial.println(code, DEC);

  memset( replyBuffer, 0, REPLY_BUFFER_SIZE);

  // set version, and 0 options for ACK
  replyBuffer[0] = coapHeader.Ver << 2;
  replyBuffer[0] = replyBuffer[0] | 0x2;
  replyBuffer[0] = replyBuffer[0] << 4;

  replyBuffer[1] = code;

  replyBuffer[2] = coapHeader.MessageId[0];
  replyBuffer[3] = coapHeader.MessageId[1];

  ether.makeUdpReply(replyBuffer, 4, coapPort);

}

//
//
//
byte parseCoap() {

  byte index;
  coap_option_t    last_option;

  Serial.println("parseCoap");

  // parse the CoAP header and remove from packet
  index = UDP_DATA_P;
  coapHeader.Ver           = (gPB[index] & 0xc0) >> 6;
  coapHeader.T             = (coap_type_t)((gPB[index] & 0x30) >> 4);
  coapHeader.OC            = (gPB[index++] & 0x0f);
  coapHeader.Code          = (coap_code_t)(gPB[index++]);
  coapHeader.MessageId[0]  = gPB[index++];
  coapHeader.MessageId[1]  = gPB[index++];

  // only version 1 exists right now
  if ( coapHeader.Ver != 1 ) {
    Serial.println("Not version 1");
    return -1;
  }

  byte i;

  // initialize the options
  for (i=0;i<MAX_COAP_OPTIONS;i++) {
    coapOptions[i].type = COAP_OPTION_NONE;
  }

  // fill in the options
  last_option = COAP_OPTION_NONE;
  for (i=0;i<coapHeader.OC;i++) {
    Serial.print("Parsing option ");
    Serial.println(i);
    coapOptions[i].type        = (coap_option_t)((uint8_t)last_option+(uint8_t)((gPB[index] & 0xf0) >> 4));
    last_option                = coapOptions[i].type;
    coapOptions[i].length      = (gPB[index] & 0x0f);
    index++;
    coapOptions[i].pValue      = &(gPB[index]);
    index += coapOptions[i].length;
  }

  return 0;
}




















