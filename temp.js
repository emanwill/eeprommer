const cliProgress = require("cli-progress");

// create a new progress bar instance and use shades_classic theme
const bar1 = new cliProgress.SingleBar(
  {
    format:
      "My Progress | {bar} | {percentage}% || {value}/{total} Chunks",
  },
  cliProgress.Presets.shades_classic
);

const promiseTimeout = (ms) => new Promise((r) => setTimeout(r, ms));

(async () => {
  // start the progress bar with a total value of 200 and start value of 0
  bar1.start(200, 0);

  for (let i = 0; i < 200; i++) {
    // update the current value in your application..
    bar1.update(i);

    await promiseTimeout(10);

    if (i > 147) {
      throw Error("oops!");
    }
  }

  bar1.update(200);

  // stop the progress bar
  bar1.stop();
})().catch(e => {
  bar1.stop();
  console.error(e)
});
