/**
  AT28C256 EEPROM reader and programmer


       Arduino Pin  |  Circuit Pin
  ------------------+------------------------------------
    PORTD[2]    D2  |  EEPROM Data 0 (pin 11)
    PORTD[3]    D3  |  EEPROM Data 1 (pin 12)
    PORTD[4]    D4  |  EEPROM Data 2 (pin 13)
    PORTD[5]    D5  |  EEPROM Data 3 (pin 15)
    PORTD[6]    D6  |  EEPROM Data 4 (pin 16)
    PORTD[7]    D7  |  EEPROM Data 5 (pin 17)
    PORTB[0]    D8  |  EEPROM Data 6 (pin 18)
    PORTB[1]    D9  |  EEPROM Data 7 (pin 19)
    PORTC[0]    A0  |  EEPROM Write Enable (pin 27, active low)
    PORTC[1]    A1  |  EEPROM Output Enable (pin 22, active low)
    PORTC[2]    A2  |  EEPROM Chip Enable (pin 20, active low)
  ------------------+------------------------------------
    PORTC[3]    A3  |  74CH595 Output Enable (pin 13, active low)
    PORTC[4]    A4  |  74CH595 Serial Input (pin 14)
    PORTB[3]   D11  |  74CH595 Serial Clock (pin 11, active low)
    PORTB[4]   D12  |  74CH595 Register Clock (pin 12, active high)
    PORTB[5]   D13  |  74CH595 Clear (pin 10, active low)
  ------------------+------------------------------------
    PORTB[2]   D10  |  Status LED


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
  OK,              // Status OK
  ERR_INVALID_CMD, // Received Invalid Command
  ERR_RESET,       // Received Reset Command
  ERR_CORRUPT_PKT, // Received Corrupt Packet
  ERR_UNEXPECTED,  // Received Unexpected Packet Type
  ERR_UNKNOWN      // Unknown Error
};

const unsigned char STATUS_CODES[] = {
  0b00000001, // OK              = (no code)
  0b00000100, // ERR_INVALID_CMD = "I" ..
  0b00001010, // ERR_RESET       = "R" .-.
  0b00010101, // ERR_CORRUPT_PKT = "C" -.-.
  0b00011001, // ERR_UNEXPECTED  = "X" -..-
  0b00001100  // ERR_UNKNOWN     = "U" ..-
};

const unsigned int MAX_PAYLOAD_SIZE = 63;

// Status signal settings
const unsigned int SPEED = 12;
const unsigned int DOTLEN = 1200 / SPEED;
const unsigned int DASHLEN = 3 * DOTLEN;

MODE mode = STANDBY;
STATUS status = OK;

/**
 * Initializes program state
 */
void setup() {
  Serial.begin(9600);

  // Initialize EEPROM control pins
  DDRC |= 0b00000111;  // set EEPROM CE, OE, WE as outputs
  PORTC |= 0b00000111; // EEPROM CE, OE, WE high

  // Initialize shift register control pins
  DDRC |= 0b00011000;  // set SHIFT SER, OE as outputs
  PORTC &= 0b11110111; // SHIFT OE low
  DDRB |= 0b00111000;  // set SHIFT RCLR, RCLK, SRCLK as outputs
  PORTB |= 0b00100000; // SHIFT RCLR high

  // Initialize status LED pin
  DDRB |= 0b00000100;  // set LED as output
  PORTB &= 0b11111011; // LED low

  enterStandbyMode();

  while (!Serial) {
    ; // Wait for serial connection to be established
  }

  // Flash single dot to indicate readiness
  flashCodeLED(0b00000010);

  // Send indicator that the unit is ready to receive commands
  sendAck();
}

/**
 * Main program loop
 */
void loop() {
  while (Serial.available() > 0) {
    unsigned char packet[MAX_PAYLOAD_SIZE + 1];
    int packetLen = receivePacket(packet, sizeof(packet), false);

    PORTB |= 0b00000100; // LED high

    if (packetLen > 0) {
      unsigned char cmd = packet[0];
      if (cmd == 'r' && packetLen == 3) {
        // READ
        unsigned char value = readChipByte((packet[1] << 8) + packet[2]);
        sendPacket(&value, 1, false);
      }
      else if (cmd == 'd' && packetLen == 1) {
        // DUMP
        dumpChipBytes();
      }
      else if (cmd == 'w' && packetLen == 4) {
        // WRITE
        writeChipByte((packet[1] << 8) + packet[2], packet[3]);
        waitForChip();
        sendAck();
      }
      else if (cmd == 'l' && packetLen == 3) {
        // LOAD
        loadChip((packet[1] << 8) + packet[2]);
        sendAck();
      }
      else if (cmd == 's' && packetLen == 1) {
        // RESET
        // Do nothing; this command is meaningless in this context
      }
      else {
        status = ERR_INVALID_CMD;
      }
    }

    PORTB &= 0b11111011; // LED low
  }

  if (status != OK) {
    delay(DASHLEN * 3);
    flashCodeLED(STATUS_CODES[status]);
    status = OK;
  }
}

/**
 * Sets I/O ports into STANDBY mode
 */
int enterStandbyMode() {
  if (mode != STANDBY) {
    DDRD &= 0b00000011;  // D2 through D7 input
    DDRB &= 0b11111100;  // B0 through B1 input
    PORTC |= 0b00000101; // EEPROM CE, WE high
    PORTC &= 0b11111101; // EEPROM OE low

    delayMicroseconds(1);
  }

  mode = STANDBY;
  return 0;
}

/**
 * Sets I/O ports into EEPROM READ mode
 */
int enterReadMode() {
  if (mode != READ) {
    DDRD &= 0b00000011;  // D2 through D7 input
    DDRB &= 0b11111100;  // B0 through B1 input
    PORTC |= 0b00000001; // EEPROM WE high
    PORTC &= 0b11111001; // EEPROM CE, OE low

    delayMicroseconds(1);
  }

  mode = READ;
  return 0;
}

/**
 * Sets I/O ports into EEPROM WRITE mode
 */
int enterWriteMode() {
  if (mode != WRITE) {
    DDRD |= 0b11111100;  // D2 through D7 output
    DDRB |= 0b00000011;  // B0 through B1 output
    PORTC |= 0b00000011; // EEPROM OE, WE high
    PORTC &= 0b11111011; // EEPROM CE low

    delayMicroseconds(1);
  }

  mode = WRITE;
  return 0;
}

/**
 * Receives an incoming serial packet
 * @param buffer Buffer that the packet will be written to
 * @param maxLength Maximum allowed packet length
 * @param replyAck Reply with ACK once packet is received
 */
int receivePacket(unsigned char *buffer, size_t maxLength, bool replyAck) {
  int lenByte = 0;
  int lenReceived = 0;

  do {
    lenByte = Serial.read();
  } while (lenByte == -1);

  if (lenByte > 0) {
    lenReceived = Serial.readBytes(buffer, min(lenByte, maxLength));
    if (lenReceived != lenByte) {
      status = ERR_CORRUPT_PKT;
      return -1;
    }
  }

  if (replyAck) {
    int sentStatus = sendAck();
    if (sentStatus == -1)
      return -1;
  }

  return lenReceived;
}

/**
 * Sends a serial packet
 * @param packet Buffer of packet data to send
 * @param length Length of packet data
 * @param waitForAck Wait for an ACK to be received before returning
 */
int sendPacket(unsigned char *packet, size_t length, bool waitForAck) {
  Serial.write(length);
  if (length > 0) {
    Serial.write(packet, length);
  }

  if (waitForAck) {
    unsigned char buffer[MAX_PAYLOAD_SIZE + 1];
    int ackLen = receivePacket(buffer, MAX_PAYLOAD_SIZE, false);

    if (ackLen != 0) {
      if (ackLen == 1 && buffer[0] == 's') {
        status = ERR_RESET;
      }
      else if (ackLen != 1) {
        status = ERR_UNEXPECTED;
      }
      else {
        status = ERR_UNKNOWN;
      }

      return -1;
    }
  }

  return 0;
}

/**
 * Sends an ACK serial packet
 */
int sendAck() {
  return sendPacket(NULL, 0, false);
}

/**
 * Reads a byte from the EEPROM at the given address
 * @param address 15-bit address
 */
unsigned char readChipByte(unsigned int address) {
  enterReadMode();
  setAddress(address);

  // Read byte from data pins
  unsigned char value = readDataBus();

  enterStandbyMode();
  return value;
}

/**
 * Reads the entire contents of the EEPROM and transmits it in a series of serial packets
 */
int dumpChipBytes() {
  enterReadMode();
  unsigned char packet[MAX_PAYLOAD_SIZE];
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
    unsigned char value = readDataBus();
    packet[packetIdx] = value;
  }

  enterStandbyMode();

  if (packetIdx > 0) {
    return sendPacket(packet, packetIdx + 1, true);
  }

  return 0;
}

/**
 * Writes a byte to the EEPROM at the given address
 * @param address 15-bit address
 * @param value byte value to write
 */
int writeChipByte(unsigned int address, unsigned char value) {
  setAddress(address);
  enterWriteMode();

  // Load byte onto data pins
  writeDataBus(value);

  // pulse EEPROM control pins
  PORTC &= 0b11111010; // EEPROM CE, WE low
  delayMicroseconds(1);
  PORTC |= 0b00000101; // EEPROM CE, WE high

  enterStandbyMode();
}

/**
 * Loads a large block of data to the EEPROM, beginning at address 0x0000
 */
int loadChip(unsigned int length) {
  unsigned int idx = 0;
  unsigned char packet[MAX_PAYLOAD_SIZE + 1];

  while (idx < length) {
    // Signal acknowledgement of previous packet and readiness for next packet
    sendAck();
    int packetLen = receivePacket(packet, MAX_PAYLOAD_SIZE + 1, false);
    writeChipPage(idx, packet, packetLen);

    idx += packetLen;
  }
}

/**
 * Writes a contiguous block (up to 64 bytes) to the EEPROM using the AT28C256's page mode
 * @param address 15-bit starting address
 * @param data Data buffer to write
 * @param length Length of data buffer
 */
int writeChipPage(unsigned int address, unsigned char *data, unsigned int length) {
  for (unsigned int idx = 0; idx < length; idx++) {
    writeChipByte(address + idx, data[idx]);
    delayMicroseconds(1);
  }

  waitForChip();
}

/**
 * Sets the given address on the EEPROM's address pins
 * @param address 15-bit address
 */
void setAddress(unsigned int address) {
  for (int i = 15; i >= 0; i--) {
    // Set/clear bit on SER pin
    if (bitRead(address, i)) {
      PORTC |= 0b00010000;
    }
    else {
      PORTC &= 0b11101111;
    }

    // Pulse SRCLK pin
    PORTB |= 0b00001000;
    PORTB &= 0b11110111;
  }

  // Pulse RCLK pin
  PORTB |= 0b00010000;
  PORTB &= 0b11101111;
}

/**
 * Reads a data byte from the I/O pins.
 * The caller must set the data I/O pins to INPUT before calling this function, 
 * otherwise the byte returned will be incorrect.
 */
unsigned char readDataBus() {
  /* 
  bit = port #
    0 = D2
    1 = D3
    ...
    5 = D7
    6 = B0
    7 = B1
  */
  return (PIND >> 2) | (PINB << 6);
}

/**
 * Sets a data byte onto the I/O pins.
 * The caller must set the data I/O pins to OUTPUT before calling this function, 
 * otherwise no data will be written.
 * @param value Byte value
 */
void writeDataBus(unsigned char value) {
  /* 
  bit = port #
    0 = D2
    1 = D3
    ...
    5 = D7
    6 = B0
    7 = B1

  lowest 6 bits go to highest 6 pins of port D
  highest 2 bits go to lowest 2 pins of port B
  */
  PORTD = (PORTD & 0b00000011) | (value << 2);
  PORTB = (PORTB & 0b11111100) | (value >> 6);
}

/**
 * Waits for the EEPROM to finish its internal write operation
 */
void waitForChip() {
  DDRB &= 0b11111110;  // Set data pin 6 (PORTB[0]) to INPUT
  PORTC |= 0b00000111; // EEPROM CE, OE, WE high

  delay(1);

  int values[3];

  do {
    for (int i = 0; i < 3; i++) {
      delayMicroseconds(2);
      PORTC &= 0b11111001; // EEPROM CE, OE low
      delayMicroseconds(2);
      values[i] = PINB & 1; // set value to data bit 6
      PORTC |= 0b00000110;  // EEPROM CE, OE high
    }
  } while (values[0] != values[1] || values[1] != values[2]);

  enterStandbyMode();
}

/**
 * Flashes the Activity LED according to the given code
 * @param c Byte representing a bit-terminated morse code sequence
 */
void flashCodeLED(unsigned char c) {
  while (c != 1) {
    PORTB |= 0b00000100; // LED high
    delay((c & 1) ? DASHLEN : DOTLEN);
    PORTB &= 0b11111011; // LED low
    delay(DOTLEN);
    c >>= 1;
  }
}
