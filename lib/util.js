const SerialPort = require("serialport");
const { SerialMsgParser, AckMsgParser } = require("./serial-msg-parser");
const fs = require("fs");
const path = require("path");
const ini = require("ini");
const onetime = require("onetime");
const signalExit = require("signal-exit");
const ansi = require("ansi");

// Restore cursor if process fails
onetime(() => {
  signalExit(
    () => {
      process.stderr.write("\u001B[?25h");
    },
    { alwaysLast: true }
  );
})();

const cursor = ansi(process.stdout);

const CFG_FILENAME = "../config.ini";
const CFG_PATH = path.join(__dirname, CFG_FILENAME);
const DEFAULT_CFG = {
  port: "COM1",
  baud_rate: 9600,
};

/**
 *
 * @param {() => number} cb Callback that returns progress as fraction from 0 to 1
 * @returns {(msg?: string) => void} Callback that terminates the progress bar
 */
const createProgressBar = (cb) => {
  const VW = process.stdout.columns - 2;
  const intvl = setInterval(() => {
    const nFill = Math.floor(Math.min(cb(), 1) * VW);
    const str = "[" + "▒".repeat(nFill) + " ".repeat(VW - nFill) + "]";
    cursor.green().horizontalAbsolute(0).eraseLine().write(str);
  }, 150);

  return (msg = "") => {
    clearInterval(intvl);
    cursor
      .horizontalAbsolute(0)
      .eraseLine()
      .reset()
      .write(msg + "\n");
  };
};

/**
 *
 * @param {number} num
 * @param {number} [digits]
 */
const toHexStr = (num, digits = 2) => num.toString(16).padStart(digits, "0");

const getConfig = () => {
  let config;

  try {
    config = ini.parse(fs.readFileSync(CFG_PATH, "utf-8"));
  } catch (e) {
    config = DEFAULT_CFG;
  }

  return config;
};

/**
 * @typedef {string | number[] | Buffer} SerialData
 */

class SerialPortManager {
  /**
   *
   * @param {string} portAddr
   * @param {import("serialport").OpenOptions} [options]
   */
  constructor(portAddr, options = {}) {
    const cfg = getConfig();

    this._port = new SerialPort(portAddr, {
      baudRate: parseInt(cfg.baud_rate),
      autoOpen: false,
    });

    this._parser = this._port.pipe(new SerialMsgParser());
    this._acker = this._port.pipe(new AckMsgParser());
  }

  async connect() {
    return new Promise((res, rej) => {
      this._acker.once("data", res);
      this._port.open((err) => {
        if (err) {
          console.log("Failed to open connection:");
          console.error(err);
          process.exit(1);
        } else {
          console.log(`Connected on port ${this._port.path}`);
        }
      });
    });
  }

  /**
   *
   * @param {(data: SerialData) => void} onMsg
   * @param {() => void} onAck
   * @param {(err: any) => void} onErr
   * @returns {(data: SerialData) => void}
   */
  _createSendAndExpect(onMsg, onAck, onErr) {
    return (data) => {
      const _onMsg = (d) => {
        this._acker.off("data", _onAck);
        onMsg(d);
      };
      const _onAck = () => {
        this._parser.off("data", _onMsg);
        onAck();
      };
      this._parser.once("data", _onMsg);
      this._acker.once("data", _onAck);
      this._port.write(data, (err) => {
        if (err) {
          this._parser.off("data", _onMsg);
          this._acker.off("data", _onAck);
          onErr(err);
        }
      });
    };
  }

  /**
   * @returns {(data: SerialData) => Promise<void>}
   */
  createSendWithoutResponse() {
    return (data) =>
      new Promise((res, rej) => {
        this._port.write(data, (err) => (err ? rej(err) : res()));
      });
  }

  /**
   * @returns {(data: SerialData) => Promise<void>}
   */
  createSendExpectAck() {
    return (data) =>
      new Promise((res, rej) => {
        const onMsg = (d) => rej(Error("unexpected serial data " + d));
        const f = this._createSendAndExpect(onMsg, res, rej);
        f(data);
      });
  }

  /**
   * @returns {(data: SerialData) => Promise<SerialData>}
   */
  createSendExpectData() {
    return (data) =>
      new Promise((res, rej) => {
        const onAck = () => rej(Error("unexpected serial ACK"));
        const f = this._createSendAndExpect(res, onAck, rej);
        f(data);
      });
  }

  close() {
    this._port.close();
    console.log("Port closed");
  }
}

module.exports = {
  createProgressBar,
  getConfig,
  toHexStr,
  SerialPortManager,
};
