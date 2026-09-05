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
      titleElement.textContent = panelTitle;
      update();
      return config;
    })
    .catch(() => {
      titleElement.textContent = panelTitle;
      update();
      return null;
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
        : left[0].localeCompare(right[0], "ja");
    });

    body.replaceChildren();
    for (const [pos, count] of rows) {
      const row = document.createElement("tr");
      const posCell = document.createElement("th");
      const countCell = document.createElement("td");
      const percentCell = document.createElement("td");

      posCell.scope = "row";
      posCell.textContent = pos;
      countCell.textContent = String(count);
      percentCell.textContent = formatPercent(count, visibleNodes);
      row.append(posCell, countCell, percentCell);
      body.append(row);
    }

    valueOutput.textContent = `${visibleNodes} nodes · field ${posField}`;
    return { visibleNodes, posField, counts };
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
    get posField() { return posField; },
  });
})();
