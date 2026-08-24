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
    const points = [];
    let index = 0;

    while (index < links.length) {
      const threshold = links[index].z;
      let next = index;

      while (next < links.length && links[next].z === threshold) {
        activeNodes.add(links[next].source);
        activeNodes.add(links[next].target);
        next++;
      }

      points.push({
        z: threshold,
        edges: next,
        nodes: activeNodes.size,
        edgePercent: totalEdges > 0 ? (next / totalEdges) * 100 : 0,
        nodePercent: totalNodes > 0 ? (activeNodes.size / totalNodes) * 100 : 0,
      });

      index = next;
    }

    return points.sort((a, b) => a.z - b.z);
  }

  const points = buildRetentionPoints();
  if (points.length === 0) {
    toggleButton.disabled = true;
    return;
  }

  const maxGapPoint = points.reduce((best, point) => {
    const gap = point.nodePercent - point.edgePercent;
    const bestGap = best.nodePercent - best.edgePercent;
    return gap > bestGap ? point : best;
  });

  const width = 400;
  const height = 400;
  const margin = { top: 34, right: 24, bottom: 34, left: 42 };
  const innerWidth = width - margin.left - margin.right;
  const innerHeight = height - margin.top - margin.bottom;
  const zMin = sliderApi.dataMin;
  const zMax = sliderApi.dataMax;
  const zSpan = zMax - zMin || 1;

  const x = (z) => margin.left + ((z - zMin) / zSpan) * innerWidth;
  const y = (percent) => margin.top + ((100 - percent) / 100) * innerHeight;

  function pathFor(key) {
    return points
      .map((point, index) => `${index === 0 ? "M" : "L"} ${x(point.z)} ${y(point[key])}`)
      .join(" ");
  }

  svg.replaceChildren();
  svg.setAttribute("viewBox", `0 0 ${width} ${height}`);
  svg.setAttribute("role", "img");
  svg.setAttribute("aria-label", "retained edge and node percentages by Z threshold");

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
  const maxGapLine = svgElement("line", {
    x1: maxGapX,
    y1: margin.top,
    x2: maxGapX,
    y2: xAxisY,
    class: "emit-retention-max-gap",
  });
  svg.append(maxGapLine);

  const currentLine = svgElement("line", {
    x1: x(zMin), y1: margin.top, x2: x(zMin), y2: xAxisY, class: "emit-retention-current",
  });
  const edgeDot = svgElement("circle", { r: 4, class: "emit-retention-edge-dot" });
  const nodeDot = svgElement("circle", { r: 4, class: "emit-retention-node-dot" });
  svg.append(currentLine, edgeDot, nodeDot);

  const edgeLegend = svgElement("text", { x: margin.left + 8, y: 16, class: "emit-retention-edge-legend" });
  edgeLegend.textContent = "edges";
  const nodeLegend = svgElement("text", { x: margin.left + 80, y: 16, class: "emit-retention-node-legend" });
  nodeLegend.textContent = "nodes";
  const maxGapLegend = svgElement("text", { x: margin.left + 150, y: 16, class: "emit-retention-max-gap-legend" });
  maxGapLegend.textContent = "max n-e gap";
  svg.append(edgeLegend, nodeLegend, maxGapLegend);

  function pointForThreshold(threshold) {
    let chosen = points[0];
    for (const point of points) {
      if (point.z < threshold) continue;
      chosen = point;
      break;
    }
    return chosen;
  }

  function update(detail) {
    const threshold = Number(detail?.threshold ?? sliderApi.slider.value);
    const visibleEdges = Number(detail?.visibleEdges);
    const visibleNodes = Number(detail?.visibleNodes);
    const totalEdges = Number(detail?.totalEdges ?? data.links.length);
    const totalNodes = Number(detail?.totalNodes ?? data.nodes.length);
    const fallback = pointForThreshold(threshold);
    const edgePercent = Number.isFinite(visibleEdges) && totalEdges > 0 ? (visibleEdges / totalEdges) * 100 : fallback.edgePercent;
    const nodePercent = Number.isFinite(visibleNodes) && totalNodes > 0 ? (visibleNodes / totalNodes) * 100 : fallback.nodePercent;
    const xx = x(Math.max(zMin, Math.min(zMax, threshold)));

    currentLine.setAttribute("x1", String(xx));
    currentLine.setAttribute("x2", String(xx));
    edgeDot.setAttribute("cx", String(xx));
    edgeDot.setAttribute("cy", String(y(edgePercent)));
    nodeDot.setAttribute("cx", String(xx));
    nodeDot.setAttribute("cy", String(y(nodePercent)));

    valueOutput.textContent = `edges ${edgePercent.toFixed(1)}% · nodes ${nodePercent.toFixed(1)}%`;
  }

  toggleButton.addEventListener("click", () => {
    panel.hidden = !panel.hidden;
    toggleButton.setAttribute("aria-pressed", String(!panel.hidden));
  });

  globalThis.addEventListener("emit-z-change", (event) => update(event.detail));
  update();

  globalThis.emitRetention = Object.freeze({ panel, svg, points, maxGapPoint, update });
})();
