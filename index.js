const fs = require("fs");
const { promisify } = require("util");
const SerialPort = require("serialport");
const argv = require("minimist")(process.argv.slice(2));
const { SerialMsgFactory } = require("./serial-msg-factory");
const { SerialMsgParser } = require("./serial-msg-parser");

const readFilePromise = promisify(fs.readFile);

const CMDS = ["read", "write", "dump", "load"];
if (argv._.length === 0 || !CMDS.includes(argv._[0])) {
  console.log('Please specify a command: "read", "write, "dump", or "load"');
  process.exit(1);
}
const CMD = argv._[0];

console.log(argv);

const isValidByte = (b) => {
  const n = parseInt(b);
  return !isNaN(n) && n >= 0 && n < 1 << 8;
};

const isValidAddress = (a) => {
  const n = parseInt(a);
  return !isNaN(n) && a >= 0 && a < 1 << 15;
};

if (!argv.p) {
  console.log(`Please specify a port using "-p", e.g. "-p COM1"`);
  process.exit(1);
}

if (CMD === "read" && (!argv.a || !isValidAddress(argv.a))) {
  console.log(`Please specify a valid address using "-a", e.g. "-a 0x01a7"`);
  process.exit(1);
}

if (CMD === "write" && (!argv.a || !isValidAddress(argv.a))) {
  console.log(`Please specify a valid address using "-a", e.g. "-a 0x01a7"`);
  process.exit(1);
}

if (CMD === "write" && (!argv.b || !isValidByte(argv.b))) {
  console.log(`Please specify a valid byte using "-b", e.g. "-b 0xa0"`);
  process.exit(1);
}

if (CMD === "load" && !argv.f) {
  console.log(`Please specify a file using "-f", e.g. "-f myfile.bin"`);
  process.exit(1);
}

const port = new SerialPort(argv.p, { baudRate: 9600 });
// const parser = port.pipe(new SerialMsgParser());

const send = (data) =>
  new Promise((res, rej) =>
    port.write(data, (err, data) => (err ? rej(err) : res(data)))
  );

port.on("open", () => {
  console.log("serial port open");

  port.on("close", () => {
    console.log("HEY!");
  });

  let promise = Promise.resolve();
  let command;
  switch (CMD) {
    case "read":
      port.on("data", (data) => {
        console.log("got stuff:", data);
      });
      command = SerialMsgFactory.read(parseInt(argv.a));
      console.log(command);
      promise = promise.then(() => send(command));
      break;
    case "dump":
      parser.on("data", (data) => {
        console.log(data);
      });
      promise = promise.then(() => send(SerialMsgFactory.dump()));
      break;
  }

  promise
    .finally(() => {
      // port.close();
    })
    .catch(console.error);
});
