const fs = require("fs");
const path = require("path");
const { SerialPortManager, toHexStr, createProgressBar } = require("./util");
const { SerialMsgFactory } = require("./serial-msg-factory");

/**
 *
 * @param {string} targetFile
 * @param {import("commander").Command} cmd
 */
const DUMP = async (targetFile, cmd) => {
  const DUMP_SIZE = 0x8000; // Total expected memory dump size, in bytes
  const COL_SIZE = 16;
  const HALF_COL = 8;

  let buffer = Buffer.alloc(0);
  let packet;
  let manager;

  console.log("Dumping memory contents...");

  const getProgress = () => buffer.length / DUMP_SIZE;
  const terminateProgressBar = createProgressBar(getProgress);

  try {
    manager = new SerialPortManager(cmd.port);

    const requestData = manager.createSendExpectData();

    packet = await requestData(SerialMsgFactory.dump());

    buffer = Buffer.concat([buffer, packet]);

    while (getProgress() < 1) {
      packet = await requestData(SerialMsgFactory.ack());
      buffer = Buffer.concat([buffer, packet]);
    }

    await manager.createSendWithoutResponse()(SerialMsgFactory.ack());

    terminateProgressBar("Memory dump fully received");

    manager.close();
  } catch (e) {
    terminateProgressBar();
    console.error(e);
    if (manager) manager.close();
    process.exit(1);
  }

  const bytesToStr = (start, end) =>
    Array.from(buffer.slice(start, end))
      .map((n) => toHexStr(n, 2))
      .join(" ");

  const bytesToCodes = (start, end) =>
    Array.from(buffer.slice(start, end))
      .map((n) => (n > 32 && n < 127 ? String.fromCharCode(n) : "."))
      .join("");

  if (buffer.length > 0) {
    try {
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

      const filepath = path.resolve(process.cwd(), targetFile);

      fs.writeFileSync(filepath, dumpString, "utf-8");
    } catch (e) {
      console.error(e);
      process.exit(1);
    }
  }
};

module.exports = DUMP;
