const fs = require("fs");
const path = require("path");
const cliProgress = require("cli-progress");
const { SerialPortManager } = require("./util");
const { SerialMsgFactory } = require("./serial-msg-factory");
const { getMemoryDump } = require("./dump");

/**
 *
 * @param {SerialPortManager} manager
 * @param {Buffer} buffer
 * @param {boolean} [showProgress]
 */
const uploadBuffer = async (manager, buffer, showProgress) => {
  let uploadProgress;
  if (showProgress) {
    uploadProgress = new cliProgress.SingleBar(
      {
        format: " {bar} {percentage}% | {value}/{total} bytes",
        hideCursor: true,
        stopOnComplete: true,
      },
      cliProgress.Presets.shades_classic
    );

    uploadProgress.start(buffer.length, 0);
  }

  try {
    const messages = SerialMsgFactory.load(buffer);

    let bytesSent = 0;
    for (let [idx, msg] of messages.entries()) {
      await manager.sendExpectAck(msg);
      if (idx > 0) bytesSent += msg[0];
      if (uploadProgress) uploadProgress.update(bytesSent);
    }
  } catch (e) {
    if (uploadProgress) uploadProgress.stop();
    throw e;
  }
};

/**
 *
 * @param {string} sourceFile
 * @param {import("commander").Command} cmd
 */
const LOAD = async (sourceFile, cmd) => {
  /** @type {Buffer} */
  let source;
  let manager;

  try {
    const filepath = path.resolve(process.cwd(), sourceFile);
    const sourceString = fs.readFileSync(filepath);
    source = Buffer.from(sourceString);
    console.log(`Read ${source.length} bytes from source file ${sourceFile}`);

    manager = new SerialPortManager(cmd.port);

    await manager.connect();

    console.log(`\nLoading to EEPROM...`);

    await uploadBuffer(manager, source, true);

    if (cmd.validate) {
      console.log("\nValidating EEPROM contents...");

      const dumpBuffer = await getMemoryDump(manager, true);

      const sourceDump = dumpBuffer.slice(0, source.length);
      const bytesMatch = Buffer.compare(source, sourceDump) === 0;
      if (bytesMatch) {
        console.log("Validation successful, EEPROM contents match source");
      } else {
        console.log("Validation failed, EEPROM contents differ from source");
      }
    }
    manager.disconnect();
    console.log("done");
  } catch (e) {
    if (manager) manager.disconnect();
    console.error(e);
    process.exit(1);
  }
};

module.exports = LOAD;
