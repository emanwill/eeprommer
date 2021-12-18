import { readFileSync } from "fs";
import { resolve } from "path";

/**
 *
 * @param {string} srcFilename
 * @param {import("commander").Command} cmd
 */
const TRUNCATE = async (srcFilename, cmd) => {
  const destFilename = cmd.name || srcFilename;
  const offset = cmd.offset;

  console.log("destFilename", destFilename);
  console.log("offset", offset);

  try {
    const filepath = resolve(process.cwd(), srcFilename);
    const sourceString = readFileSync(filepath);
    source = Buffer.from(sourceString);
  } catch (e) {
    console.error(e);
    process.exit(1);
  }
};

export default TRUNCATE;
