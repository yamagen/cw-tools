(function () {
  "use strict";

  const SVG_NS = "http://www.w3.org/2000/svg";
  const data = globalThis.emitData;
  const graph = globalThis.emitGraph;

  if (!data || !Array.isArray(data.nodes) || !Array.isArray(data.links)) {
    throw new Error("emit-slider.js requires emit-data.js to be loaded first");
  }
  if (!graph) {
    throw new Error("emit-slider.js requires emit-d3.js to be loaded first");
  }

  const slider = document.querySelector("#emit-z-slider");
  const valueOutput = document.querySelector("#emit-z-value");
  const countOutput = document.querySelector("#emit-z-count");
  const minOutput = document.querySelector("#emit-z-min");
  const maxOutput = document.querySelector("#emit-z-max");
  const resetButton = document.querySelector("#emit-z-reset");
  const distribution = document.querySelector("#emit-z-distribution");

  if (!slider || !valueOutput || !countOutput || !distribution) {
    throw new Error("emit-slider.js could not find the required slider controls");
  }

  const zValues = data.links.map((link) => Number(link.z)).filter((value) => Number.isFinite(value));

  if (zValues.length === 0) {
    slider.disabled = true;
    valueOutput.textContent = "no Z data";
    countOutput.textContent = "0 edges / 0 nodes";
    return;
  }

  const dataMin = Math.min(...zValues);
  const dataMax = Math.max(...zValues);
  const dataSpan = dataMax - dataMin;
  const sliderStep = niceStep(dataSpan === 0 ? Math.max(1, Math.abs(dataMin)) / 100 : dataSpan / 200);
  const decimals = decimalsForStep(sliderStep);

  slider.min = String(dataMin);
  slider.max = String(dataMax);
  slider.step = String(sliderStep);
  slider.value = String(dataMin);
  slider.disabled = dataSpan === 0;

  if (minOutput) minOutput.textContent = formatNumber(dataMin);
  if (maxOutput) maxOutput.textContent = formatNumber(dataMax);

  const distributionView = buildDistribution(distribution, zValues, dataMin, dataMax);

  function niceStep(value) {
    if (!Number.isFinite(value) || value <= 0) return 0.01;
    const exponent = 10 ** Math.floor(Math.log10(value));
    const fraction = value / exponent;
    const niceFraction = fraction <= 1 ? 1 : fraction <= 2 ? 2 : fraction <= 5 ? 5 : 10;
    return niceFraction * exponent;
  }

  function decimalsForStep(step) {
    if (step >= 1) return 0;
    return Math.min(8, Math.max(0, Math.ceil(-Math.log10(step))));
  }

  function formatNumber(value) {
    const fixed = Number(value).toFixed(decimals);
    return fixed.replace(/\.0+$/, "").replace(/(\.\d*?)0+$/, "$1");
  }

  function svgElement(name, attributes = {}) {
    const element = document.createElementNS(SVG_NS, name);
    for (const [key, value] of Object.entries(attributes)) {
      element.setAttribute(key, String(value));
    }
    return element;
  }

  function buildDistribution(svg, values, minimum, maximum) {
    const width = 400;
    const height = 400;
    const margin = { top: 10, right: 18, bottom: 26, left: 18 };
    const innerWidth = width - margin.left - margin.right;
    const innerHeight = height - margin.top - margin.bottom;
    const baseline = margin.top + innerHeight;
    const domainPadding = minimum === maximum ? Math.max(0.5, Math.abs(minimum) * 0.05) : 0;
    const domainMin = minimum - domainPadding;
    const domainMax = maximum + domainPadding;
    const binCount = Math.max(16, Math.min(60, Math.round(Math.sqrt(values.length) * 2)));
    const binWidth = (domainMax - domainMin) / binCount;
    const bins = Array.from({ length: binCount }, () => 0);

    for (const value of values) {
      const rawIndex = Math.floor((value - domainMin) / binWidth);
      const index = Math.max(0, Math.min(binCount - 1, rawIndex));
      bins[index]++;
    }

    const maxCount = Math.max(...bins, 1);
    const x = (value) => margin.left + ((value - domainMin) / (domainMax - domainMin)) * innerWidth;
    const y = (count) => baseline - (count / maxCount) * innerHeight;

    svg.replaceChildren();
    svg.setAttribute("viewBox", `0 0 ${width} ${height}`);
    svg.setAttribute("role", "img");
    svg.setAttribute("aria-label", "Z value distribution and selected threshold area");

    let pathData = `M ${x(domainMin)} ${baseline}`;
    for (let index = 0; index < bins.length; index++) {
      const x0 = x(domainMin + index * binWidth);
      const x1 = x(domainMin + (index + 1) * binWidth);
      const top = y(bins[index]);
      pathData += ` L ${x0} ${top} L ${x1} ${top}`;
    }
    pathData += ` L ${x(domainMax)} ${baseline} Z`;

    const clipId = "emit-z-selected-clip";
    const defs = svgElement("defs");
    const clipPath = svgElement("clipPath", { id: clipId });
    const clipRect = svgElement("rect", {
      x: margin.left,
      y: margin.top,
      width: innerWidth,
      height: innerHeight,
    });
    clipPath.append(clipRect);
    defs.append(clipPath);
    svg.append(defs);

    svg.append(svgElement("path", { d: pathData, class: "emit-distribution-base" }));
    svg.append(
      svgElement("path", {
        d: pathData,
        class: "emit-distribution-selected",
        "clip-path": `url(#${clipId})`,
      }),
    );
    svg.append(
      svgElement("line", {
        x1: margin.left,
        y1: baseline,
        x2: width - margin.right,
        y2: baseline,
        class: "emit-distribution-axis",
      }),
    );

    const thresholdLine = svgElement("line", {
      x1: margin.left,
      y1: margin.top,
      x2: margin.left,
      y2: baseline,
      class: "emit-distribution-threshold",
    });
    svg.append(thresholdLine);

    const minimumLabel = svgElement("text", {
      x: margin.left,
      y: height - 6,
      class: "emit-distribution-label",
      "text-anchor": "start",
    });
    minimumLabel.textContent = formatNumber(minimum);
    svg.append(minimumLabel);

    const maximumLabel = svgElement("text", {
      x: width - margin.right,
      y: height - 6,
      class: "emit-distribution-label",
      "text-anchor": "end",
    });
    maximumLabel.textContent = formatNumber(maximum);
    svg.append(maximumLabel);

    return {
      update(threshold) {
        const thresholdX = Math.max(margin.left, Math.min(width - margin.right, x(threshold)));
        clipRect.setAttribute("x", String(thresholdX));
        clipRect.setAttribute("width", String(width - margin.right - thresholdX));
        thresholdLine.setAttribute("x1", String(thresholdX));
        thresholdLine.setAttribute("x2", String(thresholdX));
      },
    };
  }

  function endpointId(endpoint) {
    return typeof endpoint === "object" && endpoint !== null ? endpoint.id : endpoint;
  }

  function applyThreshold(threshold) {
    const connected = new Set();
    let visibleEdges = 0;

    for (const link of data.links) {
      const visible = Number(link.z) >= threshold;
      const element = document.getElementById(link.element_id);
      if (element) element.classList.toggle("is-hidden", !visible);
      if (!visible) continue;

      visibleEdges++;
      connected.add(endpointId(link.source));
      connected.add(endpointId(link.target));
    }

    let visibleNodes = 0;
    for (const node of data.nodes) {
      const visible = connected.has(node.id);
      const element = document.getElementById(node.element_id);
      if (element) element.classList.toggle("is-hidden", !visible);
      if (visible) visibleNodes++;
    }

    valueOutput.textContent = `Z ≥ ${formatNumber(threshold)}`;
    countOutput.textContent = `${visibleEdges} / ${data.links.length} edges · ` + `${visibleNodes} / ${data.nodes.length} nodes`;
    distributionView.update(threshold);

    const detail = {
      threshold,
      visibleEdges,
      totalEdges: data.links.length,
      visibleNodes,
      totalNodes: data.nodes.length,
    };
    globalThis.dispatchEvent(new CustomEvent("emit-z-change", { detail }));
    return detail;
  }

  function setThreshold(value) {
    const threshold = Math.max(dataMin, Math.min(dataMax, Number(value)));
    slider.value = String(threshold);
    return applyThreshold(threshold);
  }

  slider.addEventListener("input", () => setThreshold(slider.value));
  if (resetButton) {
    resetButton.addEventListener("click", () => setThreshold(dataMin));
  }

  setThreshold(dataMin);

  globalThis.emitSlider = Object.freeze({
    slider,
    distribution,
    dataMin,
    dataMax,
    step: sliderStep,
    setThreshold,
    reset() {
      return setThreshold(dataMin);
    },
  });
})();
