const fs = require("fs");
const path = require("path");
const { SerialPortManager, createProgressBar } = require("./util");
const { SerialMsgFactory } = require("./serial-msg-factory");

/**
 *
 * @param {string} sourceFile
 * @param {import("commander").Command} cmd
 */
const LOAD = async (sourceFile, cmd) => {
  /** @type {Buffer} */
  let source;
  const filepath = path.resolve(process.cwd(), sourceFile);
  try {
    const sourceString = fs.readFileSync(filepath);
    source = Buffer.from(sourceString);
  } catch (e) {
    console.error(e);
    cmd.outputHelp();
    process.exit(1);
  }

  console.log(`Read ${source.length} bytes from source file ${sourceFile}`);

  const manager = new SerialPortManager();
  const messages = SerialMsgFactory.load(source);
  const sendExpectAck = manager.createSendExpectAck();
  const requestData = manager.createSendExpectData();

  console.log(`\nLoading to EEPROM...`);

  let bytesSent = 0;

  const getLoadProg = () => bytesSent / source.length;
  const terminateLoadProgBar = createProgressBar(getLoadProg);

  for (let [idx, msg] of messages.entries()) {
    await sendExpectAck(msg);
    if (idx > 0) bytesSent += msg[0];
  }

  terminateLoadProgBar("Loading complete");

  if (cmd.validate) {
    console.log("\nValidating EEPROM contents...");

    const DUMP_SIZE = 0x8000; // Total expected memory dump size, in bytes
    let buffer = Buffer.alloc(0);
    let packet;

    const getDumpProg = () => buffer.length / DUMP_SIZE;
    const terminateDumpProgBar = createProgressBar(getDumpProg);

    try {
      // Send DUMP command and await first data
      packet = await requestData(SerialMsgFactory.dump());
      buffer = Buffer.concat([buffer, packet]);
      while (getDumpProg() < 1) {
        // Acknowledge previous data and await next data
        packet = await requestData(SerialMsgFactory.ack());
        buffer = Buffer.concat([buffer, packet]);
      }

      terminateDumpProgBar("done");

      const sourceDump = buffer.slice(0, source.length);

      const bytesMatch = Buffer.compare(source, sourceDump) === 0;

      if (bytesMatch) {
        console.log("Validation successful, EEPROM contents match source");
      } else {
        console.log("Validation failed, EEPROM contents differ from source");
      }

    } catch (e) {
      terminateDumpProgBar();
      console.error(e);
    }

    manager.close();
  }
};

module.exports = LOAD;
