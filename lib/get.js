import { getConfig } from "./util.js";

/**
 *
 * @param {string} key
 * @param {import("commander").Command} _cmd
 */
const GET = async (key, _cmd) => {
  const config = getConfig();

  console.log(config[key]);
};

export default GET;
