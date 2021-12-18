#!/usr/bin/env node
// @ts-check

import onetime from "onetime";
import signalExit from "signal-exit";
import { program } from "commander";

import { getConfig } from "./lib/util.js";

import DUMP from "./lib/dump.js";
import GET from "./lib/get.js";
import LIST from "./lib/list.js";
import LOAD from "./lib/load.js";
import READ from "./lib/read.js";
import SET from "./lib/set.js";
import WRITE from "./lib/write.js";
import TRUNCATE from "./lib/truncate.js";

// Restore cursor if process fails
onetime(() => {
  signalExit(
    () => {
      process.stderr.write("\u001B[?25h");
    },
    { alwaysLast: true }
  );
})();

const config = getConfig();

program.version("0.0.1");

program
  .command("dump")
  .alias("d")
  .description("dumps the EEPROM contents to a file")
  .argument("<filename>", "dump file name")
  .option("-p, --port <port>", "EEPROMMER serial port", config.port)
  .action(DUMP);

program
  .command("get")
  .description("prints a value from the CLI config")
  .argument("<key>", "key")
  .action(GET);

program
  .command("list")
  .alias("ls")
  .description("lists available ports")
  .action(LIST);

program
  .command("load")
  .alias("ld")
  .description("dumps the EEPROM contents to the terminal or a file")
  .argument("<source_file>", "path to binary source file")
  .option("-p, --port <port>", "EEPROMMER serial port", config.port)
  .option("-v, --validate", "validate EEPROM contents after loading", false)
  .action(LOAD);

program
  .command("read")
  .alias("r")
  .description("reads a byte from the EEPROM")
  .argument(
    "<address>",
    "15-bit address, in decimal or hex; e.g. '31250' or '0x8e01'"
  )
  .option("-p, --port <port>", "EEPROMMER serial port", config.port)
  .action(READ);

program
  .command("set")
  .description("sets a value in the CLI config")
  .argument("<key>", "key")
  .argument("<value>", "value")
  .action(SET);

program
  .command("write")
  .alias("w")
  .description("writes a byte to the EEPROM")
  .argument(
    "<address>",
    "15-bit address, in decimal or hex; e.g. '17925' or '0x0c21'"
  )
  .argument("<byte>", "single byte, in decimal or hex; e.g. '83' or '0xff'")
  .option("-p, --port <port>", "EEPROMMER serial port", config.port)
  .action(WRITE);

program
  .command("truncate")
  .description("truncates a .bin file to the correct length")
  .alias("t")
  .argument("<filename>", "name of file to truncate")
  .option("-r, --rename <name>", "name of truncated file", "")
  .option("-o, --offset <bytes>", "Offset from start of file in bytes", "0")
  .action(TRUNCATE);

program.parseAsync(process.argv).catch(console.error);

if (process.argv.length < 2) {
  program.outputHelp();
}
