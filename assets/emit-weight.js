(function () {
  "use strict";

  const data = globalThis.emitData;
  const graph = globalThis.emitGraph;
  const sliderApi = globalThis.emitSlider;

  if (!data || !Array.isArray(data.links)) {
    throw new Error("emit-weight.js requires emit-data.js to be loaded first");
  }
  if (!graph || !sliderApi) {
    throw new Error("emit-weight.js requires emit-d3.js and emit-slider.js first");
  }

  const alphaSlider = document.getElementById("emit-alpha-slider");
  const betaSlider = document.getElementById("emit-beta-slider");
  const alphaOutput = document.getElementById("emit-alpha-value");
  const betaOutput = document.getElementById("emit-beta-value");
  const stateOutput = document.getElementById("emit-weight-state");

  if (!alphaSlider || !betaSlider || !alphaOutput || !betaOutput || !stateOutput) {
    throw new Error("emit-weight.js could not find the alpha-beta controls");
  }

  const renderLinks = new Map(
    (Array.isArray(graph.links) ? graph.links : []).map((link) => [link.element_id, link]),
  );

  const componentReady = data.links.every((link) =>
    Number.isFinite(Number(link.g)) &&
    Number.isFinite(Number(link.c)) &&
    Number.isFinite(Number(link.p)) &&
    Number(link.g) >= 0 &&
    Number(link.c) > 0 &&
    Number(link.p) > 0,
  );

  for (const control of [alphaSlider, betaSlider]) {
    control.min = "0";
    control.max = "1";
    control.step = "0.01";
    control.value = "1";
  }

  function clamp01(value) {
    const numeric = Number(value);
    if (!Number.isFinite(numeric)) return 0;
    return Math.max(0, Math.min(1, numeric));
  }

  function formatContribution(value) {
    return clamp01(value).toFixed(2);
  }

  function anchorName(alpha, beta) {
    const epsilon = 1e-12;
    const a0 = Math.abs(alpha) < epsilon;
    const a1 = Math.abs(alpha - 1) < epsilon;
    const b0 = Math.abs(beta) < epsilon;
    const b1 = Math.abs(beta - 1) < epsilon;
    if (a0 && b0) return "G";
    if (a1 && b0) return "CG · M01";
    if (a0 && b1) return "PG";
    if (a1 && b1) return "PCG · M16";
    return "G C^α P^β";
  }

  function setOutputs(alpha, beta, mean = null, sd = null) {
    alphaOutput.textContent = `α(C) ${formatContribution(alpha)}`;
    betaOutput.textContent = `β(P) ${formatContribution(beta)}`;
    const statistics = Number.isFinite(mean) && Number.isFinite(sd)
      ? ` · μ ${Number(mean.toPrecision(5))} · σ ${Number(sd.toPrecision(5))}`
      : "";
    stateOutput.textContent = `${anchorName(alpha, beta)}${statistics}`;
  }

  if (!componentReady || data.links.length === 0) {
    alphaSlider.disabled = true;
    betaSlider.disabled = true;
    setOutputs(1, 1);
    stateOutput.textContent = "G/C/P components unavailable; static Z only";
    globalThis.emitWeight = Object.freeze({ available: false });
    return;
  }

  let frame = null;
  let alpha = 1;
  let beta = 1;
  let mean = 0;
  let sd = 0;

  function updateRenderedLink(link) {
    const rendered = renderLinks.get(link.element_id);
    if (rendered) {
      rendered.w = link.w;
      rendered.z = link.z;
    }
    const element = document.getElementById(link.element_id);
    if (element) {
      element.setAttribute("data-z", String(link.z));
      element.setAttribute("data-w", String(link.w));
    }
  }

  function recompute(nextAlpha = alpha, nextBeta = beta, options = {}) {
    alpha = clamp01(nextAlpha);
    beta = clamp01(nextBeta);
    alphaSlider.value = String(alpha);
    betaSlider.value = String(beta);

    let count = 0;
    let runningMean = 0;
    let m2 = 0;

    for (const link of data.links) {
      const g = Number(link.g);
      const c = Number(link.c);
      const p = Number(link.p);
      const w = g * Math.pow(c, alpha) * Math.pow(p, beta);
      link.w = w;

      count++;
      const delta = w - runningMean;
      runningMean += delta / count;
      const delta2 = w - runningMean;
      m2 += delta * delta2;
    }

    mean = runningMean;
    sd = count > 1 ? Math.sqrt(m2 / (count - 1)) : 0;
    if (!Number.isFinite(sd) || sd < 0) sd = 0;

    for (const link of data.links) {
      link.z = sd > 0 ? (link.w - mean) / sd : 0;
      updateRenderedLink(link);
    }

    setOutputs(alpha, beta, mean, sd);
    const zDetail = sliderApi.refreshLandscape({
      preserveThreshold: options.preserveThreshold !== false,
    });

    const detail = {
      alpha,
      beta,
      mean,
      sd,
      anchor: anchorName(alpha, beta),
      zDetail,
    };
    globalThis.dispatchEvent(new CustomEvent("emit-weight-change", { detail }));
    return detail;
  }

  function scheduleRecompute() {
    if (frame !== null) cancelAnimationFrame(frame);
    frame = requestAnimationFrame(() => {
      frame = null;
      recompute(alphaSlider.value, betaSlider.value, { preserveThreshold: true });
    });
  }

  alphaSlider.addEventListener("input", scheduleRecompute);
  betaSlider.addEventListener("input", scheduleRecompute);

  globalThis.emitWeight = Object.freeze({
    available: true,
    alphaSlider,
    betaSlider,
    recompute,
    set(alphaValue, betaValue) {
      return recompute(alphaValue, betaValue, { preserveThreshold: true });
    },
    reset() {
      return recompute(1, 1, { preserveThreshold: true });
    },
    get alpha() { return alpha; },
    get beta() { return beta; },
    get mean() { return mean; },
    get sd() { return sd; },
  });

  recompute(1, 1, { preserveThreshold: true });
})();
