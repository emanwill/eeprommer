import { Transform } from "stream";

export class SerialMsgParser extends Transform {
  constructor() {
    super();
    this.buffer = Buffer.alloc(0);
  }

  _transform(chunk, _enc, done) {
    this.buffer = Buffer.concat([this.buffer, chunk]);

    // Get message length from first byte
    const msgLen = this.buffer[0];

    if (this.buffer.length > msgLen) {
      this.push(this.buffer.slice(1, msgLen + 1));
      this.buffer = this.buffer.slice(msgLen + 1);
    }

    done();
  }

  _flush(done) {
    if (this.buffer.length) this.push(this.buffer);
    this.buffer = Buffer.alloc(0);
    done();
  }
}

export class AckMsgParser extends Transform {
  constructor() {
    super();
    this.buffer = Buffer.alloc(0);
  }

  _transform(chunk, _enc, done) {
    this.buffer = Buffer.concat([this.buffer, chunk]);

    // Get message length from first byte
    const msgLen = this.buffer[0];

    if (this.buffer.length > msgLen) {
      if (msgLen === 0) {
        this.push(Buffer.from([0]));
      }
      this.buffer = this.buffer.slice(msgLen + 1);
    }

    done();
  }

  _flush(done) {
    this.buffer = Buffer.alloc(0);
    done();
  }
}
