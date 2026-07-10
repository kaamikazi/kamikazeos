/*
 * KamikazeOS Recovery Center Pro - read-only renderer.
 *
 * Safety contract: this file consumes exported JSON and updates the DOM.
 * The only interaction copies text to the clipboard. It never writes
 * recovery.img, calls OS APIs, applies repairs, downloads packages, or changes
 * KamikazeOS state.
 */
"use strict";

const SAFE_COMMAND = /^(approve|rollback) ACT-[0-9]{3}$|^(approve-pkg|apply-pkg|verify-pkg|rollback-pkg) PKG-[0-9]{3}$/;

function $(id) {
  return document.getElementById(id);
}

function el(tag, cls, text) {
  const n = document.createElement(tag);
  if (cls) n.className = cls;
  if (text !== undefined) n.textContent = text;
  return n;
}

function scoreClass(score) {
  return score >= 70 ? "good" : score >= 40 ? "warn" : "bad";
}

function classify(value) {
  const v = String(value ?? "").toLowerCase();
  if (["yes", "1", "ok", "allowed", "verified", "attached", "loaded", "trusted", "enabled", "irq", "none", "running", "completed"].includes(v)) return "good";
  if (["fallback", "suggested", "available", "medium", "pending", "applied", "sleeping", "kernel_managed_builtin_apps"].includes(v)) return "warn";
  if (["no", "0", "blocked", "failed", "invalid", "unsigned", "isolated", "missing_driver", "high", "error", "panic", "crashed"].includes(v)) return "bad";
  if (["low", "disk_imported", "builtin_seed", "ready", "cooperative"].includes(v)) return "info";
  return "neutral";
}

function badge(text, extra) {
  return el("span", `badge ${extra || classify(text)}`, text || "unknown");
}

function pill(text) {
  return el("span", `status-pill ${classify(text)}`, text || "-");
}

function metric(label, value) {
  const row = el("div", "metric");
  row.appendChild(el("span", null, label));
  row.appendChild(el("span", null, value ?? "-"));
  return row;
}

function empty(target, text) {
  target.innerHTML = "";
  target.appendChild(el("div", "empty-note", text));
}

function setBadge(id, text, cls) {
  const node = $(id);
  node.textContent = text;
  node.className = `badge ${cls || classify(text)}`;
}

function renderTable(id, rows, columns) {
  const table = $(id);
  table.innerHTML = "";
  const head = el("tr");
  columns.forEach(c => head.appendChild(el("th", null, c.label)));
  table.appendChild(head);

  if (!rows || rows.length === 0) {
    const tr = el("tr");
    const td = el("td", "empty-row", "No data in exported report");
    td.colSpan = columns.length;
    tr.appendChild(td);
    table.appendChild(tr);
    return;
  }

  rows.forEach(row => {
    const tr = el("tr");
    columns.forEach(c => {
      const td = el("td", c.wrap ? "wrap" : null);
      const value = row[c.key] ?? "";
      if (c.badge) td.appendChild(pill(value));
      else td.textContent = value || "-";
      tr.appendChild(td);
    });
    table.appendChild(tr);
  });
}

function renderHealth(data) {
  const score = parseInt(data.health?.score ?? "0", 10);
  const scoreEl = $("health-score");
  scoreEl.textContent = Number.isFinite(score) ? String(score) : "--";
  scoreEl.className = `score ${scoreClass(score)}`;
  setBadge("health-status", `${scoreClass(score)} health`, scoreClass(score));

  const box = $("health-breakdown");
  box.innerHTML = "";
  box.appendChild(metric("Missing driver penalty", data.health?.missing_penalty));
  box.appendChild(metric("Isolated device penalty", data.health?.isolated_penalty));
  box.appendChild(metric("Warning penalty", data.health?.warning_penalty));
  box.appendChild(metric("Critical penalty", data.health?.critical_penalty));
  box.appendChild(metric("Previous score", data.health?.prev_score));
  box.appendChild(metric("Delta", data.health?.health_delta));
}

function renderV20Overview(data) {
  const status = data.system_status || {};
  const diag = data.diagnostics || {};
  const box = $("v20-overview");
  box.innerHTML = "";
  box.appendChild(metric("Kernel", status.kernel_version || data.version));
  box.appendChild(metric("Interrupts", `IDT ${status.idt_loaded || "?"}, PIC ${status.pic_remapped || "?"}, keyboard ${status.keyboard_mode || "?"}`));
  box.appendChild(metric("Memory", `map ${status.memory_map || data.kernel_memory?.map_available || "?"}, free pages ${status.free_pages || data.pages?.free_pages || "?"}`));
  box.appendChild(metric("Heap", `${status.heap_available || data.kernel_heap?.available || "?"}, remaining ${status.heap_remaining || data.kernel_heap?.remaining_bytes || "?"} bytes`));
  box.appendChild(metric("Paging", `${status.paging_mode || data.paging?.mode || "?"}`));
  box.appendChild(metric("Storage", `${status.recovery_backend || data.backend || "?"}, TinyFS ${status.tinyfs_status || "?"}`));
  box.appendChild(metric("Apps", `${status.app_registry || data.apps_status?.total_apps || "?"} fixed built-ins`));
  box.appendChild(metric("Packages", status.package_trust_mode || data.trust?.signature_verification || "?"));
  box.appendChild(metric("Health", `${status.health_score || data.health?.score || "?"}/100`));
  box.appendChild(metric("Last critical", status.last_critical || "none"));
  box.appendChild(metric("Demo next command", diag.recommended_command || "none"));
  box.appendChild(metric("Auto repair", diag.auto_repair || "no"));
  const ready = status.idt_loaded === "yes" &&
    status.pic_remapped === "yes" &&
    parseInt(status.free_pages || data.pages?.free_pages || "0", 10) > 0 &&
    (status.heap_available || data.kernel_heap?.available) === "yes" &&
    (status.paging_prepared || data.paging?.prepared) === "yes" &&
    (data.paging?.identity_map_valid || "yes") === "yes" &&
    status.tinyfs_status === "formatted" &&
    (status.recovery_backend || "").includes("disk") &&
    parseInt(status.app_registry || "0", 10) > 0;
  setBadge("v20-status", ready ? "integrated demo ready" : "review report",
    ready ? "good" : "warn");
}

function renderPaging(data) {
  const paging = data.paging || {};
  const box = $("paging-panel");
  box.innerHTML = "";
  box.appendChild(metric("Prepared", paging.prepared || "?"));
  box.appendChild(metric("Enabled", paging.enabled || "?"));
  box.appendChild(metric("Mode", paging.mode || "prepared_not_enabled"));
  box.appendChild(metric("Identity valid", paging.identity_map_valid || "?"));
  box.appendChild(metric("Directory", paging.page_directory || "?"));
  box.appendChild(metric("Mapped range", `${paging.mapped_start || "?"} - ${paging.mapped_end || "?"}`));
  box.appendChild(metric("Fault handler", paging.page_fault_handler || "?"));
  box.appendChild(metric("Last fault", paging.last_fault_address || "none"));
  const valid = paging.prepared === "yes" && paging.identity_map_valid === "yes";
  setBadge("paging-status", paging.enabled === "yes" ? "enabled" :
    valid ? "prepared" : "invalid",
    valid ? (paging.enabled === "yes" ? "good" : "info") : "bad");

  renderTable("page-map", data.page_map || [], [
    { key: "type", label: "Type" },
    { key: "virtual_start", label: "Virtual Start" },
    { key: "virtual_end", label: "Virtual End" },
    { key: "physical_start", label: "Physical Start" },
    { key: "physical_end", label: "Physical End" },
    { key: "flags", label: "Flags", wrap: true },
    { key: "validated", label: "Validated", badge: true },
  ]);
}

function renderHeap(data) {
  const heap = data.kernel_heap || {};
  const box = $("heap-panel");
  box.innerHTML = "";
  box.appendChild(metric("Available", heap.available || "?"));
  box.appendChild(metric("Start", heap.heap_start || "?"));
  box.appendChild(metric("End", heap.heap_end || "?"));
  box.appendChild(metric("Total", heap.total_bytes ? `${heap.total_bytes} bytes` : "?"));
  box.appendChild(metric("Used", heap.used_bytes ? `${heap.used_bytes} bytes` : "0 bytes"));
  box.appendChild(metric("Remaining", heap.remaining_bytes ? `${heap.remaining_bytes} bytes` : "?"));
  box.appendChild(metric("Allocations", heap.allocation_count || "0"));
  box.appendChild(metric("Failures", heap.failed_count || "0"));
  box.appendChild(metric("free()", heap.free_supported || "no"));
  const low = parseInt(heap.total_bytes || "0", 10) > 0 &&
    parseInt(heap.remaining_bytes || "0", 10) * 10 < parseInt(heap.total_bytes || "0", 10);
  const failed = parseInt(heap.failed_count || "0", 10) > 0;
  setBadge("heap-status", heap.available === "yes" ? (low ? "low remaining" : "bump heap") : "unavailable",
    heap.available !== "yes" ? "bad" : failed || low ? "warn" : "good");
}

function renderMemory(data) {
  const mem = data.kernel_memory || {};
  const pages = data.pages || {};
  const box = $("memory-panel");
  box.innerHTML = "";
  box.appendChild(metric("Map available", mem.map_available || "?"));
  box.appendChild(metric("Fallback used", mem.fallback_used || "?"));
  box.appendChild(metric("Regions", mem.region_count || "0"));
  box.appendChild(metric("Usable", mem.usable_kib ? `${mem.usable_kib} KiB` : "?"));
  box.appendChild(metric("Reserved", mem.reserved_kib ? `${mem.reserved_kib} KiB` : "?"));
  box.appendChild(metric("Free pages", `${pages.free_pages || "?"} / ${pages.total_pages || "?"}`));
  box.appendChild(metric("Kernel pages", pages.reserved_kernel_pages || "?"));
  box.appendChild(metric("Mode", mem.allocation_mode || "physical_page_tracker_only"));
  const healthy = (mem.map_available === "yes" || mem.fallback_used === "yes") &&
    parseInt(pages.free_pages || "0", 10) > 0;
  setBadge("memory-status", mem.map_available === "yes" ? "multiboot map" :
    mem.fallback_used === "yes" ? "fallback map" : "missing",
    healthy ? (mem.map_available === "yes" ? "good" : "warn") : "bad");

  renderTable("memory-map", data.memory_map || [], [
    { key: "index", label: "Index" },
    { key: "base", label: "Base" },
    { key: "length", label: "Length" },
    { key: "kib", label: "KiB" },
    { key: "type", label: "Type", badge: true },
  ]);
}

function renderBackend(data) {
  const store = data.store || {};
  setBadge("backend-status", data.backend || "unknown", store.disk_available === "1" ? "good" : "warn");
  const box = $("backend-card");
  box.innerHTML = "";
  box.appendChild(metric("Backend", data.backend));
  box.appendChild(metric("Disk available", store.disk_available === "1" ? "yes" : "no"));
  box.appendChild(metric("State valid", store.state_valid === "1" ? "yes" : "no"));
  box.appendChild(metric("Journal entries", store.journal_count));
  box.appendChild(metric("Durable boot count", store.durable_boot_count));
  box.appendChild(metric("CMOS fallback used", store.cmos_fallback_used === "1" ? "yes" : "no"));
}

function renderBootWarnings(data) {
  const memory = data.memory || {};
  const alerts = $("boot-warnings");
  alerts.innerHTML = "";
  const active = memory.safe_mode_active === "1";
  const scheduled = memory.safe_mode_next_boot === "1";
  const panic = memory.last_boot_panic === "1";
  const rollback = memory.rollback_available === "1";
  setBadge("boot-status", panic ? "panic evidence" : active ? "safe mode" : "normal boot", panic ? "bad" : active ? "warn" : "good");

  const items = [];
  items.push({ cls: active ? "warn" : "good", text: active ? "Safe mode is active this boot." : "Safe mode is not active." });
  if (scheduled) items.push({ cls: "warn", text: "Safe mode is scheduled for the next boot." });
  if (panic) items.push({ cls: "bad", text: "Previous boot panic evidence is present." });
  if (rollback) items.push({ cls: "warn", text: "Rollback metadata is available." });
  if (items.length === 1 && !scheduled && !panic && !rollback) items.push({ cls: "good", text: "No panic, rollback, or safe-mode warnings in this report." });
  items.forEach(i => alerts.appendChild(el("div", `alert ${i.cls}`, i.text)));
}

function renderTrust(data) {
  const trust = data.trust || {};
  const box = $("trust-panel");
  box.innerHTML = "";
  box.appendChild(metric("Anchor", trust.anchor_id || "?"));
  box.appendChild(metric("Fingerprint", trust.fingerprint || "?"));
  box.appendChild(metric("Signature mode", trust.signature_verification || "simulated_kernel_check_host_ed25519"));
  box.appendChild(metric("Note", trust.note || "Host Ed25519 real; kernel Ed25519 simulated."));
  setBadge("trust-status", "read-only trust view", "info");
}

function renderKernelFoundation(data) {
  const interrupts = data.kernel_interrupts || {};
  const timing = data.kernel_timing || {};
  const exceptions = data.kernel_exceptions || {};

  const ibox = $("interrupt-panel");
  ibox.innerHTML = "";
  ibox.appendChild(metric("IDT loaded", interrupts.idt_loaded));
  ibox.appendChild(metric("PIC remapped", interrupts.pic_remapped));
  ibox.appendChild(metric("Timer IRQ", interrupts.timer_enabled));
  ibox.appendChild(metric("Interrupts enabled", interrupts.interrupts_enabled));
  ibox.appendChild(metric("Handled", interrupts.interrupts_handled));
  ibox.appendChild(metric("Last vector", interrupts.last_vector));
  ibox.appendChild(metric("Keyboard mode", interrupts.keyboard_mode));
  const foundationOk = interrupts.idt_loaded === "yes" &&
    interrupts.pic_remapped === "yes" && interrupts.timer_enabled === "yes";
  setBadge("interrupt-status", foundationOk ? interrupts.keyboard_mode || "ready" : "degraded",
    foundationOk ? (interrupts.keyboard_mode === "polling_fallback" ? "warn" : "good") : "bad");

  const tbox = $("timing-panel");
  tbox.innerHTML = "";
  tbox.appendChild(metric("Tick count", timing.tick_count));
  tbox.appendChild(metric("Frequency", timing.frequency_hz ? `${timing.frequency_hz} Hz` : "?"));
  tbox.appendChild(metric("IRQ state", timing.timer_irq));
  setBadge("timer-status", timing.timer_irq || "unknown", classify(timing.timer_irq));

  const ebox = $("exception-panel");
  ebox.innerHTML = "";
  ebox.appendChild(metric("Last exception", exceptions.last_exception || "none"));
  ebox.appendChild(metric("Vector", exceptions.last_vector || "none"));
  ebox.appendChild(metric("Error code", exceptions.last_error_code || "0"));
  ebox.appendChild(metric("Recovery attempted", exceptions.recovery_attempted || "no"));
  const hasException = exceptions.last_exception && exceptions.last_exception !== "none";
  setBadge("exception-status", hasException ? "exception recorded" : "none recorded",
    hasException ? "bad" : "good");
}

function renderTaskFoundation(data) {
  const tasks = data.kernel_tasks || {};
  const box = $("task-panel");
  box.innerHTML = "";
  box.appendChild(metric("Scheduler", tasks.scheduler_mode));
  box.appendChild(metric("Preemptive switching", tasks.preemptive_switching));
  box.appendChild(metric("Current task", tasks.running_task));
  box.appendChild(metric("Total", tasks.total_tasks));
  box.appendChild(metric("Ready", tasks.ready_count));
  box.appendChild(metric("Sleeping", tasks.sleeping_count));
  box.appendChild(metric("Blocked", tasks.blocked_count));
  box.appendChild(metric("Crashed", tasks.crashed_count));
  const unhealthy = parseInt(tasks.blocked_count || "0", 10) > 0 ||
    parseInt(tasks.crashed_count || "0", 10) > 0;
  setBadge("task-status", unhealthy ? "manual inspection" : "cooperative",
    unhealthy ? "bad" : "info");

  renderTable("tasks", data.tasks || [], [
    { key: "task_id", label: "ID" },
    { key: "name", label: "Name" },
    { key: "state", label: "State", badge: true },
    { key: "priority", label: "Priority" },
    { key: "ticks_run", label: "Ticks Run" },
    { key: "created_at_tick", label: "Created" },
    { key: "last_run_tick", label: "Last Run" },
  ]);
}

function renderTinyFS(data) {
  const fs = data.tinyfs || {};
  const box = $("tinyfs-panel");
  box.innerHTML = "";
  box.appendChild(metric("Available", fs.available));
  box.appendChild(metric("Formatted", fs.formatted));
  box.appendChild(metric("Files", `${fs.file_count || "0"} / ${fs.max_files || "?"}`));
  box.appendChild(metric("Data start LBA", fs.data_start_lba));
  box.appendChild(metric("Max file size", fs.max_file_size ? `${fs.max_file_size} bytes` : "?"));
  box.appendChild(metric("Last error", fs.last_error));
  const ready = fs.available === "yes" && fs.formatted === "yes";
  setBadge("tinyfs-status", ready ? "formatted" :
    fs.available === "yes" ? "not formatted" : "unavailable",
    ready ? "good" : "warn");

  renderTable("tinyfs-files", data.tinyfs_files || [], [
    { key: "name", label: "Name" },
    { key: "size", label: "Size" },
    { key: "start_lba", label: "Start LBA" },
    { key: "sector_count", label: "Sectors" },
    { key: "checksum", label: "Checksum" },
    { key: "flags", label: "Flags" },
  ]);
}

function renderApps(data) {
  const apps = data.apps_status || {};
  const box = $("apps-panel");
  box.innerHTML = "";
  box.appendChild(metric("Userland mode", apps.userland_mode || "kernel_managed_builtin_apps"));
  box.appendChild(metric("Total apps", apps.total_apps));
  box.appendChild(metric("Last app", apps.last_app));
  box.appendChild(metric("Last result", apps.last_result));
  box.appendChild(metric("Blocked attempts", apps.blocked_attempts || "0"));
  box.appendChild(metric("Boundary", "kernel-managed built-ins only"));
  const blocked = parseInt(apps.blocked_attempts || "0", 10) > 0;
  setBadge("apps-status", blocked ? "blocked attempts" : "fixed registry",
    blocked ? "warn" : "info");

  renderTable("apps", data.apps || [], [
    { key: "app_id", label: "ID" },
    { key: "name", label: "Name" },
    { key: "description", label: "Description", wrap: true },
    { key: "version", label: "Version" },
    { key: "type", label: "Type", badge: true },
    { key: "status", label: "Status", badge: true },
    { key: "run_count", label: "Runs" },
    { key: "last_result", label: "Last Result", badge: true },
  ]);

  renderTable("app-log", data.app_log || [], [
    { key: "seq", label: "Seq" },
    { key: "name", label: "Name" },
    { key: "result", label: "Result", badge: true },
    { key: "status", label: "Status", badge: true },
    { key: "message", label: "Message", wrap: true },
  ]);
}

function renderTrend(snapshots) {
  const trend = $("trend");
  trend.innerHTML = "";
  if (!snapshots || snapshots.length === 0) {
    trend.appendChild(el("div", "empty-note", "No snapshots yet. Boot KamikazeOS with the recovery disk attached."));
    return;
  }
  snapshots.forEach(s => {
    const score = parseInt(s.health || "0", 10);
    const point = el("div", "trend-point");
    const bar = el("div", `bar ${scoreClass(score)}`);
    bar.style.height = `${Math.max(8, Math.min(100, score))}%`;
    point.appendChild(bar);
    const labels = [`boot ${s.boot}`, `${score}/100`];
    if (s.safe_mode === "1") labels.push("safe");
    if (s.panic === "1") labels.push("panic");
    point.appendChild(el("div", "bar-label", labels.join(" | ")));
    trend.appendChild(point);
  });
}

function safeCopyCommand(cmd, btn) {
  if (!SAFE_COMMAND.test(cmd)) {
    btn.textContent = "blocked";
    return;
  }
  navigator.clipboard?.writeText(cmd);
  btn.textContent = "copied";
  setTimeout(() => { btn.textContent = cmd; }, 1800);
}

function renderRecommendations(data) {
  const recs = $("recommendations");
  recs.innerHTML = "";
  const rows = [];
  (data.package_recommendations || []).forEach(r => rows.push({
    id: r.id,
    name: r.name,
    reason: r.reason,
    risk: r.risk,
    reversible: r.reversible,
    commands: r.commands || [],
  }));
  (data.recommendations || []).forEach(r => rows.push({
    id: r.id,
    name: r.name,
    reason: r.reason,
    risk: r.risk,
    reversible: r.reversible,
    commands: [r.command],
  }));

  if (rows.length === 0) {
    empty(recs, "No recommended commands in this exported report.");
    return;
  }

  rows.forEach(r => {
    const row = el("div", "rec");
    const left = el("div");
    left.appendChild(el("div", "rec-title", `${r.id} ${r.name || ""}`.trim()));
    left.appendChild(el("div", "why", `risk: ${r.risk || "?"} | reversible: ${r.reversible || "?"}`));
    left.appendChild(el("div", "why", r.reason || "reported by analyzer"));
    row.appendChild(left);

    const stack = el("div", "cmd-stack");
    r.commands.filter(Boolean).forEach(cmd => {
      const btn = el("button", "cmd", cmd);
      btn.title = "Copy command text. Nothing is executed.";
      btn.addEventListener("click", () => safeCopyCommand(cmd, btn));
      stack.appendChild(btn);
    });
    row.appendChild(stack);
    recs.appendChild(row);
  });
}

function renderTimelines(data) {
  const journal = $("journal");
  journal.innerHTML = "";
  if (!data.journal?.length) {
    empty(journal, "No durable journal entries in this report.");
  } else {
    data.journal.forEach(j => {
      const item = el("div", "timeline-item");
      item.appendChild(el("div", "timeline-title", `boot ${j.boot || "?"} | ${j.event || "event"}`));
      item.appendChild(el("div", "timeline-detail", `${j.action || j.package || ""} ${j.result ? "[" + j.result + "]" : ""}`.trim()));
      journal.appendChild(item);
    });
  }

  const snaps = $("snapshot-timeline");
  snaps.innerHTML = "";
  if (!data.snapshots?.length) {
    empty(snaps, "No boot snapshots in this report.");
  } else {
    data.snapshots.forEach(s => {
      const item = el("div", "timeline-item");
      item.appendChild(el("div", "timeline-title", `boot ${s.boot} | health ${s.health}/100`));
      item.appendChild(el("div", "timeline-detail",
        `attached ${s.attached}, fallback ${s.fallback}, missing ${s.missing}, isolated ${s.isolated}, warnings ${s.warnings}, criticals ${s.criticals}`));
      snaps.appendChild(item);
    });
  }
}

function renderIssues(data) {
  const issues = $("issues");
  issues.innerHTML = "";
  if (!data.events?.length) {
    empty(issues, "No warnings or errors in this exported report.");
    return;
  }
  data.events.forEach(e => {
    const critical = e.severity === "error" || e.severity === "panic";
    const div = el("div", `issue${critical ? " critical" : ""}`);
    const head = el("div");
    head.appendChild(el("span", "code", `${e.code || "EVENT"} `));
    head.appendChild(document.createTextNode(e.message || ""));
    div.appendChild(head);
    const rule = (data.rules || {})[e.code];
    div.appendChild(el("div", "cause", rule ? `${rule.cause} Action: ${rule.action}` : `hint: ${e.hint || "-"}`));
    issues.appendChild(div);
  });
}

function render(data) {
  if (!data || typeof data !== "object") {
    showMissingData();
    return;
  }

  $("dashboard").classList.remove("hidden");
  $("empty-state").classList.add("hidden");

  $("report-version").textContent = `report v${data.version || "?"}`;
  $("meta").textContent =
    `backend ${data.backend || "?"} | boot ${data.memory?.boot_count || "?"} | journal ${data.store?.journal_count || "0"} entries`;
  setBadge("system-badge", "read-only JSON view", "info");
  if (data.safety_notice) $("safety-banner").textContent = data.safety_notice;

  renderV20Overview(data);
  renderHealth(data);
  renderBackend(data);
  renderBootWarnings(data);
  renderTrust(data);
  renderMemory(data);
  renderHeap(data);
  renderPaging(data);
  renderKernelFoundation(data);
  renderTaskFoundation(data);
  renderTinyFS(data);
  renderApps(data);
  renderTrend(data.snapshots || []);
  renderRecommendations(data);
  renderTimelines(data);
  renderIssues(data);

  const attached = (data.devices || []).filter(d => d.status === "attached").length;
  const total = (data.devices || []).length;
  $("device-summary").textContent = `${attached}/${total} attached`;

  renderTable("devices", data.devices || [], [
    { key: "id", label: "ID" },
    { key: "name", label: "Device", wrap: true },
    { key: "type", label: "Type" },
    { key: "bus", label: "Bus" },
    { key: "driver", label: "Driver", wrap: true },
    { key: "status", label: "Status", badge: true },
  ]);
  renderTable("drivers", data.drivers || [], [
    { key: "id", label: "ID" },
    { key: "name", label: "Driver", wrap: true },
    { key: "supports", label: "Supports" },
    { key: "version", label: "Version" },
    { key: "verified", label: "Verified", badge: true },
    { key: "risk", label: "Risk", badge: true },
    { key: "status", label: "Status", badge: true },
  ]);
  renderTable("actions", data.actions || [], [
    { key: "id", label: "ID" },
    { key: "name", label: "Action", wrap: true },
    { key: "risk", label: "Risk", badge: true },
    { key: "reversible", label: "Reversible", badge: true },
    { key: "status", label: "Status", badge: true },
  ]);
  renderTable("manifests", data.manifests || [], [
    { key: "id", label: "ID" },
    { key: "payload", label: "Payload", wrap: true },
    { key: "source", label: "Source", badge: true },
    { key: "checksum_valid", label: "Checksum", badge: true },
    { key: "trust_valid", label: "Trust", badge: true },
    { key: "signature_valid", label: "Signature", badge: true },
    { key: "policy_result", label: "Policy", badge: true },
    { key: "block_reason", label: "Block Reason", wrap: true },
  ]);
  renderTable("package-states", data.package_states || [], [
    { key: "id", label: "ID" },
    { key: "approved", label: "Approved", badge: true },
    { key: "applied", label: "Applied", badge: true },
    { key: "verified", label: "Verified", badge: true },
    { key: "failed", label: "Failed", badge: true },
    { key: "rolled_back", label: "Rolled Back", badge: true },
    { key: "from", label: "From" },
    { key: "to", label: "To" },
  ]);
  setBadge("pending-verification",
    data.pending_verification ? "verification pending" : "no pending verification",
    data.pending_verification ? "warn" : "good");
  renderTable("module-slots", data.module_slots || [], [
    { key: "module", label: "Module", wrap: true },
    { key: "active", label: "Active", badge: true },
    { key: "slot_a", label: "Slot A" },
    { key: "slot_a_verified", label: "A OK", badge: true },
    { key: "slot_b", label: "Slot B" },
    { key: "slot_b_verified", label: "B OK", badge: true },
    { key: "rollback", label: "Rollback" },
    { key: "pending_verify", label: "Pending", badge: true },
  ]);
  renderTable("packages", data.packages || [], [
    { key: "id", label: "ID" },
    { key: "name", label: "Package", wrap: true },
    { key: "target", label: "Target", wrap: true },
    { key: "from", label: "From" },
    { key: "to", label: "To" },
    { key: "risk", label: "Risk", badge: true },
    { key: "source", label: "Source", badge: true },
    { key: "signature_valid", label: "Signature", badge: true },
    { key: "policy_result", label: "Policy", badge: true },
    { key: "status", label: "Status", badge: true },
    { key: "block_reason", label: "Block Reason", wrap: true },
  ]);

  const insights = $("insights");
  insights.innerHTML = "";
  if (!data.insights?.length) {
    insights.appendChild(el("li", null, "No notable analyzer insights in this report."));
  } else {
    data.insights.forEach(i => insights.appendChild(el("li", null, i)));
  }
}

function showMissingData() {
  $("dashboard").classList.add("hidden");
  $("empty-state").classList.remove("hidden");
  $("meta").textContent = "Run make dashboard-json after booting KamikazeOS.";
  setBadge("system-badge", "no data", "warn");
}

if (window.RECOVERY_DATA) {
  render(window.RECOVERY_DATA);
} else {
  fetch("data/recovery-report.json")
    .then(r => {
      if (!r.ok) throw new Error("missing data");
      return r.json();
    })
    .then(render)
    .catch(showMissingData);
}
