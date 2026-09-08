(function () {
  "use strict";

  const graph = globalThis.emitGraph;
  const sliderApi = globalThis.emitSlider;
  const engine = globalThis.emitLayouts?.kamadaKawai;

  if (!graph || !sliderApi) {
    throw new Error("emit-kk-snapshot.js requires emit-d3.js and emit-slider.js first");
  }

  const button = document.getElementById("emit-kk");
  const resetButton = document.getElementById("emit-z-reset");
  const slider = sliderApi.slider;
  const minOutput = document.getElementById("emit-z-min");
  const svgElement = document.querySelector("#emit-graph, #graph");

  if (!button || !slider || !svgElement) {
    throw new Error("emit-kk-snapshot.js could not find the required KK controls");
  }

  let snapshotFloor = null;
  let baselinePositions = null;
  let running = false;
  let layoutOptionsPromise = null;

  function endpointId(endpoint) {
    return typeof endpoint === "object" && endpoint !== null ? endpoint.id : endpoint;
  }

  function decimalsForSlider() {
    const step = Number(slider.step);
    if (!Number.isFinite(step) || step <= 0 || step >= 1) return 2;
    return Math.min(8, Math.max(0, Math.ceil(-Math.log10(step))));
  }

  function formatThreshold(value) {
    const fixed = Number(value).toFixed(decimalsForSlider());
    return fixed.replace(/\.0+$/, "").replace(/(\.\d*?)0+$/, "$1");
  }

  function setButtonState(active) {
    button.setAttribute("aria-pressed", active ? "true" : "false");
    button.textContent = active && snapshotFloor !== null
      ? `KK · Z≥${formatThreshold(snapshotFloor)}`
      : "KK";
  }

  function restoreSliderMinimum() {
    slider.min = String(sliderApi.dataMin);
    if (minOutput) minOutput.textContent = formatThreshold(sliderApi.dataMin);
  }

  function captureBaselinePositions() {
    baselinePositions = graph.nodes.map((node) => ({
      node,
      x: node.x,
      y: node.y,
      fx: node.fx,
      fy: node.fy,
    }));
  }

  function restoreBaselinePositions() {
    if (!baselinePositions) return;
    for (const saved of baselinePositions) {
      saved.node.x = saved.x;
      saved.node.y = saved.y;
      saved.node.fx = saved.fx;
      saved.node.fy = saved.fy;
    }
    renderPositions();
  }

  function clearSnapshot(options = {}) {
    const restorePositions = options.restorePositions !== false;
    if (restorePositions) restoreBaselinePositions();
    snapshotFloor = null;
    baselinePositions = null;
    restoreSliderMinimum();
    setButtonState(false);
    globalThis.dispatchEvent(new CustomEvent("emit-kk-snapshot-clear"));
  }

  function setSnapshotFloor(threshold) {
    snapshotFloor = threshold;
    slider.min = String(threshold);
    if (minOutput) minOutput.textContent = formatThreshold(threshold);
    setButtonState(true);
  }

  function visibleSubgraph(threshold) {
    const links = graph.links.filter((link) => Number(link.z) >= threshold);
    const nodeIds = new Set();
    for (const link of links) {
      nodeIds.add(endpointId(link.source));
      nodeIds.add(endpointId(link.target));
    }
    const nodes = graph.nodes.filter((node) => nodeIds.has(node.id));
    return { nodes, links };
  }

  function renderPositions() {
    graph.edgeGroups.select(".edge-hit")
      .attr("x1", (link) => link.source.x)
      .attr("y1", (link) => link.source.y)
      .attr("x2", (link) => link.target.x)
      .attr("y2", (link) => link.target.y);

    graph.edgeGroups.select(".edge-line")
      .attr("x1", (link) => link.source.x)
      .attr("y1", (link) => link.source.y)
      .attr("x2", (link) => link.target.x)
      .attr("y2", (link) => link.target.y);

    graph.edgeGroups.select(".edge-label")
      .attr("x", (link) => (link.source.x + link.target.x) / 2)
      .attr("y", (link) => (link.source.y + link.target.y) / 2);

    graph.nodeGroups.attr("transform", (node) => `translate(${node.x},${node.y})`);
  }

  function loadLayoutOptions() {
    if (layoutOptionsPromise) return layoutOptionsPromise;
    layoutOptionsPromise = fetch("emit-d3.config.json")
      .then((response) => {
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        return response.json();
      })
      .then((config) => {
        const options = {
          ...(config && typeof config.layout_options === "object" ? config.layout_options : {}),
        };
        if (config && config.layout && typeof config.layout === "object") {
          Object.assign(options, config.layout);
          delete options.type;
        }
        return options;
      })
      .catch(() => ({}));
    return layoutOptionsPromise;
  }

  async function makeSnapshot() {
    if (running) return;
    if (typeof engine !== "function") {
      console.warn("Kamada-Kawai snapshot engine is unavailable");
      return;
    }

    const threshold = Number(slider.value);
    if (!Number.isFinite(threshold)) return;

    const subgraph = visibleSubgraph(threshold);
    if (subgraph.links.length === 0 || subgraph.nodes.length === 0) return;

    if (snapshotFloor === null) captureBaselinePositions();

    running = true;
    button.disabled = true;
    slider.disabled = true;

    try {
      const options = await loadLayoutOptions();
      const result = await engine(subgraph.nodes, subgraph.links, {
        width: svgElement.clientWidth || 960,
        height: svgElement.clientHeight || 720,
        ...options,
      });

      renderPositions();
      setSnapshotFloor(threshold);
      sliderApi.setThreshold(threshold);

      globalThis.dispatchEvent(new CustomEvent("emit-kk-snapshot", {
        detail: {
          threshold,
          nodes: subgraph.nodes.length,
          edges: subgraph.links.length,
          result,
        },
      }));
    } finally {
      slider.disabled = false;
      button.disabled = false;
    }
  }

  button.addEventListener("click", () => {
    void makeSnapshot();
  });

  // Reset Z ends KK snapshot mode completely. It restores the coordinates from
  // before the first KK snapshot, releases the KK floor, and then returns to
  // the ordinary minimum-Z view. This makes another KK snapshot possible
  // without reloading the page.
  if (resetButton) {
    resetButton.addEventListener("click", (event) => {
      if (snapshotFloor === null) return;
      event.preventDefault();
      event.stopImmediatePropagation();
      clearSnapshot({ restorePositions: true });
      sliderApi.setThreshold(sliderApi.dataMin);
    }, true);
  }

  // Changing alpha/beta rebuilds the Z landscape, so a KK snapshot made under
  // the previous landscape is no longer a valid anchor. Restore the ordinary
  // coordinates and release the snapshot floor.
  globalThis.addEventListener("emit-weight-change", () => {
    if (snapshotFloor !== null) clearSnapshot({ restorePositions: true });
  });

  globalThis.emitKKSnapshot = Object.freeze({
    run: makeSnapshot,
    clear: clearSnapshot,
    get active() { return snapshotFloor !== null; },
    get floor() { return snapshotFloor; },
  });

  setButtonState(false);
})();
