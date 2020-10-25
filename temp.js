const { writeFile } = require("fs");
const { promisify } = require("util");
const SerialPort = require("serialport");
const { SerialMsgParser, AckMsgParser } = require("./serial-msg-parser");
const { SerialMsgFactory } = require("./serial-msg-factory");

const port = new SerialPort("COM3", { baudRate: 9600 }, (err) => {
  if (err) console.log("got an error on open:", err.message);
});
const parser = port.pipe(new SerialMsgParser());
const acker = port.pipe(new AckMsgParser());

const ACK_MSG = Buffer.from([0]);

const promiseTimeout = (ms) => new Promise((r) => setTimeout(r, ms));

const createSendAndExpect = (onMsg, onAck, onErr) => (data) => {
  const _onMsg = (d) => {
    acker.off("data", _onAck);
    onMsg(d);
  };
  const _onAck = (d) => {
    parser.off("data", _onMsg);
    onAck(d);
  };
  parser.once("data", _onMsg);
  acker.once("data", _onAck);
  port.write(data, (err) => {
    if (err) {
      parser.off("data", _onMsg);
      acker.off("data", _onAck);
      onErr(err);
    }
  });
  // console.log("sent", Array.from(data).toString());
};

const sendWithoutResponse = (data) =>
  new Promise((res, rej) => {
    port.write(data, (err) => (err ? rej(err) : res()));
    // console.log("sent", Array.from(data).toString());
  });

/**
 *
 * @param {string| number[] | Buffer} data
 * @returns {Promise<void>}
 */
const sendAndExpectAck = (data) =>
  new Promise((res, rej) => {
    createSendAndExpect(() => rej(Error("unexpected data")), res, rej)(data);
  });

/**
 *
 * @param {string | number[] | Buffer} data
 * @returns {Promise<Buffer>}
 */
const sendAndExpectResponse = (data) =>
  new Promise((res, rej) => {
    createSendAndExpect(res, () => rej(Error("unexpected ACK")), rej)(data);
  });

/**
 *
 * @param {string} filename
 */
const dumpMemory = async (filename) => {
  console.log("");
  process.stdout;
  const DUMP_SIZE = 0x8000;
  let idx = 0;
  let dumpBuffer = Buffer.alloc(0);
  let packet;
  const dumpCmd = SerialMsgFactory.dump();

  const updateConsole = () => {
    // process.stdout.clearLine();
    process.stdout.cursorTo(0);
    const pct = ((idx / DUMP_SIZE) * 100).toFixed(0);
    process.stdout.write(`Dumping EEPROM contents, ${pct}% complete`);
  };

  packet = await sendAndExpectResponse(dumpCmd);
  dumpBuffer = Buffer.concat([dumpBuffer, packet]);
  idx = dumpBuffer.length;

  updateConsole();

  while (idx < DUMP_SIZE) {
    packet = await sendAndExpectResponse(ACK_MSG);

    dumpBuffer = Buffer.concat([dumpBuffer, packet]);

    idx = dumpBuffer.length;
    updateConsole();
  }

  await sendWithoutResponse(ACK_MSG);

  process.stdout.write("\n");

  console.log("Writing dump to file " + filename);

  let str = "";
  let i = 0;
  for (let row = 0; row < 0x8000 / 16; row++) {
    i = row * 16;
    const sStart = i.toString(16).padStart(4, "0");
    const sEnd = (i + 15).toString(16).padStart(4, "0");
    const bytes = Array.from(dumpBuffer.slice(i, i + 16))
      .map((n) => n.toString(16).padStart(2, "0"))
      .join(" ");
    str += `$${sStart} - $${sEnd}:  ${bytes}\n`;
  }

  await promisify(writeFile)(filename, str);
};

/**
 *
 * @param {string | number[] | Buffer} data
 */
const uploadData = async (data) => {
  console.log("");
  const commands = SerialMsgFactory.load(data);
  let bytes = 0;

  const updateConsole = () => {
    // process.stdout.clearLine();
    process.stdout.cursorTo(0);
    const pct = ((bytes / data.length) * 100).toFixed(0);
    process.stdout.write(`Loading data to EEPROM, ${pct}% complete`);
  };

  for (let [i, command] of commands.entries()) {
    await sendAndExpectAck(command);
    if (i > 0) bytes += command[0];
    updateConsole();
  }

  process.stdout.write("\n");
};

const data = Buffer.from(
  Array(32768)
    .fill(1)
);

port.on("open", () => {
  console.log("the port is open");

  acker.once("data", () => {

    Promise.resolve()
      .then(() => uploadData(data))
      .then(() => promiseTimeout(100))
      .then(() => dumpMemory("dump01.txt"))
      .then(() => console.log("done"))
      .catch(console.error)
      .finally(() => {
        port.close();
      });
  });
});
