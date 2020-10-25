const bindings = require("@serialport/bindings");

function jsonl(ports) {
  ports.forEach((port) => {
    console.log(JSON.stringify(port));
  });
}

const formatters = {
  text(ports) {
    ports.forEach((port) => {
      console.log(
        `${port.path}\t${port.pnpId || ""}\t${port.manufacturer || ""}`
      );
    });
  },
  json(ports) {
    console.log(JSON.stringify(ports));
  },
  jsonl,
  jsonline: jsonl,
};

bindings.list().then(formatters[args.format], (err) => {
  console.error(JSON.stringify(err));
  process.exit(1);
});
