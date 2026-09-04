(function () {
  "use strict";

  const SVG_NS = "http://www.w3.org/2000/svg";
  const data = globalThis.emitData;
  const sliderApi = globalThis.emitSlider;
  const alphaApi = globalThis.emitAlpha;

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

  function currentAlphaThreshold() {
    return alphaApi ? alphaApi.currentAlpha() : -Infinity;
  }

  function buildRetentionPoints(alphaThreshold) {
    const links = data.links
      .map((link) => ({
        z: Number(link.z),
        alpha: Number(link.alpha),
        source: endpointId(link.source),
        target: endpointId(link.target),
      }))
      .filter((link) =>
        Number.isFinite(link.z) &&
        (!alphaApi || (Number.isFinite(link.alpha) && link.alpha >= alphaThreshold)),
      )
      .sort((a, b) => b.z - a.z);

    const totalEdges = data.links.length;
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

  svg.replaceChildren();
  svg.setAttribute("viewBox", `0 0 ${width} ${height}`);
  svg.setAttribute("role", "img");
  svg.setAttribute("aria-label", "retained edge and node percentages by Z threshold at the current alpha threshold");

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

  const edgePath = svgElement("path", { class: "emit-retention-edge-curve" });
  const nodePath = svgElement("path", { class: "emit-retention-node-curve" });
  svg.append(edgePath, nodePath);

  const maxGapLine = svgElement("line", {
    x1: x(zMin),
    y1: margin.top,
    x2: x(zMin),
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

  let points = [];
  let maxGapPoint = null;
  let alphaThreshold = currentAlphaThreshold();

  function pathFor(key) {
    return points
      .map((point, index) => `${index === 0 ? "M" : "L"} ${x(point.z)} ${y(point[key])}`)
      .join(" ");
  }

  function rebuild(nextAlphaThreshold) {
    alphaThreshold = Number(nextAlphaThreshold);
    points = buildRetentionPoints(alphaThreshold);

    edgePath.setAttribute("d", pathFor("edgePercent"));
    nodePath.setAttribute("d", pathFor("nodePercent"));

    if (points.length === 0) {
      maxGapPoint = null;
      maxGapLine.style.display = "none";
      return;
    }

    maxGapPoint = points.reduce((best, point) => {
      const gap = point.nodePercent - point.edgePercent;
      const bestGap = best.nodePercent - best.edgePercent;
      return gap > bestGap ? point : best;
    });

    const xx = x(maxGapPoint.z);
    maxGapLine.style.display = "";
    maxGapLine.setAttribute("x1", String(xx));
    maxGapLine.setAttribute("x2", String(xx));
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

  function update(detail) {
    const nextAlpha = Number(detail?.alphaThreshold ?? currentAlphaThreshold());
    if (!Number.isFinite(alphaThreshold) || nextAlpha !== alphaThreshold) {
      rebuild(nextAlpha);
    }

    const threshold = Number(detail?.zThreshold ?? detail?.threshold ?? sliderApi.slider.value);
    const visibleEdges = Number(detail?.visibleEdges);
    const visibleNodes = Number(detail?.visibleNodes);
    const totalEdges = Number(detail?.totalEdges ?? data.links.length);
    const totalNodes = Number(detail?.totalNodes ?? data.nodes.length);
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

    const alphaText = alphaApi && Number.isFinite(nextAlpha) ? ` · α ${Number(nextAlpha.toPrecision(5))}` : "";
    valueOutput.textContent = `edges ${edgePercent.toFixed(1)}% · nodes ${nodePercent.toFixed(1)}%${alphaText}`;
  }

  toggleButton.addEventListener("click", () => {
    panel.hidden = !panel.hidden;
    toggleButton.setAttribute("aria-pressed", String(!panel.hidden));
  });

  rebuild(alphaThreshold);
  globalThis.addEventListener("emit-filter-change", (event) => update(event.detail));
  if (!alphaApi) {
    globalThis.addEventListener("emit-z-change", (event) => update(event.detail));
  }
  update();

  globalThis.emitRetention = Object.freeze({
    panel,
    svg,
    update,
    rebuild,
    get points() { return points; },
    get maxGapPoint() { return maxGapPoint; },
    get alphaThreshold() { return alphaThreshold; },
  });
})();
