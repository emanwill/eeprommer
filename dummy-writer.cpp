/**
 * AT28C256 EEPROM reader and programmer
 * 
 * Arduino Pin  |  Circuit Pin
 * -------------+---------------
 *          D2  |  EEPROM Data 0 (pin 11)
 *          D3  |  EEPROM Data 1 (pin 12)
 *          D4  |  EEPROM Data 2 (pin 13)
 *          D5  |  EEPROM Data 3 (pin 15)
 *          D6  |  EEPROM Data 4 (pin 16)
 *          D7  |  EEPROM Data 5 (pin 17)
 *          D8  |  EEPROM Data 6 (pin 18)
 *          D9  |  EEPROM Data 7 (pin 19)
 *          A0  |  EEPROM Write Enable (pin 27, active low)
 *          A1  |  EEPROM Output Enable (pin 22, active low)
 *          A2  |  EEPROM Chip Enable (pin 20, active low)
 * -------------+---------------
 *          A3  |  74CH595 Output Enable (pin 13, active low)
 *          A4  |  74CH595 Serial Input (pin 14)
 *         D11  |  74CH595 Serial Clock (pin 11, active low)
 *         D12  |  74CH595 Register Clock (pin 12, active high)
 *         D13  |  74CH595 Clear (pin 10, active low)
 * -------------+---------------
 *         D10  |  Status LED
 */

#include <Arduino.h>

enum MODE {
  STANDBY,
  READ,
  WRITE
};

enum STATUS {
  OK,
  ERR_RESET,
  ERR_CORRUPT,
  ERR_UNEXPECTED,
  ERR_UNKNOWN
};

const unsigned int MAX_PAYLOAD_BYTES = 63;
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

const unsigned int STATUS_LED = 10;
const unsigned int SPEED = 12;
const unsigned int DOTLEN = 1200 / SPEED;
const unsigned int DASHLEN = 3 * DOTLEN;

MODE mode = STANDBY;
STATUS status = OK;

int receive(byte *buffer, size_t length, bool sendAck);
int send(byte *buffer, size_t length, bool waitForAck);

byte read(unsigned int address);
int write(unsigned int address, byte value);
int dump();
int load(unsigned int length);

void setAddressPins(unsigned int address);
void pulsePin(int pinNo, bool activeHigh);

int enterStandbyMode();
int enterReadMode();
int enterWriteMode();

void handleError();
void pulseLED(unsigned int msHigh, unsigned int msLow);

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(120000L);

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
}

void loop() {
  if (Serial.available() > 0) {
    byte buffer[MAX_PAYLOAD_BYTES + 1];
    digitalWrite(STATUS_LED, HIGH);

    const int length = receive(buffer, sizeof(buffer), false);
    if (length > 0) {
      byte command = buffer[0];
      if (command == 0x72 && length == 3) {
        // "Read" command
        byte value = read((buffer[1] << 8) + buffer[2]);
        send(&value, 1, false);
      } else if (command == 0x77 && length == 4) {
        // "Write" command
        write((buffer[1] << 8) + buffer[2], buffer[3]);
        send(NULL, 0, false);
      } else if (command == 0x64 && length == 1) {
        // "Dump" command
        dump();
      } else if (command == 0x6c && length == 3) {
        // "Load" command
        send(NULL, 0, false); // Acknowledge command message
        load((buffer[1] << 8) + buffer[2]);
      } else if (command == 0x73 && length == 1) {
        // "Reset" command
        // Ignore reset command; 
        // it's meaningless unless interrupting another operation
      } else {
        // Unknown message
        status = ERR_UNKNOWN;
      }
    }

    digitalWrite(STATUS_LED, LOW);
  }

  if (status != OK) {
    handleError();
  }
}

int receive(byte *buffer, size_t length, bool sendAck) {
  int b;

  // Wait for first byte (length byte) of incoming message
  do {
    b = Serial.read();
  } while (b == -1);

  // Retrieve remaining bytes of incoming message and store to buffer
  if (b > 0 && Serial.readBytes(buffer, min(b, length)) != b) {
    status = ERR_CORRUPT;
    return -1;
  }

  // Send ack message if indicated
  if (sendAck && send(NULL, 0, false) == -1) {
    return -1;
  }

  return b;
}

int send(byte *buffer, size_t length, bool waitForAck) {
  Serial.write(length);
  if (length > 0) {
    Serial.write(buffer, length);
  }

  if (waitForAck) {
    byte buffer[1 + MAX_PAYLOAD_BYTES];
    int ackLength = receive(buffer, MAX_PAYLOAD_BYTES, false);

    if (ackLength != 0) {
      if (ackLength == 1 && buffer[0] == 0x73) {
        status = ERR_RESET;
      } else if (ackLength != 1) {
        status = ERR_UNEXPECTED;
      }

      return -1;
    }
  }

  return 0;
}

byte read(unsigned int address) {
  enterReadMode();
  setAddressPins(address);
  delayMicroseconds(DELAY_MICROS);

  // Read byte from data pins
  byte value = 0;
  for (unsigned int i = 0; i < 8; i++) {
    value |= (digitalRead(DATA_PINS[i]) << i);
  }

  enterStandbyMode();
  return value;
}

int write(unsigned int address, byte value) {
  setAddressPins(address);
  enterWriteMode();

  // Load byte onto data pins
  for (unsigned int i = 0; i < 8; i++) {
    int bit = (address >> i) & 1;
    digitalWrite(DATA_PINS[i], bit ? HIGH : LOW);
  }
  delayMicroseconds(DELAY_MICROS);

  pulsePin(EEPROM_WE, false);
  enterStandbyMode();
}

int dump() {
  byte payload[MAX_PAYLOAD_BYTES];
  unsigned int i = 0;

  for (unsigned int address = 0; address < 0x8000; address++) {
    i = address % MAX_PAYLOAD_BYTES;

    if (address > 0 && i == 0) {
      if (send(payload, sizeof(payload), true) == -1) {
        // Abort command received
        return -1;
      }
    }

    payload[i++] = read(address);
  }

  if (i) {
    // Send any remaining bytes
    return send(payload, i, true);
  }

  return 0;
}

int load(unsigned int length) {
  unsigned int address = 0;
  byte buffer[MAX_PAYLOAD_BYTES + 1];

  while (address < length) {
    int count = receive(buffer, sizeof(buffer), true);
    if (count == -1) {
      // Unexpected error; abort
      return -1;
    }

    for (int i = 0; i < count; i++) {
      write(address++, buffer[i]);
      delay(10);
    }

    return 0;
  }
}

void setAddressPins(unsigned int address) {
  for (int i = 15; i >= 0; i--) {
    int bit = (address >> i) & 1;
    digitalWrite(SHIFT_SER, bit ? HIGH : LOW);
    delayMicroseconds(DELAY_MICROS);
    pulsePin(SHIFT_SER_CLK, true);
  }

  delayMicroseconds(DELAY_MICROS);
  pulsePin(SHIFT_REG_CLK, true);
}

void pulsePin(int pinNo, bool activeHigh) {
  digitalWrite(pinNo, activeHigh ? HIGH : LOW);
  delayMicroseconds(DELAY_MICROS);
  digitalWrite(pinNo, activeHigh ? LOW : HIGH);
  delayMicroseconds(DELAY_MICROS);
}

int enterStandbyMode() {
  if (mode != STANDBY) {
    for (unsigned int i = 0; i < 8; i++) {
      pinMode(DATA_PINS[i], INPUT);
    }

    digitalWrite(EEPROM_CE, HIGH);
    digitalWrite(EEPROM_OE, LOW);
    digitalWrite(EEPROM_WE, HIGH);

    delayMicroseconds(DELAY_MICROS);
  }

  mode = STANDBY;
  return 0;
}

int enterReadMode() {
  if (mode != READ) {
    for (unsigned int i = 0; i < 8; i++) {
      pinMode(DATA_PINS[i], INPUT);
    }

    digitalWrite(EEPROM_CE, LOW);
    digitalWrite(EEPROM_OE, LOW);
    digitalWrite(EEPROM_WE, HIGH);

    delayMicroseconds(DELAY_MICROS);
  }

  mode = READ;
  return 0;
}

int enterWriteMode() {
  if (mode != WRITE) {
    for (unsigned int i = 0; i < 8; i++) {
      pinMode(DATA_PINS[i], OUTPUT);
    }

    digitalWrite(EEPROM_CE, LOW);
    digitalWrite(EEPROM_OE, HIGH);
    digitalWrite(EEPROM_WE, HIGH);

    delayMicroseconds(DELAY_MICROS);
  }

  mode = WRITE;
  return 0;
}

void handleError() {
  switch (status) {
    case ERR_CORRUPT: // "C" = _._.
      pulseLED(DASHLEN, DOTLEN);
      pulseLED(DOTLEN, DOTLEN);
      pulseLED(DASHLEN, DOTLEN);
      pulseLED(DOTLEN, DOTLEN);
      break;
    case ERR_RESET: // "R" = ._.
      pulseLED(DOTLEN, DOTLEN);
      pulseLED(DASHLEN, DOTLEN);
      pulseLED(DOTLEN, DOTLEN);
      break;
    case ERR_UNEXPECTED: // "X" = _.._
      pulseLED(DASHLEN, DOTLEN);
      pulseLED(DOTLEN, DOTLEN);
      pulseLED(DOTLEN, DOTLEN);
      pulseLED(DASHLEN, DOTLEN);
      break;
    case ERR_UNKNOWN: // "U" = .._
      pulseLED(DOTLEN, DOTLEN);
      pulseLED(DOTLEN, DOTLEN);
      pulseLED(DASHLEN, DOTLEN);
      break;
  }

  status = OK;
}

void pulseLED(unsigned int msHigh, unsigned int msLow) {
  digitalWrite(STATUS_LED, HIGH);
  delay(msHigh);
  digitalWrite(STATUS_LED, LOW);
  delay(msLow);
}
