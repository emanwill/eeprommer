/**
  AT28C256 EEPROM reader and programmer

  Works with Arduino Nano Every


         Arduino Pin  |  Circuit Pin
  --------------------+------------------------------------
    PORTD.0 (pin A3)  |  EEPROM Data 0 (pin 11)
    PORTD.1 (pin A2)  |  EEPROM Data 1 (pin 12)
    PORTD.2 (pin A1)  |  EEPROM Data 2 (pin 13)
    PORTD.3 (pin A0)  |  EEPROM Data 3 (pin 15)
    PORTD.4 (pin A6)  |  EEPROM Data 4 (pin 16)
    PORTD.5 (pin A7)  |  EEPROM Data 5 (pin 17)
    PORTA.2 (pin A4)  |  EEPROM Data 6 (pin 18)
    PORTA.3 (pin A5)  |  EEPROM Data 7 (pin 19)
                      |
    PORTA.0 (pin D2)  |  EEPROM WE (pin 27, active low)
    PORTF.5 (pin D3)  |  EEPROM OE (pin 22, active low)
    PORTA.1 (pin D7)  |  EEPROM CE (pin 20, active low)
  --------------------+------------------------------------
    PORTB.2 (pin D5)  |  74CH595 SER (pin 14)
    PORTC.6 (pin D4)  |  74CH595 OE (pin 13, active low)
    PORTF.4 (pin D6)  |  74CH595 SRCLK (pin 11, active high)
    PORTE.3 (pin D8)  |  74CH595 RCLK (pin 12, active high)
    PORTB.0 (pin D9)  |  74CH595 RCLR (pin 10, active low)
  --------------------+------------------------------------
    PORTE.2 (pin D13) |  Status LED
*/

#include <Arduino.h>

enum MODE
{
  STANDBY,
  READ,
  WRITE
};

enum STATUS
{
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
const unsigned int SPEED = 10;
const unsigned int DOTLEN = 1200 / SPEED;
const unsigned int DASHLEN = 3 * DOTLEN;

MODE mode = STANDBY;
STATUS status = OK;

/**
 * Initializes program state
 */
void setup()
{
  Serial.begin(115200);

  // Initialize EEPROM control pins
  // PORT#         76543210
  PORTA.DIRSET = 0b00000011; // EEPROM CE, WE as outputs
  PORTA.OUTSET = 0b00000011; // EEPROM CE, WE high (inactive)
  PORTF.DIRSET = 0b00100000; // EEPROM OE as output
  PORTF.OUTSET = 0b00100000; // EEPROM OE high (inactive)

  // Initialize shift register control pins
  // PORT#         76543210
  PORTB.DIRSET = 0b00000101; // SHIFT RCLR, SER as outputs
  PORTB.OUTSET = 0b00000001; // SHIFT RCLR high (inactive)
  PORTB.OUTCLR = 0b00000100; // SHIFT SER low (binary 0)
  // PORT#         76543210
  PORTC.DIRSET = 0b01000000; // SHIFT OE as output
  PORTC.OUTCLR = 0b01000000; // SHIFT OE low (active)
  // PORT#         76543210
  PORTE.DIRSET = 0b00001000; // SHIFT RCLK as output
  PORTE.OUTCLR = 0b00001000; // SHIFT RCLK low (inactive)
  // PORT#         76543210
  PORTF.DIRSET = 0b00010000; // SHIFT SRCLK as output
  PORTF.OUTCLR = 0b00010000; // SHIFT SRCLK low (inactive)

  // Initialize status LED pin
  // PORT#         76543210
  PORTE.DIRSET = 0b00000100; // LED as output
  PORTE.OUTCLR = 0b00000100; // LED low

  enterStandbyMode();

  while (!Serial)
  {
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
void loop()
{
  while (Serial.available() > 0)
  {
    unsigned char packet[MAX_PAYLOAD_SIZE + 1];
    int packetLen = receivePacket(packet, sizeof(packet), false);

    // PORT#         76543210
    PORTE.OUTSET = 0b00000100; // LED high

    if (packetLen > 0)
    {
      unsigned char cmd = packet[0];
      if (cmd == 'r' && packetLen == 3)
      {
        // READ
        unsigned char value = readChipByte((packet[1] << 8) + packet[2]);
        sendPacket(&value, 1, false);
      }
      else if (cmd == 'd' && packetLen == 1)
      {
        // DUMP
        dumpChipBytes();
      }
      else if (cmd == 'w' && packetLen == 4)
      {
        // WRITE
        writeChipByte((packet[1] << 8) + packet[2], packet[3]);
        waitForChip();
        sendAck();
      }
      else if (cmd == 'l' && packetLen == 3)
      {
        // LOAD
        loadChip((packet[1] << 8) + packet[2]);
        sendAck();
      }
      else if (cmd == 's' && packetLen == 1)
      {
        // RESET
        // Do nothing; this command is meaningless in this context
      }
      else
      {
        status = ERR_INVALID_CMD;
      }
    }

    // PORT#         76543210
    PORTE.OUTCLR = 0b00000100; // LED low
  }

  if (status != OK)
  {
    delay(DASHLEN * 3);
    flashCodeLED(STATUS_CODES[status]);
    status = OK;
  }
}

/**
 * Sets I/O ports into STANDBY mode
 */
int enterStandbyMode()
{
  if (mode != STANDBY)
  {
    // Set EEPROM Data pins as input
    // PORT #        76543210
    PORTD.DIRCLR = 0b00111111; // EEPROM Data 0 through Data 5 input
    PORTA.DIRCLR = 0b00001100; // EEPROM Data 6 through Data 7 input

    // Set EEPROM control pins
    // PORT#         76543210
    PORTA.OUTSET = 0b00000011; // EEPROM CE, WE high (inactive)
    PORTF.OUTCLR = 0b00100000; // EEPROM OE low (active)

    delayMicroseconds(1);
  }

  mode = STANDBY;
  return 0;
}

/**
 * Sets I/O ports into EEPROM READ mode
 */
int enterReadMode()
{
  if (mode != READ)
  {
    // Set EEPROM Data pins as input
    // PORT #        76543210
    PORTD.DIRCLR = 0b00111111; // EEPROM Data 0 through Data 5 input
    PORTA.DIRCLR = 0b00001100; // EEPROM Data 6 through Data 7 input

    // Set EEPROM control pins
    // PORT#         76543210
    PORTA.OUTSET = 0b00000001; // EEPROM WE high (inactive)
    PORTA.OUTCLR = 0b00000010; // EEPROM CE low (active)
    PORTF.OUTCLR = 0b00100000; // EEPROM OE low (active)

    delayMicroseconds(1);
  }

  mode = READ;
  return 0;
}

/**
 * Sets I/O ports into EEPROM WRITE mode
 */
int enterWriteMode()
{
  if (mode != WRITE)
  {
    // Set EEPROM Data pins as output
    // PORT #        76543210
    PORTD.DIRSET = 0b00111111; // EEPROM Data 0 through Data 5 output
    PORTA.DIRSET = 0b00001100; // EEPROM Data 6 through Data 7 output

    // Set EEPROM control pins
    // PORT#         76543210
    PORTA.OUTSET = 0b00000001; // EEPROM WE high (inactive)
    PORTA.OUTCLR = 0b00000010; // EEPROM CE low (active)
    PORTF.OUTSET = 0b00100000; // EEPROM OE high (inactive)

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
int receivePacket(unsigned char *buffer, size_t maxLength, bool replyAck)
{
  int lenByte = 0;
  int lenReceived = 0;

  do
  {
    lenByte = Serial.read();
  } while (lenByte == -1);

  if (lenByte > 0)
  {
    lenReceived = Serial.readBytes(buffer, min(lenByte, maxLength));
    if (lenReceived != lenByte)
    {
      status = ERR_CORRUPT_PKT;
      return -1;
    }
  }

  if (replyAck)
  {
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
int sendPacket(unsigned char *packet, size_t length, bool waitForAck)
{
  Serial.write(length);
  if (length > 0)
  {
    Serial.write(packet, length);
  }

  if (waitForAck)
  {
    unsigned char buffer[MAX_PAYLOAD_SIZE + 1];
    int ackLen = receivePacket(buffer, MAX_PAYLOAD_SIZE, false);

    if (ackLen != 0)
    {
      if (ackLen == 1 && buffer[0] == 's')
      {
        status = ERR_RESET;
      }
      else if (ackLen != 1)
      {
        status = ERR_UNEXPECTED;
      }
      else
      {
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
int sendAck()
{
  return sendPacket(NULL, 0, false);
}

/**
 * Reads a byte from the EEPROM at the given address
 * @param address 15-bit address
 */
unsigned char readChipByte(unsigned int address)
{
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
int dumpChipBytes()
{
  enterReadMode();
  unsigned char packet[MAX_PAYLOAD_SIZE];
  int packetIdx = 0;

  for (unsigned int address = 0; address < 0x8000; address++)
  {
    packetIdx = address % MAX_PAYLOAD_SIZE;

    if (address > 0 && packetIdx == 0)
    {
      int packetStatus = sendPacket(packet, MAX_PAYLOAD_SIZE, true);
      if (packetStatus == -1)
      {
        enterStandbyMode();
        return -1;
      }
    }

    setAddress(address);
    unsigned char value = readDataBus();
    packet[packetIdx] = value;
  }

  enterStandbyMode();

  if (packetIdx > 0)
  {
    return sendPacket(packet, packetIdx + 1, true);
  }

  return 0;
}

/**
 * Writes a byte to the EEPROM at the given address
 * @param address 15-bit address
 * @param value byte value to write
 */
int writeChipByte(unsigned int address, unsigned char value)
{
  setAddress(address);
  enterWriteMode();

  // Load byte onto data pins
  writeDataBus(value);

  // pulse EEPROM control pins
  // PORT#         76543210
  PORTA.OUTCLR = 0b00000011; // EEPROM CE, WE low (active)
  delayMicroseconds(1);
  // PORT#         76543210
  PORTA.OUTSET = 0b00000011; // EEPROM CE, WE high (inactive)

  enterStandbyMode();
}

/**
 * Loads a large block of data to the EEPROM, beginning at address 0x0000
 */
int loadChip(unsigned int length)
{
  unsigned int idx = 0;
  unsigned char packet[MAX_PAYLOAD_SIZE + 1];

  while (idx < length)
  {
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
int writeChipPage(unsigned int address, unsigned char *data, unsigned int length)
{
  for (unsigned int idx = 0; idx < length; idx++)
  {
    writeChipByte(address + idx, data[idx]);
    delayMicroseconds(1);
  }

  waitForChip();
}

/**
 * Sets the given address on the EEPROM's address pins
 * @param address 15-bit address
 */
void setAddress(unsigned int address)
{
  for (int i = 15; i >= 0; i--)
  {
    // Set/clear bit on SER pin
    if (bitRead(address, i))
    {
      // PORT#         76543210
      PORTB.OUTSET = 0b00000100; // SHIFT SER high (binary 1)
    }
    else
    {
      // PORT#         76543210
      PORTB.OUTCLR = 0b00000100; // SHIFT SER low (binary 0)
    }

    // Pulse SRCLK
    // PORT#         76543210
    PORTF.OUTSET = 0b00010000; // SHIFT SRCLK high (active)
    PORTF.OUTCLR = 0b00010000; // SHIFT SRCLK low (inactive)
  }

  // Pulse RCLK
  // PORT#         76543210
  PORTE.OUTSET = 0b00001000; // SHIFT RCLK high (active)
  PORTE.OUTCLR = 0b00001000; // SHIFT RCLK low (inactive)
}

/**
 * Reads a data byte from the I/O pins.
 * The caller must set the data I/O pins to INPUT before calling this function, 
 * otherwise the byte returned will be incorrect.
 */
unsigned char readDataBus()
{
  // PORT #            76543210                   76543210
  return (PORTD.IN & 0b00111111) | ((PORTA.IN & 0b00001100) << 4);
}

/**
 * Sets a data byte onto the I/O pins.
 * The caller must set the data I/O pins to OUTPUT before calling this function, 
 * otherwise no data will be written.
 * @param value Byte value
 */
void writeDataBus(unsigned char value)
{
  // PORT#                   76543210
  PORTD.OUTSET =  (value & 0b00111111);
  PORTD.OUTCLR = (~value & 0b00111111);
  PORTA.OUTSET =  (value & 0b11000000) >> 4;
  PORTA.OUTCLR = (~value & 0b11000000) >> 4;
}

/**
 * Pauses the program until the EEPROM finishes its internal write operation
 */
void waitForChip()
{
  // Set EEPROM Data pin 6 as input
  // PORT#         76543210
  PORTA.DIRCLR = 0b00000100; // EEPROM Data 6 input

  // Set EEPROM control pins
  // PORT#         76543210
  PORTA.OUTSET = 0b00000011; // EEPROM CE, WE high (inactive)
  PORTF.OUTSET = 0b00100000; // EEPROM OE high (inactive)

  // Initially wait for 1 millisecond
  delay(1);

  int values[3];

  do
  {
    for (int i = 0; i < 3; i++)
    {
      delayMicroseconds(2);
      // PORT#         76543210
      PORTA.OUTCLR = 0b00000010; // EEPROM CE low (active)
      PORTF.OUTCLR = 0b00100000; // EEPROM OE low (active)
      delayMicroseconds(2);
      // PORT#                 76543210
      values[i] = PORTA.IN & 0b00000100; // set value to data bit 6
      // PORT#         76543210
      PORTA.OUTSET = 0b00000010; // EEPROM CE high (inactive)
      PORTF.OUTSET = 0b00100000; // EEPROM OE high (inactive)
    }
  } while (values[0] != values[1] || values[1] != values[2]);

  enterStandbyMode();
}

/**
 * Flashes the Activity LED according to the given code
 * @param c Byte representing a bit-terminated morse code sequence
 */
void flashCodeLED(unsigned char c)
{
  while (c != 1)
  {
    // PORT#         76543210
    PORTE.OUTSET = 0b00000100; // LED low
    delay((c & 1) ? DASHLEN : DOTLEN);
    // PORT#         76543210
    PORTE.OUTCLR = 0b00000100; // LED low
    delay(DOTLEN);
    c >>= 1;
  }
}
