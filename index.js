#!/usr/bin/env node

const onetime = require("onetime");
const signalExit = require("signal-exit");

// Restore cursor if process fails
onetime(() => {
  signalExit(
    () => {
      process.stderr.write("\u001B[?25h");
    },
    { alwaysLast: true }
  );
})();

const { program } = require("commander");
const { getConfig } = require("./lib/util");

const DUMP = require("./lib/dump");
const GET = require("./lib/get");
const LIST = require("./lib/list");
const LOAD = require("./lib/load");
const READ = require("./lib/read");
const SET = require("./lib/set");
const WRITE = require("./lib/write");

const config = getConfig();

program.version("0.0.1");

program
  .command("dump <target_file>")
  .alias("d")
  .description("dumps the EEPROM contents to a file", {
    target_file: "name of target file for dump contents",
  })
  .option("-p, --port <port>", "EEPROMMER serial port", config.port)
  .action(DUMP);

program
  .command("get <key>")
  .description("prints a value from the CLI config", {
    key: "key",
  })
  .action(GET);

program
  .command("list")
  .alias("ls")
  .description("lists available ports")
  .action(LIST);

program
  .command("load <source_file>")
  .alias("ld")
  .description("dumps the EEPROM contents to the terminal or a file", {
    source_file: "name of binary source file",
  })
  .option("-p, --port <port>", "EEPROMMER serial port", config.port)
  .option("-v, --validate", "validate EEPROM contents after loading", false)
  .action(LOAD);

program
  .command("read <address>")
  .alias("r")
  .description("reads a byte from the EEPROM", {
    address: "15-bit address, in decimal or hex; e.g. '31250' or '0x8e01'",
  })
  .option("-p, --port <port>", "EEPROMMER serial port", config.port)
  .action(READ);

program
  .command("set <key> <value>")
  .description("sets a value in the CLI config", {
    key: "key",
    value: "value",
  })
  .action(SET);

program
  .command("write <address> <byte>")
  .alias("w")
  .description("writes a byte to the EEPROM", {
    address: "15-bit address, in decimal or hex; e.g. '17925' or '0x0c21'",
    byte: "single byte, in decimal or hex; e.g. '83' or '0xff'",
  })
  .option("-p, --port <port>", "EEPROMMER serial port", config.port)
  .action(WRITE);

program.parseAsync(process.argv).catch(console.error);

if (process.argv.length < 2) {
  program.outputHelp();
}
