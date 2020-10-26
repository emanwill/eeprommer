const bindings = require("@serialport/bindings");

/**
 * @param {import("commander").Command} _cmd
 */
const LIST = async (_cmd) => {
  try {
    const ports = await bindings.list();
    ports.forEach((p) => {
      console.log(`${p.path}\t${p.pnpId || ""}\t${p.manufacturer || ""}`);
    });
  } catch (e) {
    console.error(JSON.stringify(e));
    process.exit(1);
  }
};

module.exports = LIST;
