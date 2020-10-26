const { getConfig } = require("./util");

/**
 *
 * @param {string} key
 * @param {import("commander").Command} _cmd
 */
const GET = async (key, _cmd) => {
  const config = getConfig();

  console.log(config[key]);
};

module.exports = GET;
