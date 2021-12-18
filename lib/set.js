import { getConfig } from "./util.js";
import { writeFile } from "fs";
import { join, resolve } from "path";
import { stringify } from "ini";
import { promisify } from "util";

const promiseWriteFile = promisify(writeFile);

const dir = resolve();

const CFG_FILENAME = "./config.ini";
const CFG_PATH = join(dir, CFG_FILENAME);

/**
 *
 * @param {string} key
 * @param {string} value
 * @param {import("commander").Command} _cmd
 */
const SET = async (key, value, _cmd) => {
  const config = getConfig();

  config[key] = value;

  await promiseWriteFile(CFG_PATH, stringify(config), "utf-8");
};

export default SET;
