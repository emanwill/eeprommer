/**
   AT28C256 EEPROM reader and programmer

   Arduino Pin  |  Circuit Pin
   -------------+---------------
      D     D2  |  EEPROM Data 0 (pin 11)
      D     D3  |  EEPROM Data 1 (pin 12)
      D     D4  |  EEPROM Data 2 (pin 13)
      D     D5  |  EEPROM Data 3 (pin 15)
      D     D6  |  EEPROM Data 4 (pin 16)
      D     D7  |  EEPROM Data 5 (pin 17)
      B     D8  |  EEPROM Data 6 (pin 18)
      B     D9  |  EEPROM Data 7 (pin 19)
      C     A0  |  EEPROM Write Enable (pin 27, active low)
      C     A1  |  EEPROM Output Enable (pin 22, active low)
      C     A2  |  EEPROM Chip Enable (pin 20, active low)
   -------------+---------------
      C     A3  |  74CH595 Output Enable (pin 13, active low)
      C     A4  |  74CH595 Serial Input (pin 14)
      B    D11  |  74CH595 Serial Clock (pin 11, active low)
      B    D12  |  74CH595 Register Clock (pin 12, active high)
      B    D13  |  74CH595 Clear (pin 10, active low)
   -------------+---------------
      B    D10  |  Status LED


   NOTES:
   I can't seem to get the EEPROM's page mode write cycle to work.
   I wonder if the timing for that is pickier than what I was attempting.
*/

#include <Arduino.h>

enum MODE {
  STANDBY,
  READ,
  WRITE
};

enum STATUS {
  OK,
  ERR_INVALID,
  ERR_RESET,
  ERR_CORRUPT,
  ERR_UNEXPECTED,
  ERR_UNKNOWN
};

const unsigned int MAX_PAYLOAD_SIZE = 63;
const unsigned int DELAY_MICROS = 10;

// EEPROM control and data lines
const unsigned int EEPROM_WE = A0;
const unsigned int EEPROM_OE = A1;
const unsigned int EEPROM_CE = A2;
const unsigned int DATA_PINS[] = {2, 3, 4, 5, 6, 7, 8, 9};

// Shift register control lines
const unsigned int SHIFT_OE = A3;
const unsigned int SHIFT_SER = A4;
const unsigned int SHIFT_SER_CLK = 11;
const unsigned int SHIFT_REG_CLK = 12;
const unsigned int SHIFT_CLEAR = 13;

// Status LED and signal settings
const unsigned int STATUS_LED = 10;
const unsigned int SPEED = 12;
const unsigned int DOTLEN = 1200 / SPEED;
const unsigned int DASHLEN = 3 * DOTLEN;

MODE mode = STANDBY;
STATUS status = OK;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  // Initialize EEPROM control pins
  pinMode(EEPROM_CE, OUTPUT);
  pinMode(EEPROM_OE, OUTPUT);
  pinMode(EEPROM_WE, OUTPUT);

  // Initialize shift register control pins
  pinMode(SHIFT_OE, OUTPUT);
  digitalWrite(SHIFT_OE, LOW);
  pinMode(SHIFT_SER, OUTPUT);
  pinMode(SHIFT_REG_CLK, OUTPUT);
  pinMode(SHIFT_SER_CLK, OUTPUT);
  pinMode(SHIFT_CLEAR, OUTPUT);
  digitalWrite(SHIFT_CLEAR, HIGH);

  // Initialize status LED pin
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);

  enterStandbyMode();

  while (!Serial) {
    ;
  }

  dotLED();
  sendAckPacket();
}

void loop() {
  while (Serial.available() > 0) {
    byte packet[MAX_PAYLOAD_SIZE + 1];
    int packetLen = receivePacket(packet, sizeof(packet), false);

    digitalWrite(STATUS_LED, HIGH);

    if (packetLen > 0) {
      byte cmd = packet[0];
      if (cmd == 0x72 && packetLen == 3) {
        dashLED();
        // Read from EEPROM
        byte value = readChipByte((packet[1] << 8) + packet[2]);
        sendPacket(&value, 1, false);
      } else if (cmd == 0x64 && packetLen == 1) {
        // Dump from EEPROM
        int dumpStatus = dumpChipBytes();
      } else if (cmd == 0x77 && packetLen == 4) {
        // Write byte to EEPROM
        writeChipByte((packet[1] << 8) + packet[2], packet[3]);
        delay(1);
        waitForChip();
      } else if (cmd == 0x6c && packetLen == 3) {
        // Upload block data to EEPROM
        loadChip((packet[1] << 8) + packet[2]);
      } else if (cmd == 0x73 && packetLen == 1) {
        // Reset
        // Do nothing; this command is meaningless in this context
      } else if (cmd == 0x74) {
        // DO TESTING HERE!

        byte testData[63];
        for (int i = 0; i < 63; i++) {
          testData[i] = i;
        }
        writeChipPage(16, testData, 63);
        writeChipPage(128, testData, 63);

        byte value = readChipByte(9);
        sendPacket(&value, 1, false);
      } else {
        status = ERR_INVALID;
      }
    }

    digitalWrite(STATUS_LED, LOW);
  }

  if (status != OK) {
    delay(DASHLEN * 2);
    handleError();
  }
}

int enterStandbyMode() {
  if (mode != STANDBY) {
    DDRD = DDRD & B00000011; // D2 through D7 input
    DDRB = DDRB & B11111100; // B0 through B1 input

    PORTC = PORTC | B00000101; // EEPROM CE, WE high
    PORTC = PORTC & B11111101; // EEPROM OE low

    delayMicroseconds(DELAY_MICROS);
  }

  mode = STANDBY;
  return 0;
}

int enterReadMode() {
  if (mode != READ) {
    DDRD = DDRD & B00000011; // D2 through D7 input
    DDRB = DDRB & B11111100; // B0 through B1 input

    PORTC = PORTC | B00000001; // EEPROM WE high
    PORTC = PORTC & B11111001; // EEPROM CE, OE low

    delayMicroseconds(DELAY_MICROS);
  }

  mode = READ;
  return 0;
}

int enterWriteMode() {
  if (mode != WRITE) {
    DDRD = DDRD | B11111100; // D2 through D7 output
    DDRB = DDRB | B00000011; // B0 through B1 output

    PORTC = PORTC | B00000011; // EEPROM OE, WE high
    PORTC = PORTC & B11111011; // EEPROM CE low

    delayMicroseconds(DELAY_MICROS);
  }

  mode = WRITE;
  return 0;
}

int receivePacket(byte *buffer, size_t maxLength, bool sendAck) {
  int lenByte = 0;
  int lenReceived = 0;
  do {
    lenByte = Serial.read();
  } while (lenByte == -1);

  if (lenByte > 0) {
    lenReceived = Serial.readBytes(buffer, min(lenByte, maxLength));
    if (lenReceived != lenByte) {
      status = ERR_CORRUPT;
      return -1;
    }
  }

  if (sendAck) {
    int sentStatus = sendAckPacket();
    if (sentStatus == -1) return -1;
  }

  return lenReceived;
}

int sendPacket(byte *packet, size_t length, bool waitForAck) {
  Serial.write(length);
  if (length > 0) {
    Serial.write(packet, length);
  }

  if (waitForAck) {
    byte buffer[MAX_PAYLOAD_SIZE + 1];
    int ackLen = receivePacket(buffer, MAX_PAYLOAD_SIZE, false);

    if (ackLen != 0) {
      if (ackLen == 1 && buffer[0] == 0x73) {
        status = ERR_RESET;
      } else if (ackLen != 1) {
        status = ERR_UNEXPECTED;
      } else {
        status = ERR_UNKNOWN;
      }

      return -1;
    }
  }

  return 0;
}

int sendAckPacket() {
  return sendPacket(NULL, 0, false);
}

byte readChipByte(unsigned int address) {
  enterReadMode();
  setAddress(address);

  // Read byte from data pins
  byte value = readDataBus();

  enterStandbyMode();
  return value;
}

int dumpChipBytes() {
  enterReadMode();
  byte packet[MAX_PAYLOAD_SIZE];
  int packetIdx = 0;

  for (unsigned int address = 0; address < 0x8000; address++) {
    packetIdx = address % MAX_PAYLOAD_SIZE;

    if (address > 0 && packetIdx == 0) {
      int packetStatus = sendPacket(packet, MAX_PAYLOAD_SIZE, true);
      if (packetStatus == -1) {
        enterStandbyMode();
        return -1;
      }
    }

    setAddress(address);
    byte value = readDataBus();
    packet[packetIdx] = value;
  }

  enterStandbyMode();

  if (packetIdx > 0) {
    return sendPacket(packet, packetIdx + 1, true);
  }

  return 0;
}

int writeChipByte(unsigned int address, byte value) {
  setAddress(address);
  enterWriteMode();

  // Load byte onto data pins
  writeDataBus(value);

  // pulse EEPROM control pins
  // Port C0 = EEPROM /WE
  // Port C2 = EEPROM /CE
  PORTC = PORTC & B11111010;
  delayMicroseconds(1);
  PORTC = PORTC | B00000101;

  enterStandbyMode();
}

int loadChip(unsigned int length) {
  sendAckPacket();
  unsigned int idx = 0;
  byte packet[MAX_PAYLOAD_SIZE + 1];

  while (idx < length) {
    int packetLen = receivePacket(packet, MAX_PAYLOAD_SIZE + 1, false);
    writeChipPage(idx, packet, packetLen);
    sendAckPacket();
    idx += packetLen;
  }
}

int writeChipPage(unsigned int address, byte *data, unsigned int length) {
  for (unsigned int idx = 0; idx < length; idx++) {
    writeChipByte(address + idx, data[idx]);
    delayMicroseconds(1);
  }

  delay(1);

  waitForChip();
}

void setAddress(unsigned int address) {
  for (int i = 15; i >= 0; i--) {
    // Set SER value
    if (bitRead(address, i)) {
      PORTC = PORTC | B00010000;
    } else {
      PORTC = PORTC & B11101111;
    }

    // Pulse SRCLK
    PORTB = PORTB | B00001000;
    PORTB = PORTB & B11110111;
  }

  // Pulse RCLK
  PORTB = PORTB | B00010000;
  PORTB = PORTB & B11101111;
}

// Read a byte from the data bus.  The caller must set the bus to input_mode
// before calling this or no useful data will be returned.
byte readDataBus() {
  /* bit = port #
       0 = D2
       1 = D3
       ...
       5 = D7
       6 = B0
       7 = B1

     DO NOT TOUCH D0, D1, B6, or B7
  */
  return (PIND >> 2) | (PINB << 6);
}

// Write a byte to the data bus.  The caller must set the bus to output_mode
// before calling this or no data will be written.
void writeDataBus(byte value) {
  /* bit = port #
       0 = D2
       1 = D3
       ...
       5 = D7
       6 = B0
       7 = B1

     lowest 6 bits go to highest 6 pins of port D
     highest 2 bits go to lowest 2 pins of port B

     DO NOT TOUCH D0, D1, B6, or B7
  */
  PORTD = (PORTD & B00000011) | (value << 2);
  PORTB = (PORTB & B11111100) | (value >> 6);
}

void waitForChip() {
  DDRB = DDRB & B11111110; // Set data pin 6 (PORTB[0]) to INPUT
  PORTC = PORTC | B00000111; // EEPROM CE, OE, WE high

  int values[3];

  do {
    for (int i = 0; i < 3; i++) {
      delayMicroseconds(2);
      PORTC = PORTC & B11111001; // EEPROM CE, OE low
      delayMicroseconds(2);
      values[i] = PINB & 1; // set value to data bit 6
      PORTC = PORTC | B00000110; // EEPROM CE, OE high
    }
  } while (values[0] != values[1] || values[1] != values[2]);

  enterStandbyMode();
}

void handleError() {
  switch (status) {
    case ERR_CORRUPT: // "C" = _._.
      dashLED();
      dotLED();
      dashLED();
      dotLED();
      break;
    case ERR_RESET: // "R" = ._.
      dotLED();
      dashLED();
      dotLED();
      break;
    case ERR_UNEXPECTED: // "X" = _.._
      dashLED();
      dotLED();
      dotLED();
      dashLED();
      break;
    case ERR_UNKNOWN: // "U" = .._
      dotLED();
      dotLED();
      dashLED();
      break;
    case ERR_INVALID: // "V" = ..._
      dotLED();
      dotLED();
      dotLED();
      dashLED();
      break;
  }

  status = OK;
}

void dotLED() {
  digitalWrite(STATUS_LED, HIGH);
  delay(DOTLEN);
  digitalWrite(STATUS_LED, LOW);
  delay(DOTLEN);
}

void dashLED() {
  digitalWrite(STATUS_LED, HIGH);
  delay(DASHLEN);
  digitalWrite(STATUS_LED, LOW);
  delay(DOTLEN);
}