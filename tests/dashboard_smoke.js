// Recovery Center compatibility smoke test.
//
// Runs recovery-center/app.js against an exported dashboard JSON inside a
// minimal fake DOM (no browser, no network). Fails if rendering throws or if
// the core panels stay empty. Usage: node dashboard_smoke.js <report.json>
"use strict";

const fs = require("fs");
const path = require("path");
const vm = require("vm");

class FakeElement {
  constructor(tag, id) {
    this.tagName = (tag || "div").toUpperCase();
    this.id = id || "";
    this.children = [];
    this.className = "";
    this.textContent = "";
    this.style = {};
    this.dataset = {};
    this.attributes = {};
    this.listeners = {};
    const classes = new Set();
    this.classList = {
      add: (...c) => c.forEach(x => classes.add(x)),
      remove: (...c) => c.forEach(x => classes.delete(x)),
      toggle: (c, on) => ((on ?? !classes.has(c)) ? classes.add(c) : classes.delete(c)),
      contains: c => classes.has(c),
    };
  }
  set innerHTML(v) { if (v === "") this.children = []; this._html = v; }
  get innerHTML() { return this._html || ""; }
  appendChild(child) { this.children.push(child); return child; }
  append(...kids) { kids.forEach(k => this.appendChild(k)); }
  replaceChildren(...kids) { this.children = kids; }
  setAttribute(k, v) { this.attributes[k] = String(v); }
  getAttribute(k) { return this.attributes[k]; }
  addEventListener(type, fn) { this.listeners[type] = fn; }
  querySelector() { return new FakeElement("div"); }
  querySelectorAll() { return []; }
  closest() { return null; }
  get childElementCount() { return this.children.length; }
}

const byId = new Map();
const document = {
  getElementById(id) {
    if (!byId.has(id)) byId.set(id, new FakeElement("div", id));
    return byId.get(id);
  },
  createElement: tag => new FakeElement(tag),
  createTextNode: text => Object.assign(new FakeElement("#text"), { textContent: text }),
  querySelector: () => new FakeElement("div"),
  querySelectorAll: () => [],
  addEventListener() {},
};

const data = JSON.parse(fs.readFileSync(process.argv[2], "utf8"));
const window = { RECOVERY_DATA: data };
const context = vm.createContext({
  window, document, console, navigator: { clipboard: { writeText: async () => {} } },
  fetch: () => Promise.reject(new Error("network disabled in smoke test")),
  setTimeout, clearTimeout, Number, String, JSON, Math, Date, Array, Object, Set, Map,
});

const appPath = path.join(__dirname, "..", "recovery-center", "app.js");
try {
  vm.runInContext(fs.readFileSync(appPath, "utf8"), context, { filename: appPath });
} catch (err) {
  console.error("render threw:", err && err.stack || err);
  process.exit(1);
}

const dashboard = byId.get("dashboard");
if (!dashboard || dashboard.classList.contains("hidden")) {
  console.error("dashboard stayed hidden - data rejected");
  process.exit(1);
}
const populated = [...byId.values()].filter(e => e.children.length || e.textContent).length;
if (populated < 10) {
  console.error(`only ${populated} elements populated`);
  process.exit(1);
}
console.log(`dashboard smoke ok: ${populated} elements populated`);
