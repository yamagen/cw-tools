(function () {
  "use strict";

  const data = globalThis.emitData;

  if (!data || !Array.isArray(data.nodes)) {
    throw new Error("emit-pos.js requires emit-data.js to be loaded first");
  }

  const panel = document.getElementById("emit-pos-panel");
  const toggleButton = document.getElementById("emit-pos-toggle");
  const titleElement = document.getElementById("emit-pos-title");
  const valueOutput = document.getElementById("emit-pos-value");
  const body = document.getElementById("emit-pos-body");

  if (!panel || !toggleButton || !titleElement || !valueOutput || !body) {
    throw new Error("emit-pos.js could not find the required POS controls");
  }

  let posField = 3;
  let panelTitle = "POS composition";
  let posLabelsPath = "../tests/tools/pos.tsv";
  const posLabels = new Map();

  function parsePosLabels(text) {
    posLabels.clear();
    for (const line of String(text).split(/\r?\n/)) {
      if (!line.trim() || line.trimStart().startsWith("#")) continue;
      const tab = line.indexOf("\t");
      if (tab < 0) continue;
      const code = line.slice(0, tab).trim();
      const label = line.slice(tab + 1).trim();
      if (code) posLabels.set(code, label || code);
    }
  }

  function loadPosLabels() {
    return fetch(posLabelsPath, { cache: "no-store" })
      .then((response) => {
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        return response.text();
      })
      .then((text) => {
        parsePosLabels(text);
        update();
        return posLabels;
      })
      .catch(() => {
        update();
        return posLabels;
      });
  }

  const configPromise = fetch("emit-d3.config.json")
    .then((response) => {
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      return response.json();
    })
    .then((config) => {
      const observation = config && typeof config.observation === "object"
        ? config.observation
        : {};
      const field = Number(observation.pos_field);
      if (Number.isInteger(field) && field >= 1) posField = field;
      if (typeof observation.pos_title === "string" && observation.pos_title.trim()) {
        panelTitle = observation.pos_title.trim();
      }
      if (typeof observation.pos_labels_path === "string" && observation.pos_labels_path.trim()) {
        posLabelsPath = observation.pos_labels_path.trim();
      }
      titleElement.textContent = panelTitle;
      update();
      return loadPosLabels();
    })
    .catch(() => {
      titleElement.textContent = panelTitle;
      update();
      return loadPosLabels();
    });

  function tokenFields(node) {
    if (Array.isArray(node.fields) && node.fields.length > 0) return node.fields;
    return String(node.id ?? "").split("/");
  }

  function posOf(node) {
    const fields = tokenFields(node);
    const value = fields[posField - 1];
    if (value === undefined || value === null || String(value).trim() === "") {
      return "(unknown)";
    }
    return String(value);
  }

  function isVisible(node) {
    const element = document.getElementById(node.element_id);
    return !element || !element.classList.contains("is-hidden");
  }

  function formatPercent(count, total) {
    return total > 0 ? `${((count / total) * 100).toFixed(1)}%` : "0.0%";
  }

  function pastelFor(code) {
    if (code === "(unknown)") return { background: "hsl(0 0% 92%)", border: "hsl(0 0% 78%)" };
    let hash = 0;
    for (const ch of String(code)) hash = (hash * 31 + ch.codePointAt(0)) >>> 0;
    const hue = hash % 360;
    return {
      background: `hsl(${hue} 58% 91%)`,
      border: `hsl(${hue} 38% 76%)`,
    };
  }

  function update() {
    const counts = new Map();
    let visibleNodes = 0;

    for (const node of data.nodes) {
      if (!isVisible(node)) continue;
      visibleNodes++;
      const pos = posOf(node);
      counts.set(pos, (counts.get(pos) || 0) + 1);
    }

    const rows = [...counts.entries()].sort((left, right) => {
      const countDifference = right[1] - left[1];
      return countDifference !== 0
        ? countDifference
        : left[0].localeCompare(right[0], "ja", { numeric: true });
    });

    body.replaceChildren();
    for (const [pos, count] of rows) {
      const row = document.createElement("tr");
      const posCell = document.createElement("th");
      const countCell = document.createElement("td");
      const percentCell = document.createElement("td");
      const tag = document.createElement("span");
      const code = document.createElement("span");
      const label = document.createElement("span");
      const colors = pastelFor(pos);

      posCell.scope = "row";
      tag.className = "emit-pos-tag";
      tag.style.backgroundColor = colors.background;
      tag.style.borderColor = colors.border;
      code.className = "emit-pos-code";
      code.textContent = pos;
      label.className = "emit-pos-label";
      label.textContent = posLabels.get(pos) || "";
      tag.append(code, label);
      posCell.append(tag);

      countCell.textContent = String(count);
      percentCell.textContent = formatPercent(count, visibleNodes);
      row.append(posCell, countCell, percentCell);
      body.append(row);
    }

    valueOutput.textContent = `${visibleNodes} nodes · field ${posField}`;
    return { visibleNodes, posField, counts, posLabels };
  }

  toggleButton.addEventListener("click", () => {
    panel.hidden = !panel.hidden;
    toggleButton.setAttribute("aria-pressed", String(!panel.hidden));
    if (!panel.hidden) update();
  });

  globalThis.addEventListener("emit-z-change", update);
  globalThis.addEventListener("emit-weight-change", update);

  titleElement.textContent = panelTitle;
  update();
  void configPromise;

  globalThis.emitPos = Object.freeze({
    panel,
    update,
    posLabels,
    get posField() { return posField; },
    get posLabelsPath() { return posLabelsPath; },
  });
})();
