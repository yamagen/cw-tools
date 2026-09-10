(function () {
  "use strict";

  const d3 = globalThis.d3;
  const graph = globalThis.emitGraph;
  if (!d3 || !graph) throw new Error("emit-layout-controls.js requires d3 and emit-d3.js first");

  const forceButton = document.getElementById("emit-force");
  const kkButton = document.getElementById("emit-kk");
  const bfsButton = document.getElementById("emit-bfs");
  const topOutput = document.getElementById("emit-top-node");
  const svgElement = document.querySelector("#emit-graph, #graph");
  if (!forceButton || !kkButton || !bfsButton || !svgElement) return;

  let simulation = null;

  function endpointId(endpoint) {
    return typeof endpoint === "object" && endpoint !== null ? endpoint.id : endpoint;
  }

  function elementIsVisible(element) {
    if (!element || element.classList.contains("is-hidden")) return false;
    if (element.style.opacity === "0") return false;
    return true;
  }

  function visibleSubgraph() {
    const links = graph.links.filter((link) => elementIsVisible(document.getElementById(link.element_id)));
    const ids = new Set();
    const degree = new Map();
    for (const link of links) {
      const s = endpointId(link.source);
      const t = endpointId(link.target);
      ids.add(s); ids.add(t);
      degree.set(s, (degree.get(s) || 0) + 1);
      degree.set(t, (degree.get(t) || 0) + 1);
    }
    const nodes = graph.nodes.filter((node) => ids.has(node.id) && elementIsVisible(document.getElementById(node.element_id)));
    return { nodes, links, degree };
  }

  function topNode(subgraph = visibleSubgraph()) {
    return [...subgraph.nodes].sort((a, b) => {
      const d = (subgraph.degree.get(b.id) || 0) - (subgraph.degree.get(a.id) || 0);
      return d || String(a.id).localeCompare(String(b.id));
    })[0] || null;
  }

  function updateTopNode() {
    if (!topOutput) return;
    const node = topNode();
    topOutput.textContent = node ? `Top: ${node.label ?? node.id}` : "Top: —";
  }

  function setActive(name) {
    for (const [key, button] of [["force", forceButton], ["kk", kkButton], ["bfs", bfsButton]]) {
      button.setAttribute("aria-pressed", key === name ? "true" : "false");
    }
  }

  function renderPositions() {
    graph.edgeGroups.select(".edge-hit")
      .attr("x1", (link) => link.source.x).attr("y1", (link) => link.source.y)
      .attr("x2", (link) => link.target.x).attr("y2", (link) => link.target.y);
    graph.edgeGroups.select(".edge-line")
      .attr("x1", (link) => link.source.x).attr("y1", (link) => link.source.y)
      .attr("x2", (link) => link.target.x).attr("y2", (link) => link.target.y);
    graph.edgeGroups.select(".edge-label")
      .attr("x", (link) => (link.source.x + link.target.x) / 2)
      .attr("y", (link) => (link.source.y + link.target.y) / 2);
    graph.nodeGroups.attr("transform", (node) => `translate(${node.x},${node.y})`);
  }

  function runForce() {
    const subgraph = visibleSubgraph();
    if (!subgraph.nodes.length || !subgraph.links.length) return;
    if (simulation) simulation.stop();

    for (const node of subgraph.nodes) { node.fx = null; node.fy = null; }
    setActive("force");
    updateTopNode();
    forceButton.disabled = true;

    const width = svgElement.clientWidth || 960;
    const height = svgElement.clientHeight || 720;
    simulation = d3.forceSimulation(subgraph.nodes)
      .force("link", d3.forceLink(subgraph.links).id((node) => node.id).distance((link) => {
        const s = Number(link.source.degree ?? 0);
        const t = Number(link.target.degree ?? 0);
        const m = Math.max(s, t);
        return m >= 12 ? 50 : m >= 3 ? 30 : 10;
      }))
      .force("charge", d3.forceManyBody().strength(-50))
      .force("x", d3.forceX(width / 1.4).strength(0.02))
      .force("y", d3.forceY(height / 2).strength(0.04))
      .alpha(1)
      .on("tick", renderPositions)
      .on("end", () => {
        for (const node of subgraph.nodes) { node.fx = node.x; node.fy = node.y; }
        forceButton.disabled = false;
        globalThis.dispatchEvent(new CustomEvent("emit-force-layout", { detail: { nodes: subgraph.nodes.length, edges: subgraph.links.length } }));
      });
  }

  forceButton.addEventListener("click", runForce);
  globalThis.addEventListener("emit-kk-snapshot", () => { setActive("kk"); updateTopNode(); });
  globalThis.addEventListener("emit-bfs-layout", () => { setActive("bfs"); updateTopNode(); });
  globalThis.addEventListener("emit-weight-change", () => { setActive(null); updateTopNode(); });
  globalThis.addEventListener("emit-layout-ready", (event) => {
    if (event.detail?.layout === "force") setActive("force");
    updateTopNode();
  });

  updateTopNode();
  globalThis.emitLayoutControls = Object.freeze({ runForce, updateTopNode, topNode });
})();
