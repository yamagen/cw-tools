(function () {
  "use strict";

  const data = globalThis.emitData;
  const graph = globalThis.emitGraph;

  if (!data || !Array.isArray(data.nodes) || !Array.isArray(data.links)) {
    throw new Error("emit-interaction.js requires emit-data.js to be loaded first");
  }
  if (!graph) {
    throw new Error("emit-interaction.js requires emit-d3.js to be loaded first");
  }

  const svgElement = document.querySelector("#emit-graph, #graph");
  if (!svgElement) {
    throw new Error("emit-interaction.js could not find #emit-graph or #graph");
  }

  function endpointId(endpoint) {
    return typeof endpoint === "object" && endpoint !== null ? endpoint.id : endpoint;
  }

  function makeTransparent(element) {
    if (!element) return;
    element.style.opacity = "0";
    element.style.pointerEvents = "none";
  }

  function makeNodeAndEdgesTransparent(nodeId) {
    const node = data.nodes.find((item) => item.id === nodeId);
    if (!node) return;

    makeTransparent(document.getElementById(node.element_id));

    for (const link of data.links) {
      const sourceId = endpointId(link.source);
      const targetId = endpointId(link.target);
      if (sourceId !== nodeId && targetId !== nodeId) continue;
      makeTransparent(document.getElementById(link.element_id));
    }
  }

  svgElement.addEventListener("dblclick", (event) => {
    const nodeElement = event.target.closest(".node[data-node-id]");
    if (!nodeElement) return;

    event.preventDefault();
    event.stopPropagation();

    const nodeId = nodeElement.getAttribute("data-node-id");
    makeNodeAndEdgesTransparent(nodeId);

    const sourcePanel = document.getElementById("emit-source-panel");
    if (sourcePanel) sourcePanel.hidden = true;
  });

  globalThis.emitInteraction = Object.freeze({
    makeNodeAndEdgesTransparent,
  });
})();
