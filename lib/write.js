const { SerialPortManager, toHexStr } = require("./util");
const { SerialMsgFactory } = require("./serial-msg-factory");

/**
 *
 * @param {string} address
 * @param {string} byte
 * @param {import("commander").Command} cmd
 */
const WRITE = async (address, byte, cmd) => {
  const addr = parseInt(address);
  const bt = parseInt(byte);

  if (isNaN(addr) || !Number.isInteger(addr) || addr < 0 || addr > 32767) {
    // address is invalid
    console.log("address value is invalid");
    cmd.outputHelp();
    process.exit(1);
  }

  if (isNaN(bt) || !Number.isInteger(bt) || bt < 0 || bt > 255) {
    console.log("byte value is invalid");
    cmd.outputHelp();
    process.exit(1);
  }

  const manager = new SerialPortManager(cmd.port);

  await manager.connect();

  await manager.sendExpectAck(SerialMsgFactory.write(addr, bt));

  const buf = await manager.sendExpectResponse(SerialMsgFactory.read(addr));

  manager.disconnect();

  if (buf[0] === bt) {
    console.log(`Wrote byte ${byte} to address ${address}`);
  } else {
    console.log("Write failed: actual value doesn't match expected.");
    console.log(`Expected: 0x${toHexStr(bt)}\tActual: 0x${toHexStr(buf[0])}`);
    process.exit(1);
  }
};

module.exports = WRITE;
