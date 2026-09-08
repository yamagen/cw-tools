(function () {
  "use strict";

  const d3 = globalThis.d3;
  if (!d3) return;

  function radiusForNode(node) {
    const fontSize = Number(node?.font_size ?? 10);
    // Keep the existing semantic channel (font_size), but give circle size
    // more visual contrast, closer to the Graphviz rendering.
    return Math.max(8, Math.min(30, 4 + fontSize * 0.95));
  }

  function applyNodeScale() {
    d3.selectAll("#emit-graph .emit-nodes > g circle, #graph .emit-nodes > g circle")
      .attr("r", radiusForNode);
  }

  applyNodeScale();
  globalThis.addEventListener("emit-layout-ready", applyNodeScale);
  globalThis.addEventListener("emit-kk-snapshot", applyNodeScale);

  globalThis.emitNodeScale = Object.freeze({
    apply: applyNodeScale,
    radiusForNode,
  });
})();