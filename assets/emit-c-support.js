(function () {
  "use strict";

  const data = globalThis.emitData;
  const sliderApi = globalThis.emitSlider;
  const weightApi = globalThis.emitWeight;

  if (!data || !Array.isArray(data.nodes) || !Array.isArray(data.links)) {
    throw new Error("emit-c-support.js requires emit-data.js to be loaded first");
  }
  if (!sliderApi || !weightApi || !weightApi.available) {
    throw new Error("emit-c-support.js requires emit-slider.js and emit-weight.js first");
  }

  const panel = document.getElementById("emit-c-support-panel");
  const toggleButton = document.getElementById("emit-c-support-toggle");
  const valueOutput = document.getElementById("emit-c-support-value");
  const body = document.getElementById("emit-c-support-body");

  if (!panel || !toggleButton || !valueOutput || !body) {
    throw new Error("emit-c-support.js could not find the required controls");
  }

  function endpointId(endpoint) {
    return typeof endpoint === "object" && endpoint !== null ? endpoint.id : endpoint;
  }

  function sampleStats(values) {
    let count = 0;
    let mean = 0;
    let m2 = 0;
    for (const value of values) {
      count++;
      const delta = value - mean;
      mean += delta / count;
      const delta2 = value - mean;
      m2 += delta * delta2;
    }
    const sd = count > 1 ? Math.sqrt(m2 / (count - 1)) : 0;
    return { mean, sd: Number.isFinite(sd) ? sd : 0 };
  }

  function baselineAt(beta) {
    const weights = data.links.map((link) => {
      const g = Number(link.g);
      const p = Number(link.p);
      return g * Math.pow(p, beta);
    });
    const stats = sampleStats(weights);
    const z = weights.map((w) => stats.sd > 0 ? (w - stats.mean) / stats.sd : 0);
    return { weights, z, ...stats };
  }

  function nodeLabel(node) {
    if (typeof node.label === "string" && node.label) return node.label;
    const id = String(node.id ?? "");
    return id.split("/")[0] || id;
  }

  function nodePos(node) {
    const posApi = globalThis.emitPos;
    const field = Number(posApi?.posField ?? 3);
    const fields = Array.isArray(node.fields) && node.fields.length > 0
      ? node.fields
      : String(node.id ?? "").split("/");
    return String(fields[field - 1] ?? "(unknown)");
  }

  function posLabel(code) {
    return globalThis.emitPos?.posLabels?.get(code) || "";
  }

  function statusRank(status) {
    if (status === "recruited") return 0;
    if (status === "retained") return 1;
    if (status === "lost") return 2;
    return 3;
  }

  function formatNumber(value) {
    if (!Number.isFinite(value)) return "NA";
    if (Math.abs(value) >= 1000) return value.toExponential(2);
    if (Math.abs(value) >= 10) return value.toFixed(2);
    if (Math.abs(value) >= 1) return value.toFixed(3);
    return value.toPrecision(3);
  }

  function compute() {
    const alpha = Number(weightApi.alpha);
    const beta = Number(weightApi.beta);
    const threshold = Number(sliderApi.slider.value);
    const baseline = baselineAt(beta);

    const currentConnected = new Set();
    const baselineConnected = new Set();
    const support = new Map();
    const incident = new Map();

    function ensureNode(id) {
      if (!support.has(id)) support.set(id, 0);
      if (!incident.has(id)) incident.set(id, []);
    }

    data.links.forEach((link, index) => {
      const source = endpointId(link.source);
      const target = endpointId(link.target);
      ensureNode(source);
      ensureNode(target);

      const currentVisible = Number(link.z) >= threshold;
      const baselineVisible = baseline.z[index] >= threshold;
      if (currentVisible) {
        currentConnected.add(source);
        currentConnected.add(target);
      }
      if (baselineVisible) {
        baselineConnected.add(source);
        baselineConnected.add(target);
      }

      const g = Number(link.g);
      const c = Number(link.c);
      const p = Number(link.p);
      const baseW = baseline.weights[index];
      const currentW = g * Math.pow(c, alpha) * Math.pow(p, beta);
      const deltaW = currentW - baseW;

      if (currentVisible && deltaW > 0) {
        support.set(source, support.get(source) + deltaW);
        support.set(target, support.get(target) + deltaW);
        incident.get(source).push({ neighbor: target, deltaW, ctf: link.ctf, z: link.z });
        incident.get(target).push({ neighbor: source, deltaW, ctf: link.ctf, z: link.z });
      }
    });

    const rows = [];
    for (const node of data.nodes) {
      const current = currentConnected.has(node.id);
      const base = baselineConnected.has(node.id);
      if (!current && !base) continue;
      const status = current && !base ? "recruited" : current && base ? "retained" : "lost";
      const edges = incident.get(node.id) || [];
      edges.sort((a, b) => b.deltaW - a.deltaW);
      rows.push({
        node,
        status,
        support: support.get(node.id) || 0,
        edgeCount: edges.length,
        edges,
      });
    }

    rows.sort((a, b) => {
      const sr = statusRank(a.status) - statusRank(b.status);
      if (sr !== 0) return sr;
      const ds = b.support - a.support;
      if (Math.abs(ds) > 1e-12) return ds;
      return nodeLabel(a.node).localeCompare(nodeLabel(b.node), "ja");
    });

    return { alpha, beta, threshold, baseline, rows, currentConnected, baselineConnected };
  }

  function appendDetails(parentRow, item) {
    if (item.edges.length === 0) return;
    const detailRow = document.createElement("tr");
    detailRow.className = "emit-c-support-details";
    detailRow.hidden = true;
    const cell = document.createElement("td");
    cell.colSpan = 5;

    const list = document.createElement("ol");
    for (const edge of item.edges.slice(0, 8)) {
      const li = document.createElement("li");
      li.textContent = `${String(edge.neighbor).split("/")[0]} · ΔW ${formatNumber(edge.deltaW)} · ctf ${edge.ctf} · Z ${Number(edge.z).toFixed(2)}`;
      list.append(li);
    }
    cell.append(list);
    detailRow.append(cell);
    parentRow.after(detailRow);
    parentRow.classList.add("emit-c-support-expandable");
    parentRow.tabIndex = 0;
    parentRow.setAttribute("aria-expanded", "false");

    function toggle() {
      detailRow.hidden = !detailRow.hidden;
      parentRow.setAttribute("aria-expanded", String(!detailRow.hidden));
    }
    parentRow.addEventListener("click", toggle);
    parentRow.addEventListener("keydown", (event) => {
      if (event.key === "Enter" || event.key === " ") {
        event.preventDefault();
        toggle();
      }
    });
  }

  function update() {
    const result = compute();
    body.replaceChildren();

    for (const item of result.rows) {
      const row = document.createElement("tr");
      row.className = `emit-c-support-${item.status}`;

      const nodeCell = document.createElement("th");
      const posCell = document.createElement("td");
      const supportCell = document.createElement("td");
      const edgeCell = document.createElement("td");
      const statusCell = document.createElement("td");

      nodeCell.scope = "row";
      nodeCell.textContent = nodeLabel(item.node);
      const code = nodePos(item.node);
      posCell.textContent = posLabel(code) ? `${code} ${posLabel(code)}` : code;
      supportCell.textContent = formatNumber(item.support);
      edgeCell.textContent = String(item.edgeCount);
      statusCell.textContent = item.status;

      row.append(nodeCell, posCell, supportCell, edgeCell, statusCell);
      body.append(row);
      appendDetails(row, item);
    }

    const recruited = result.rows.filter((row) => row.status === "recruited").length;
    const retained = result.rows.filter((row) => row.status === "retained").length;
    const lost = result.rows.filter((row) => row.status === "lost").length;
    valueOutput.textContent = `Z ${result.threshold.toFixed(2)} · α ${result.alpha.toFixed(2)} · β ${result.beta.toFixed(2)} · +${recruited} =${retained} −${lost}`;
    return result;
  }

  toggleButton.addEventListener("click", () => {
    panel.hidden = !panel.hidden;
    toggleButton.setAttribute("aria-pressed", String(!panel.hidden));
    if (!panel.hidden) update();
  });

  globalThis.addEventListener("emit-z-change", () => {
    if (!panel.hidden) update();
  });
  globalThis.addEventListener("emit-weight-change", () => {
    if (!panel.hidden) update();
  });

  globalThis.emitCSupport = Object.freeze({ panel, update, compute });
})();
