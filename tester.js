const fs = require("fs");
const ini = require("ini");
const { program } = require("commander");
const { O_DIRECT } = require("constants");
const config = ini.parse(fs.readFileSync("./config.ini", "utf-8"));

const doREAD = (address, cmd) => {
  console.log("reading from", address);
  if (cmd.port) console.log("got a port as well:", cmd.port);
};

const doWRITE = (address, byte, cmd) => {
  console.log(`writing ${byte} to address ${address}`);
  if (cmd.port) console.log("got a port as well:", cmd.port);
};

const doDUMP = (targetFile, cmd) => {
  console.log("dumping...");
  if (targetFile) console.log("target file is", targetFile);
  if (cmd.port) console.log("got a port as well:", cmd.port);
};

program.version("0.0.1");

program
  .command("read <address>")
  .alias("r")
  .description("reads a byte from the EEPROM", {
    address: "15-bit address, in decimal or hex; e.g. '0x12a7' or '31250'",
  })
  .option("-p, --port <port>", "EEPROMMER serial port", config.port)
  .action(doREAD);

program
  .command("write <address> <byte>")
  .alias("w")
  .description("writes a byte to the EEPROM", {
    address: "15-bit address, in decimal or hex; e.g. '0x8e01' or '17925'",
    byte: "single byte, in decimal or hex; e.g. '83' or '0xff'",
  })
  .option("-p, --port <port>", "EEPROMMER serial port", config.port)
  .action(doWRITE);

program
  .command("dump [target_file]")
  .alias("d")
  .description("dumps the EEPROM contents to the terminal or a file")
  .option("-p, --port <port>", "EEPROMMER serial port", config.port)
  .action(doDUMP);

program.parse(process.argv);

if (process.argv.length === 1) {
  program.outputHelp();
}
