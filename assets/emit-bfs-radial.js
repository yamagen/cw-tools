(function () {
  "use strict";

  const d3 = globalThis.d3;
  const graph = globalThis.emitGraph;

  if (!d3 || !graph) throw new Error("emit-bfs-radial.js requires d3 and emit-d3.js first");

  const button = document.getElementById("emit-bfs");
  const svgElement = document.querySelector("#emit-graph, #graph");
  if (!button || !svgElement) return;

  let simulation = null;
  const endpointId = (endpoint) => typeof endpoint === "object" && endpoint !== null ? endpoint.id : endpoint;
  const elementIsVisible = (element) => !!element && !element.classList.contains("is-hidden") && element.style.opacity !== "0";

  function visibleSubgraph() {
    const links = graph.links.filter((link) => elementIsVisible(document.getElementById(link.element_id)));
    const ids = new Set();
    for (const link of links) { ids.add(endpointId(link.source)); ids.add(endpointId(link.target)); }
    const nodes = graph.nodes.filter((node) => ids.has(node.id) && elementIsVisible(document.getElementById(node.element_id)));
    return { nodes, links };
  }

  function renderPositions() {
    graph.edgeGroups.select(".edge-hit").attr("x1", (l) => l.source.x).attr("y1", (l) => l.source.y).attr("x2", (l) => l.target.x).attr("y2", (l) => l.target.y);
    graph.edgeGroups.select(".edge-line").attr("x1", (l) => l.source.x).attr("y1", (l) => l.source.y).attr("x2", (l) => l.target.x).attr("y2", (l) => l.target.y);
    graph.edgeGroups.select(".edge-label").attr("x", (l) => (l.source.x + l.target.x) / 2).attr("y", (l) => (l.source.y + l.target.y) / 2);
    graph.nodeGroups.attr("transform", (node) => `translate(${node.x},${node.y})`);
  }

  function bfsDepths(nodes, links) {
    const nodeIds = new Set(nodes.map((node) => node.id));
    const adjacency = new Map(nodes.map((node) => [node.id, []]));
    for (const link of links) {
      const source = endpointId(link.source), target = endpointId(link.target);
      if (!nodeIds.has(source) || !nodeIds.has(target)) continue;
      adjacency.get(source).push(target); adjacency.get(target).push(source);
    }
    const root = [...nodes].sort((a, b) => {
      const degreeDiff = adjacency.get(b.id).length - adjacency.get(a.id).length;
      return degreeDiff || String(a.id).localeCompare(String(b.id));
    })[0];
    const depth = new Map([[root.id, 0]]), queue = [root.id];
    for (let head = 0; head < queue.length; head++) {
      const current = queue[head], nextDepth = depth.get(current) + 1;
      for (const neighbor of adjacency.get(current)) {
        if (depth.has(neighbor)) continue;
        depth.set(neighbor, nextDepth); queue.push(neighbor);
      }
    }
    const maxDepth = Math.max(0, ...depth.values());
    for (const node of nodes) if (!depth.has(node.id)) depth.set(node.id, maxDepth + 1);
    return { root, depth };
  }

  function run() {
    const subgraph = visibleSubgraph();
    if (!subgraph.nodes.length || !subgraph.links.length) return;
    if (simulation) simulation.stop();
    const { root, depth } = bfsDepths(subgraph.nodes, subgraph.links);
    const width = svgElement.clientWidth || 960, height = svgElement.clientHeight || 720;
    const cx = width / 2, cy = height / 2, ring = 64;
    for (const node of subgraph.nodes) { node.fx = null; node.fy = null; }
    root.x = cx; root.y = cy; root.fx = cx; root.fy = cy;
    button.disabled = true;
    button.setAttribute("aria-pressed", "true");

    simulation = d3.forceSimulation(subgraph.nodes)
      .force("link", d3.forceLink(subgraph.links).id((node) => node.id).distance(ring).strength(0.45))
      .force("charge", d3.forceManyBody().strength(-70))
      .force("collide", d3.forceCollide().radius((node) => Math.max(13, Number(node.font_size || 12))))
      .force("radial-depth", d3.forceRadial((node) => ring * (depth.get(node.id) || 0), cx, cy).strength((node) => node === root ? 1 : 0.9))
      .alpha(1).alphaDecay(0.035).on("tick", renderPositions).on("end", () => {
        for (const node of subgraph.nodes) { node.fx = node.x; node.fy = node.y; }
        button.disabled = false;
        globalThis.dispatchEvent(new CustomEvent("emit-bfs-layout", { detail: { root: root.id, nodes: subgraph.nodes.length, edges: subgraph.links.length, maxDepth: Math.max(...depth.values()) } }));
      });
  }

  button.addEventListener("click", run);
  globalThis.addEventListener("emit-weight-change", () => button.setAttribute("aria-pressed", "false"));
  globalThis.emitBFSRadial = Object.freeze({ run });

  // This viewer loads BFS last among the three layout modules, so attach the
  // shared Force/KK/BFS controls here without requiring another template edit.
  if (!globalThis.emitLayoutControls && !document.querySelector('script[data-emit-layout-controls]')) {
    const script = document.createElement("script");
    script.src = "../assets/emit-layout-controls.js";
    script.dataset.emitLayoutControls = "true";
    document.body.appendChild(script);
  }
})();
