const fs = require("fs");
const path = require("path");
const cliProgress = require("cli-progress");
const { SerialPortManager, toHexStr } = require("./util");
const { SerialMsgFactory } = require("./serial-msg-factory");

const DUMP_SIZE = 0x8000; // Total expected memory dump size, in bytes

/**
 *
 * @param {SerialPortManager} manager
 * @param {boolean} [showProgress]
 */
const getMemoryDump = async (manager, showProgress) => {
  let dumpProgress;
  if (showProgress) {
    dumpProgress = new cliProgress.SingleBar(
      {
        format: " {bar} {percentage}% | {value}/{total} bytes",
        hideCursor: true,
        stopOnComplete: true,
      },
      cliProgress.Presets.shades_classic
    );

    dumpProgress.start(DUMP_SIZE, 0);
  }

  try {
    let buffer = Buffer.alloc(0);
    let packet;

    packet = await manager.sendExpectResponse(SerialMsgFactory.dump());
    buffer = Buffer.concat([buffer, packet]);
    if (dumpProgress) dumpProgress.update(buffer.length);

    while (buffer.length < DUMP_SIZE) {
      packet = await manager.sendExpectResponse(SerialMsgFactory.ack());
      buffer = Buffer.concat([buffer, packet]);
      if (dumpProgress) dumpProgress.update(buffer.length);
    }

    await manager.sendBlind(SerialMsgFactory.ack());
    
    return buffer;
  } catch (e) {
    if (dumpProgress) dumpProgress.stop();
    throw e;
  }
};

/**
 *
 * @param {Buffer} buffer
 * @param {string} filepath
 */
const writeToDumpFile = (buffer, filepath) => {
  const COL_SIZE = 16;
  const HALF_COL = 8;

  const bytesToStr = (start, end) =>
    Array.from(buffer.slice(start, end))
      .map((n) => toHexStr(n, 2))
      .join(" ");

  const bytesToCodes = (start, end) =>
    Array.from(buffer.slice(start, end))
      .map((n) => (n > 32 && n < 127 ? String.fromCharCode(n) : "."))
      .join("");

  const strings = [];
  for (let row = 0; row < DUMP_SIZE / COL_SIZE; row++) {
    let idx = row * COL_SIZE;
    const la = toHexStr(idx, 4);
    const ha = toHexStr(idx + COL_SIZE - 1, 4);
    const lb = bytesToStr(idx, idx + HALF_COL);
    const hb = bytesToStr(idx + HALF_COL, idx + COL_SIZE);
    const lc = bytesToCodes(idx, idx + HALF_COL);
    const hc = bytesToCodes(idx + HALF_COL, idx + COL_SIZE);
    strings.push(`$${la} - $${ha}:  ${lb}  ${hb}  ${lc} ${hc}`);

    if (row % 16 === 15) strings.push("");
  }
  const dumpString = strings.join("\n");

  fs.writeFileSync(filepath, dumpString, "utf-8");
};

/**
 *
 * @param {string} targetFile
 * @param {import("commander").Command} cmd
 */
const DUMP = async (targetFile, cmd) => {
  let buffer = Buffer.alloc(0);
  let manager;

  try {
    manager = new SerialPortManager(cmd.port);

    await manager.connect();

    console.log("Dumping memory contents...");
    buffer = await getMemoryDump(manager, true);

    manager.disconnect();
  } catch (e) {
    if (manager) manager.disconnect();
    console.error(e);
    process.exit(1);
  }

  const filepath = path.resolve(process.cwd(), targetFile);
  console.log(`Writing memory contents to file ${filepath}`);
  writeToDumpFile(buffer, filepath);

  console.log("done");
};

module.exports = DUMP;
module.exports.writeToDumpFile = writeToDumpFile;
module.exports.getMemoryDump = getMemoryDump;
