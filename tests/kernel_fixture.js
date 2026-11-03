// Execute one exported defensive fixture compiled from kernel C as WebAssembly.
"use strict";
const fs = require("fs");
const assert = require("assert");
(async () => {
  const {instance} = await WebAssembly.instantiate(fs.readFileSync(process.argv[2]));
  const test = process.argv[3];
  assert.strictEqual(instance.exports[test](), 1, `${test} failed`);
  console.log(`${test}: PASS`);
})().catch(error => { console.error(error.message); process.exitCode = 1; });
