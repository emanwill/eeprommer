const to15Bits = (x) => {
  const b = Buffer.alloc(2);
  b.writeUInt16BE(x & 0x7fff);
  return b;
};

const to16Bits = (x) => {
  const b = Buffer.alloc(2);
  b.writeUInt16BE(x & 0xffff);
  return b;
};

export class SerialMsgFactory {
  /**
   * Creates a command to read a byte at the given address.
   * @param {number} address 15-bit address
   */
  static read(address) {
    return Buffer.concat([Buffer.from([3, 0x72]), to15Bits(address)]);
  }

  /**
   * Creates a command to write a byte to the given address.
   * @param {number} address 15-bit address
   * @param {number} byte Single byte to write
   */
  static write(address, byte) {
    return Buffer.concat([
      Buffer.from([4, 0x77]),
      to15Bits(address),
      Buffer.from([byte]),
    ]);
  }

  /**
   * Creates a command to dump the entire contents of the EEPROM
   */
  static dump() {
    return Buffer.from([1, 0x64]);
  }

  /**
   * Creates a command to load the given data packet to the EEPROM, starting at address 0.
   *
   * The command returned by this function takes the form of multiple packets to be sent in sequence.
   * @param {Buffer} data Up to 32,768 bytes of data
   */
  static load(data) {
    if (data.length > 1 << 15) throw Error("Data packet is too large");
    if (data.length === 0) return [];

    const header = Buffer.concat([
      Buffer.from([3, 0x6c]),
      to16Bits(data.length),
    ]);

    const chunks = [];
    const bytes = Array.from(data);
    while (bytes.length) chunks.push(bytes.splice(0, 32));

    const packets = chunks.map((p) =>
      Buffer.concat([Buffer.from([p.length]), Buffer.from(p)])
    );
    packets.unshift(header);

    return packets;
  }

  static reset() {
    return Buffer.from([1, 0x73]);
  }

  static ack() {
    return Buffer.from([0]);
  }
}
