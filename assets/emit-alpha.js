(function () {
  "use strict";

  const data = globalThis.emitData;
  const sliderApi = globalThis.emitSlider;

  if (!data || !Array.isArray(data.nodes) || !Array.isArray(data.links)) {
    throw new Error("emit-alpha.js requires emit-data.js to be loaded first");
  }
  if (!sliderApi) {
    throw new Error("emit-alpha.js requires emit-slider.js to be loaded first");
  }

  const slider = document.getElementById("emit-alpha-slider");
  const valueOutput = document.getElementById("emit-alpha-value");
  const minOutput = document.getElementById("emit-alpha-min");
  const maxOutput = document.getElementById("emit-alpha-max");
  const countOutput = document.getElementById("emit-z-count");

  if (!slider || !valueOutput || !countOutput) {
    throw new Error("emit-alpha.js could not find the required alpha controls");
  }

  const nodeById = new Map(data.nodes.map((node) => [node.id, node]));

  function endpointId(endpoint) {
    return typeof endpoint === "object" && endpoint !== null ? endpoint.id : endpoint;
  }

  function endpointNode(endpoint) {
    if (typeof endpoint === "object" && endpoint !== null) return endpoint;
    return nodeById.get(endpoint);
  }

  function alphaForLink(link) {
    const source = endpointNode(link.source);
    const target = endpointNode(link.target);
    const idf1 = Number(source?.idf);
    const idf2 = Number(target?.idf);
    const ctf = Number(link.ctf);

    if (!Number.isFinite(idf1) || !Number.isFinite(idf2) ||
        !Number.isFinite(ctf) || idf1 < 0 || idf2 < 0 || ctf <= 0) {
      return NaN;
    }

    const g = Math.sqrt(idf1 * idf2);
    return (1 + Math.log(ctf)) * g;
  }

  for (const link of data.links) {
    const alpha = alphaForLink(link);
    link.alpha = alpha;

    const cw = Number(link.cw);
    link.pair_weight = Number.isFinite(alpha) && alpha !== 0 && Number.isFinite(cw)
      ? cw / alpha
      : NaN;
  }

  const alphaValues = Array.from(
    new Set(
      data.links
        .map((link) => Number(link.alpha))
        .filter((value) => Number.isFinite(value)),
    ),
  ).sort((a, b) => a - b);

  function formatNumber(value) {
    if (!Number.isFinite(value)) return "NA";
    const magnitude = Math.abs(value);
    if (magnitude !== 0 && (magnitude >= 10000 || magnitude < 0.001)) {
      return value.toExponential(3);
    }
    return Number(value.toPrecision(6)).toString();
  }

  function formatCount(visible, total, label) {
    const percent = total > 0 ? (visible / total) * 100 : 0;
    return `${visible} / ${total} ${label} (${percent.toFixed(1)}%)`;
  }

  function setCountOutput(visibleEdges, visibleNodes) {
    const edgeLine = document.createElement("span");
    edgeLine.textContent = formatCount(visibleEdges, data.links.length, "edges");

    const nodeLine = document.createElement("span");
    nodeLine.textContent = formatCount(visibleNodes, data.nodes.length, "nodes");

    countOutput.replaceChildren(edgeLine, nodeLine);
  }

  if (alphaValues.length === 0) {
    slider.disabled = true;
    valueOutput.textContent = "α: no CTF data";
    return;
  }

  slider.min = "0";
  slider.max = String(alphaValues.length - 1);
  slider.step = "1";
  slider.value = "0";
  slider.disabled = alphaValues.length <= 1;

  if (minOutput) minOutput.textContent = formatNumber(alphaValues[0]);
  if (maxOutput) maxOutput.textContent = formatNumber(alphaValues[alphaValues.length - 1]);

  function currentAlpha() {
    const index = Math.max(0, Math.min(alphaValues.length - 1, Number(slider.value) || 0));
    return alphaValues[index];
  }

  function currentZ() {
    const value = Number(sliderApi.slider.value);
    return Number.isFinite(value) ? value : sliderApi.dataMin;
  }

  function applyCombined(zThreshold = currentZ(), alphaThreshold = currentAlpha()) {
    const connected = new Set();
    let visibleEdges = 0;

    for (const link of data.links) {
      const z = Number(link.z);
      const alpha = Number(link.alpha);
      const visible = Number.isFinite(z) && Number.isFinite(alpha) &&
        z >= zThreshold && alpha >= alphaThreshold;
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

    valueOutput.textContent = `α ≥ ${formatNumber(alphaThreshold)}`;
    setCountOutput(visibleEdges, visibleNodes);

    const detail = {
      zThreshold,
      alphaThreshold,
      alphaIndex: Number(slider.value),
      visibleEdges,
      totalEdges: data.links.length,
      visibleNodes,
      totalNodes: data.nodes.length,
    };

    globalThis.dispatchEvent(new CustomEvent("emit-filter-change", { detail }));
    return detail;
  }

  function setAlphaIndex(index) {
    const bounded = Math.max(0, Math.min(alphaValues.length - 1, Number(index) || 0));
    slider.value = String(bounded);
    return applyCombined(currentZ(), alphaValues[bounded]);
  }

  slider.addEventListener("input", () => setAlphaIndex(slider.value));

  globalThis.addEventListener("emit-z-change", (event) => {
    const threshold = Number(event.detail?.threshold);
    applyCombined(Number.isFinite(threshold) ? threshold : currentZ(), currentAlpha());
  });

  applyCombined();

  globalThis.emitAlpha = Object.freeze({
    slider,
    values: alphaValues,
    dataMin: alphaValues[0],
    dataMax: alphaValues[alphaValues.length - 1],
    currentAlpha,
    setAlphaIndex,
    setAlphaValue(value) {
      const target = Number(value);
      if (!Number.isFinite(target)) return applyCombined();
      let index = 0;
      while (index + 1 < alphaValues.length && alphaValues[index] < target) index++;
      return setAlphaIndex(index);
    },
    reset() {
      return setAlphaIndex(0);
    },
  });
})();
