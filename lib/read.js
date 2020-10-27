const { SerialPortManager, toHexStr } = require("./util");
const { SerialMsgFactory } = require("./serial-msg-factory");

/**
 *
 * @param {string} address
 * @param {import("commander").Command} cmd
 */
const READ = async (address, cmd) => {
  const addr = parseInt(address);

  if (isNaN(addr) || !Number.isInteger(addr) || addr < 0 || addr >= 1 << 15) {
    // address is invalid
    console.log("address is invalid");
    cmd.outputHelp();
    process.exit(1);
  }

  const manager = new SerialPortManager(cmd.port);

  await manager.connect();

  const request = manager.createSendExpectData();
  const buf = await request(SerialMsgFactory.read(addr));
  const byte = toHexStr(buf[0], 2);

  console.log(`byte at address ${address}: 0x${byte}`);

  manager.close();
};

module.exports = READ;
