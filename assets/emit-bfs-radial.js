(function () {
  "use strict";

  const d3 = globalThis.d3;
  const graph = globalThis.emitGraph;

  if (!d3 || !graph) {
    throw new Error("emit-bfs-radial.js requires d3 and emit-d3.js first");
  }

  const button = document.getElementById("emit-bfs");
  const svgElement = document.querySelector("#emit-graph, #graph");
  if (!button || !svgElement) return;

  let simulation = null;

  function endpointId(endpoint) {
    return typeof endpoint === "object" && endpoint !== null ? endpoint.id : endpoint;
  }

  function elementIsVisible(element) {
    if (!element) return false;
    if (element.classList.contains("is-hidden")) return false;
    if (element.style.opacity === "0") return false;
    return true;
  }

  function visibleSubgraph() {
    const links = graph.links.filter((link) =>
      elementIsVisible(document.getElementById(link.element_id)),
    );
    const ids = new Set();
    for (const link of links) {
      ids.add(endpointId(link.source));
      ids.add(endpointId(link.target));
    }
    const nodes = graph.nodes.filter((node) =>
      ids.has(node.id) && elementIsVisible(document.getElementById(node.element_id)),
    );
    return { nodes, links };
  }

  function renderPositions() {
    graph.edgeGroups.select(".edge-hit")
      .attr("x1", (link) => link.source.x)
      .attr("y1", (link) => link.source.y)
      .attr("x2", (link) => link.target.x)
      .attr("y2", (link) => link.target.y);

    graph.edgeGroups.select(".edge-line")
      .attr("x1", (link) => link.source.x)
      .attr("y1", (link) => link.source.y)
      .attr("x2", (link) => link.target.x)
      .attr("y2", (link) => link.target.y);

    graph.edgeGroups.select(".edge-label")
      .attr("x", (link) => (link.source.x + link.target.x) / 2)
      .attr("y", (link) => (link.source.y + link.target.y) / 2);

    graph.nodeGroups.attr("transform", (node) => `translate(${node.x},${node.y})`);
  }

  function bfsDepths(nodes, links) {
    const nodeIds = new Set(nodes.map((node) => node.id));
    const adjacency = new Map(nodes.map((node) => [node.id, []]));

    for (const link of links) {
      const source = endpointId(link.source);
      const target = endpointId(link.target);
      if (!nodeIds.has(source) || !nodeIds.has(target)) continue;
      adjacency.get(source).push(target);
      adjacency.get(target).push(source);
    }

    // The visible hub is the most useful automatic root for cw observation.
    // Ties are deterministic so repeated BFS snapshots are comparable.
    const root = [...nodes].sort((a, b) => {
      const degreeDiff = adjacency.get(b.id).length - adjacency.get(a.id).length;
      if (degreeDiff !== 0) return degreeDiff;
      return String(a.id).localeCompare(String(b.id));
    })[0];

    const depth = new Map([[root.id, 0]]);
    const queue = [root.id];
    for (let head = 0; head < queue.length; head++) {
      const current = queue[head];
      const nextDepth = depth.get(current) + 1;
      for (const neighbor of adjacency.get(current)) {
        if (depth.has(neighbor)) continue;
        depth.set(neighbor, nextDepth);
        queue.push(neighbor);
      }
    }

    // Disconnected visible components are placed outside the rooted component.
    const maxDepth = Math.max(0, ...depth.values());
    for (const node of nodes) {
      if (!depth.has(node.id)) depth.set(node.id, maxDepth + 1);
    }

    return { root, depth };
  }

  function run() {
    const subgraph = visibleSubgraph();
    if (subgraph.nodes.length === 0 || subgraph.links.length === 0) return;

    if (simulation) simulation.stop();

    const { root, depth } = bfsDepths(subgraph.nodes, subgraph.links);
    const width = svgElement.clientWidth || 960;
    const height = svgElement.clientHeight || 720;
    const cx = width / 2;
    const cy = height / 2;
    const ring = 64;

    for (const node of subgraph.nodes) {
      node.fx = null;
      node.fy = null;
    }
    root.x = cx;
    root.y = cy;
    root.fx = cx;
    root.fy = cy;

    button.disabled = true;
    button.setAttribute("aria-pressed", "true");
    button.textContent = `BFS · ${root.label ?? root.id}`;

    simulation = d3.forceSimulation(subgraph.nodes)
      .force("link", d3.forceLink(subgraph.links)
        .id((node) => node.id)
        .distance(ring)
        .strength(0.45))
      .force("charge", d3.forceManyBody().strength(-70))
      .force("collide", d3.forceCollide().radius((node) => Math.max(13, Number(node.font_size || 12))))
      .force("radial-depth", d3.forceRadial(
        (node) => ring * (depth.get(node.id) || 0),
        cx,
        cy,
      ).strength((node) => node === root ? 1 : 0.9))
      .alpha(1)
      .alphaDecay(0.035)
      .on("tick", renderPositions)
      .on("end", () => {
        for (const node of subgraph.nodes) {
          node.fx = node.x;
          node.fy = node.y;
        }
        button.disabled = false;
        globalThis.dispatchEvent(new CustomEvent("emit-bfs-layout", {
          detail: {
            root: root.id,
            nodes: subgraph.nodes.length,
            edges: subgraph.links.length,
            maxDepth: Math.max(...depth.values()),
          },
        }));
      });
  }

  button.addEventListener("click", run);

  globalThis.addEventListener("emit-weight-change", () => {
    button.setAttribute("aria-pressed", "false");
    button.textContent = "BFS";
  });

  globalThis.emitBFSRadial = Object.freeze({ run });
})();
