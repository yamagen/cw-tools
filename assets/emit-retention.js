(function () {
  "use strict";

  const SVG_NS = "http://www.w3.org/2000/svg";
  const data = globalThis.emitData;
  const sliderApi = globalThis.emitSlider;

  if (!data || !Array.isArray(data.nodes) || !Array.isArray(data.links)) {
    throw new Error("emit-retention.js requires emit-data.js to be loaded first");
  }
  if (!sliderApi) {
    throw new Error("emit-retention.js requires emit-slider.js to be loaded first");
  }

  const panel = document.getElementById("emit-retention-panel");
  const toggleButton = document.getElementById("emit-retention-toggle");
  const svg = document.getElementById("emit-retention-chart");
  const valueOutput = document.getElementById("emit-retention-value");

  if (!panel || !toggleButton || !svg || !valueOutput) {
    throw new Error("emit-retention.js could not find the required retention controls");
  }

  const width = 400;
  const height = 400;
  const margin = { top: 34, right: 24, bottom: 34, left: 42 };
  const innerWidth = width - margin.left - margin.right;
  const innerHeight = height - margin.top - margin.bottom;

  let points = [];
  let maxGapPoint = null;
  let zMin = 0;
  let zMax = 0;
  let x = () => margin.left;
  let y = (percent) => margin.top + ((100 - percent) / 100) * innerHeight;
  let currentLine = null;
  let edgeDot = null;
  let nodeDot = null;

  function endpointId(endpoint) {
    return typeof endpoint === "object" && endpoint !== null ? endpoint.id : endpoint;
  }

  function svgElement(name, attributes = {}) {
    const element = document.createElementNS(SVG_NS, name);
    for (const [key, value] of Object.entries(attributes)) {
      element.setAttribute(key, String(value));
    }
    return element;
  }

  function buildRetentionPoints() {
    const links = data.links
      .map((link) => ({
        z: Number(link.z),
        source: endpointId(link.source),
        target: endpointId(link.target),
      }))
      .filter((link) => Number.isFinite(link.z))
      .sort((a, b) => b.z - a.z);

    const totalEdges = links.length;
    const totalNodes = data.nodes.length;
    const activeNodes = new Set();
    const result = [];
    let index = 0;

    while (index < links.length) {
      const threshold = links[index].z;
      let next = index;
      while (next < links.length && links[next].z === threshold) {
        activeNodes.add(links[next].source);
        activeNodes.add(links[next].target);
        next++;
      }
      result.push({
        z: threshold,
        edges: next,
        nodes: activeNodes.size,
        edgePercent: totalEdges > 0 ? (next / totalEdges) * 100 : 0,
        nodePercent: totalNodes > 0 ? (activeNodes.size / totalNodes) * 100 : 0,
      });
      index = next;
    }

    return result.sort((a, b) => a.z - b.z);
  }

  function pathFor(key) {
    return points
      .map((point, index) => `${index === 0 ? "M" : "L"} ${x(point.z)} ${y(point[key])}`)
      .join(" ");
  }

  function renderLandscape() {
    points = buildRetentionPoints();
    zMin = sliderApi.dataMin;
    zMax = sliderApi.dataMax;
    const zSpan = zMax - zMin || 1;
    x = (z) => margin.left + ((z - zMin) / zSpan) * innerWidth;
    y = (percent) => margin.top + ((100 - percent) / 100) * innerHeight;

    svg.replaceChildren();
    svg.setAttribute("viewBox", `0 0 ${width} ${height}`);
    svg.setAttribute("role", "img");
    svg.setAttribute("aria-label", "retained edge and node percentages by current Z landscape");

    if (points.length === 0) {
      maxGapPoint = null;
      valueOutput.textContent = "no retention data";
      return;
    }

    maxGapPoint = points.reduce((best, point) => {
      const gap = point.nodePercent - point.edgePercent;
      const bestGap = best.nodePercent - best.edgePercent;
      return gap > bestGap ? point : best;
    });

    const xAxisY = height - margin.bottom;
    const yAxisX = margin.left;
    svg.append(svgElement("line", { x1: yAxisX, y1: margin.top, x2: yAxisX, y2: xAxisY, class: "emit-retention-axis" }));
    svg.append(svgElement("line", { x1: yAxisX, y1: xAxisY, x2: width - margin.right, y2: xAxisY, class: "emit-retention-axis" }));

    for (const percent of [0, 25, 50, 75, 100]) {
      const yy = y(percent);
      svg.append(svgElement("line", { x1: yAxisX, y1: yy, x2: width - margin.right, y2: yy, class: "emit-retention-grid" }));
      const label = svgElement("text", { x: margin.left - 7, y: yy + 4, "text-anchor": "end", class: "emit-retention-label" });
      label.textContent = `${percent}%`;
      svg.append(label);
    }

    for (const [z, anchor] of [[zMin, "start"], [0, "middle"], [zMax, "end"]]) {
      if (z < zMin || z > zMax) continue;
      const label = svgElement("text", { x: x(z), y: height - 9, "text-anchor": anchor, class: "emit-retention-label" });
      label.textContent = Number(z.toFixed(2)).toString();
      svg.append(label);
    }

    svg.append(svgElement("path", { d: pathFor("edgePercent"), class: "emit-retention-edge-curve" }));
    svg.append(svgElement("path", { d: pathFor("nodePercent"), class: "emit-retention-node-curve" }));

    const maxGapX = x(maxGapPoint.z);
    svg.append(svgElement("line", {
      x1: maxGapX,
      y1: margin.top,
      x2: maxGapX,
      y2: xAxisY,
      class: "emit-retention-max-gap",
    }));

    currentLine = svgElement("line", {
      x1: x(zMin), y1: margin.top, x2: x(zMin), y2: xAxisY, class: "emit-retention-current",
    });
    edgeDot = svgElement("circle", { r: 4, class: "emit-retention-edge-dot" });
    nodeDot = svgElement("circle", { r: 4, class: "emit-retention-node-dot" });
    svg.append(currentLine, edgeDot, nodeDot);

    const edgeLegend = svgElement("text", { x: margin.left + 8, y: 16, class: "emit-retention-edge-legend" });
    edgeLegend.textContent = "edges";
    const nodeLegend = svgElement("text", { x: margin.left + 80, y: 16, class: "emit-retention-node-legend" });
    nodeLegend.textContent = "nodes";
    const maxGapLegend = svgElement("text", { x: margin.left + 150, y: 16, class: "emit-retention-max-gap-legend" });
    maxGapLegend.textContent = "max n-e gap";
    svg.append(edgeLegend, nodeLegend, maxGapLegend);
  }

  function pointForThreshold(threshold) {
    if (points.length === 0) return null;
    let chosen = points[points.length - 1];
    for (const point of points) {
      if (point.z < threshold) continue;
      chosen = point;
      break;
    }
    return chosen;
  }

  function update(detail = {}) {
    if (points.length === 0 || !currentLine || !edgeDot || !nodeDot) return;

    const threshold = Number(detail.threshold ?? sliderApi.slider.value);
    const visibleEdges = Number(detail.visibleEdges);
    const visibleNodes = Number(detail.visibleNodes);
    const totalEdges = Number(detail.totalEdges ?? data.links.length);
    const totalNodes = Number(detail.totalNodes ?? data.nodes.length);
    const fallback = pointForThreshold(threshold);
    const edgePercent = Number.isFinite(visibleEdges) && totalEdges > 0
      ? (visibleEdges / totalEdges) * 100
      : (fallback?.edgePercent ?? 0);
    const nodePercent = Number.isFinite(visibleNodes) && totalNodes > 0
      ? (visibleNodes / totalNodes) * 100
      : (fallback?.nodePercent ?? 0);
    const xx = x(Math.max(zMin, Math.min(zMax, threshold)));

    currentLine.setAttribute("x1", String(xx));
    currentLine.setAttribute("x2", String(xx));
    edgeDot.setAttribute("cx", String(xx));
    edgeDot.setAttribute("cy", String(y(edgePercent)));
    nodeDot.setAttribute("cx", String(xx));
    nodeDot.setAttribute("cy", String(y(nodePercent)));

    const weightApi = globalThis.emitWeight;
    const state = weightApi?.available
      ? ` · α ${weightApi.alpha.toFixed(2)} · β ${weightApi.beta.toFixed(2)}`
      : "";
    valueOutput.textContent = `edges ${edgePercent.toFixed(1)}% · nodes ${nodePercent.toFixed(1)}%${state}`;
  }

  toggleButton.addEventListener("click", () => {
    panel.hidden = !panel.hidden;
    toggleButton.setAttribute("aria-pressed", String(!panel.hidden));
  });

  globalThis.addEventListener("emit-z-landscape-change", () => {
    renderLandscape();
    update();
  });
  globalThis.addEventListener("emit-z-change", (event) => update(event.detail));

  renderLandscape();
  update();

  globalThis.emitRetention = Object.freeze({
    panel,
    svg,
    update,
    renderLandscape,
    get points() { return points; },
    get maxGapPoint() { return maxGapPoint; },
  });
})();
