const { getConfig } = require("./util");
const fs = require("fs");
const path = require("path");
const ini = require("ini");
const { promisify } = require("util");

const promiseWriteFile = promisify(fs.writeFile);

const CFG_FILENAME = "../config.ini";
const CFG_PATH = path.join(__dirname, CFG_FILENAME);

/**
 *
 * @param {string} key
 * @param {string} value
 * @param {import("commander").Command} _cmd
 */
const SET = async (key, value, _cmd) => {
  const config = getConfig();

  config[key] = value;

  await promiseWriteFile(CFG_PATH, ini.stringify(config), "utf-8");
};

module.exports = SET;
