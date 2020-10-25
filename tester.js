
const { SerialMsgFactory } = require("./serial-msg-factory");


const data = Buffer.from(Array(270).fill(0x42));

const commands = SerialMsgFactory.load(data);

commands.forEach(c => console.log(c));
