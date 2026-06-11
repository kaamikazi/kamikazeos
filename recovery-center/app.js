/*
 * KamikazeOS Recovery Center — rendering only.
 *
 * SAFETY CONTRACT OF THIS FILE: it reads a JSON snapshot and draws HTML.
 * There are no network calls to the OS, no writes, no way to apply an
 * action. The only "interactive" thing here is a button that copies a
 * shell command string to the clipboard — the human still has to type
 * or paste it inside KamikazeOS themselves.
 */
"use strict";

function scoreClass(score) {
  return score >= 70 ? "good" : score >= 40 ? "warn" : "bad";
}

function el(tag, cls, text) {
  const n = document.createElement(tag);
  if (cls) n.className = cls;
  if (text !== undefined) n.textContent = text;
  return n;
}

function renderTable(id, rows, columns) {
  const table = document.getElementById(id);
  table.innerHTML = "";
  const head = el("tr");
  columns.forEach(c => head.appendChild(el("th", null, c.label)));
  table.appendChild(head);
  rows.forEach(r => {
    const tr = el("tr");
    columns.forEach(c => {
      const value = r[c.key] ?? "";
      const td = el("td", c.statusClass ? `status-${value}` : null, value);
      tr.appendChild(td);
    });
    table.appendChild(tr);
  });
}

function render(data) {
  // Header + health hero
  document.getElementById("meta").textContent =
    `KamikazeOS v${data.version} · backend: ${data.backend} · ` +
    `boot ${data.memory?.boot_count ?? "?"} · ` +
    `journal entries: ${data.store?.journal_count ?? "?"}`;

  const score = parseInt(data.health?.score ?? "0", 10);
  const scoreEl = document.getElementById("health-score");
  scoreEl.textContent = `${score}`;
  scoreEl.className = `score ${scoreClass(score)}`;

  // The safety notice always comes from the data file, so the kernel-side
  // contract and the UI text can never drift apart silently.
  if (data.safety_notice)
    document.getElementById("safety-banner").textContent = data.safety_notice;

  // Health trend bars (one per snapshot)
  const trend = document.getElementById("trend");
  trend.innerHTML = "";
  (data.snapshots || []).forEach(s => {
    const h = parseInt(s.health ?? "0", 10);
    const wrap = el("div", "bar-wrap");
    const bar = el("span", `bar ${scoreClass(h)}`);
    bar.style.height = `${Math.max(6, h)}px`;
    bar.title = `boot ${s.boot}: ${h}/100`;
    wrap.appendChild(bar);
    wrap.appendChild(el("div", "bar-label", `boot ${s.boot} · ${h}`));
    const tags = [];
    if (s.safe_mode === "1") tags.push("safe");
    if (s.panic === "1") tags.push("panic");
    if (tags.length) wrap.appendChild(el("div", "bar-tags", tags.join(" · ")));
    trend.appendChild(wrap);
  });
  if (!data.snapshots?.length)
    trend.appendChild(el("div", "hint", "no snapshots yet — finish one boot with the disk attached"));

  // Insights
  const insights = document.getElementById("insights");
  insights.innerHTML = "";
  (data.insights || []).forEach(i => insights.appendChild(el("li", null, i)));
  if (!data.insights?.length)
    insights.appendChild(el("li", "hint", "no notable patterns detected"));

  // Module versions + packages (v0.9)
  renderTable("modules", data.modules || [], [
    { key: "module", label: "Module" },
    { key: "version", label: "Version" },
    { key: "base", label: "Base" },
    { key: "updated", label: "Updated" },
  ]);

  const pol = document.getElementById("pkg-policy");
  pol.innerHTML = "";
  (data.package_policy || []).forEach(rule => {
    const text = rule.replace(/^rule=/, "").replace(/_/g, " ");
    pol.appendChild(el("li", null, text));
  });

  renderTable("packages", data.packages || [], [
    { key: "id", label: "ID" }, { key: "name", label: "Package" },
    { key: "target", label: "Target" },
    { key: "from", label: "From" }, { key: "to", label: "To" },
    { key: "risk", label: "Risk" }, { key: "signature", label: "Signature" },
    { key: "status", label: "Status", statusClass: true },
  ]);

  const ph = document.getElementById("pkg-history");
  ph.innerHTML = "";
  (data.package_history || []).forEach(h => {
    ph.appendChild(el("div", "tl-events",
      `boot ${h.boot} — ${h.event} ${h.package} [${h.result}]`));
  });
  if (!data.package_history?.length)
    ph.appendChild(el("div", "hint", "no package events recorded yet"));

  // Recommendations — display + copy only. NO apply path exists.
  const recs = document.getElementById("recommendations");
  recs.innerHTML = "";
  // Package recommendations join the action recommendations; each
  // command gets its own copy chip. Still zero apply paths.
  (data.package_recommendations || []).forEach(r => {
    const row = el("div", "rec");
    const left = el("div");
    left.appendChild(el("div", null,
      `${r.id} ${r.name} — risk: ${r.risk}, reversible: ${r.reversible}`));
    left.appendChild(el("div", "why", `why: ${r.reason}`));
    row.appendChild(left);
    const right = el("div");
    (r.commands || []).forEach(cmd => {
      const chip = el("span", "cmd", cmd);
      chip.style.display = "block";
      chip.style.marginBottom = "4px";
      chip.style.cursor = "pointer";
      chip.title = "click to copy — then type it inside KamikazeOS";
      chip.addEventListener("click", () => navigator.clipboard?.writeText(cmd));
      right.appendChild(chip);
    });
    row.appendChild(right);
    recs.appendChild(row);
  });
  (data.recommendations || []).forEach(r => {
    const row = el("div", "rec");
    const left = el("div");
    left.appendChild(el("div", null, `${r.id} ${r.name} — risk: ${r.risk}, reversible: ${r.reversible}`));
    left.appendChild(el("div", "why", `why: ${r.reason}`));
    row.appendChild(left);
    const right = el("div");
    right.appendChild(el("span", "cmd", r.command));
    const btn = el("button", "copy", "copy");
    btn.addEventListener("click", () => {
      navigator.clipboard?.writeText(r.command);
      btn.textContent = "copied — type it in KamikazeOS";
      setTimeout(() => (btn.textContent = "copy"), 2500);
    });
    right.appendChild(document.createTextNode(" "));
    right.appendChild(btn);
    row.appendChild(right);
    recs.appendChild(row);
  });
  if (!data.recommendations?.length && !data.package_recommendations?.length)
    recs.appendChild(el("div", "hint", "nothing recommended right now"));

  // Tables
  renderTable("devices", data.devices || [], [
    { key: "id", label: "ID" }, { key: "name", label: "Device" },
    { key: "type", label: "Type" }, { key: "bus", label: "Bus" },
    { key: "driver", label: "Driver" },
    { key: "status", label: "Status", statusClass: true },
  ]);
  renderTable("drivers", data.drivers || [], [
    { key: "id", label: "ID" }, { key: "name", label: "Driver" },
    { key: "supports", label: "Supports" }, { key: "version", label: "Ver" },
    { key: "verified", label: "Verified" }, { key: "risk", label: "Risk" },
    { key: "status", label: "Status", statusClass: true },
  ]);
  renderTable("actions", data.actions || [], [
    { key: "id", label: "ID" }, { key: "name", label: "Action" },
    { key: "risk", label: "Risk" }, { key: "reversible", label: "Rev" },
    { key: "status", label: "Status", statusClass: true },
  ]);
  renderTable("snapshots", data.snapshots || [], [
    { key: "boot", label: "Boot" }, { key: "health", label: "Health" },
    { key: "attached", label: "Att" }, { key: "fallback", label: "Fall" },
    { key: "missing", label: "Miss" }, { key: "isolated", label: "Isol" },
    { key: "warnings", label: "Warn" }, { key: "criticals", label: "Crit" },
    { key: "applied", label: "Actions" },
  ]);

  // Timeline
  const tl = document.getElementById("timeline");
  tl.innerHTML = "";
  (data.timeline || []).forEach(t => {
    const boot = el("div", "tl-boot");
    let head = `boot ${t.boot} — health ${t.health}/100`;
    if (t.safe_mode === "1") head += "  [SAFE MODE]";
    if (t.panic === "1") head += "  [AFTER PANIC]";
    boot.appendChild(el("div", "tl-head", head));
    (t.events || "").split(",").forEach(ev => {
      if (ev && ev !== "none")
        boot.appendChild(el("div", "tl-events", ev));
    });
    tl.appendChild(boot);
  });

  // Warnings & errors with the rule base's explanations
  const issues = document.getElementById("issues");
  issues.innerHTML = "";
  (data.events || []).forEach(e => {
    const critical = e.severity === "error" || e.severity === "panic";
    const div = el("div", `issue${critical ? " critical" : ""}`);
    const head = el("div");
    head.appendChild(el("span", "code", `${e.code} `));
    head.appendChild(document.createTextNode(e.message || ""));
    div.appendChild(head);
    const rule = (data.rules || {})[e.code];
    div.appendChild(el("div", "cause",
      rule ? `${rule.cause} → ${rule.action}` : `hint: ${e.hint || "-"}`));
    issues.appendChild(div);
  });
  if (!data.events?.length)
    issues.appendChild(el("div", "hint", "no warnings or errors — system healthy"));
}

// Data source: prefer the .js companion (works from file://); fall back
// to fetching the JSON (works when served over http).
if (window.RECOVERY_DATA) {
  render(window.RECOVERY_DATA);
} else {
  fetch("data/recovery-report.json")
    .then(r => r.json())
    .then(render)
    .catch(() => {
      document.getElementById("meta").textContent =
        "no data found — run: make dashboard-json (after booting the OS)";
    });
}
