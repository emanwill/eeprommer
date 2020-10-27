const SerialPort = require("serialport");
const { SerialMsgParser, AckMsgParser } = require("./serial-msg-parser");
const fs = require("fs");
const path = require("path");
const ini = require("ini");

const CFG_FILENAME = "../config.ini";
const CFG_PATH = path.join(__dirname, CFG_FILENAME);
const DEFAULT_CFG = {
  port: "COM1",
  baud_rate: 9600,
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

/**
 * 
 * @param {SerialPortManager} self 
 * @param {(data: SerialData) => void} onData 
 * @param {() => void} onAck 
 * @param {(err: any) => void} onErr 
 * @return {(data: SerialData) => void}
 */
const createSendAndExpect = (self, onData, onAck, onErr) => {
  return (data) => {
    const _onData = (d) => {
      self._acker.off("data", _onAck);
      onData(d);
    };
    const _onAck = () => {
      self._parser.off("data", _onData);
      onAck();
    };
    self._parser.once("data", _onData);
    self._acker.once("data", _onAck);
    self._port.write(data, (err) => {
      if (err) {
        self._parser.off("data", _onData);
        self._acker.off("data", _onAck);
        onErr(err);
      }
    });
  };
}

class SerialPortManager {
  /**
   *
   * @param {string} portAddr
   */
  constructor(portAddr) {
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
          rej(err);
        } else {
          console.log(`Connected on port ${this._port.path}`);
        }
      });
    });
  }

  /**
   * @param {SerialData} data 
   * @returns {Promise<SerialData>}
   */
  async sendExpectResponse(data) {
    return new Promise((res, rej) => {
      const onAck = () => rej(Error("unexpected serial ACK"));
      const f = createSendAndExpect(this, res, onAck, rej);
      f(data);
    });
  }

  /**
   * @param {SerialData} data 
   * @returns {Promise<void>}
   */
  async sendExpectAck(data) {
    return new Promise((res, rej) => {
      const onData = (d) => rej(Error("unexpected serial data " + d));
      const f = createSendAndExpect(this, onData, res, rej);
      f(data);
    });
  }

  /**
   * @param {SerialData} data 
   * @returns {Promise<void>}
   */
  async sendBlind(data) {
    return new Promise((res, rej) => {
      this._port.write(data, (err) => (err ? rej(err) : res()));
    });
  }

  disconnect() {
    this._port.close();
    console.log("Port closed");
  }
}

module.exports = {
  getConfig,
  toHexStr,
  SerialPortManager,
};
