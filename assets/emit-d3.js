(function () {
  "use strict";

  const d3 = globalThis.d3;
  const data = globalThis.emitData;

  if (!d3) {
    throw new Error("emit-d3.js requires d3.v7.min.js to be loaded first");
  }
  if (!data || !Array.isArray(data.nodes) || !Array.isArray(data.links)) {
    throw new Error("emit-d3.js requires emit-data.js to be loaded first");
  }

  const svgElement = document.querySelector("#emit-graph, #graph");
  if (!svgElement) {
    throw new Error("emit-d3.js could not find #emit-graph or #graph");
  }

  const tipElement = document.querySelector("#emit-tip, #tip");
  const svg = d3.select(svgElement);
  const tip = tipElement ? d3.select(tipElement) : null;
  const nodes = data.nodes.map((node) => ({ ...node }));
  const links = data.links.map((link) => ({ ...link }));

  const graphClasses = Array.isArray(data.classes) ? data.classes : ["graph"];

  svg.attr("class", graphClasses.join(" ")).attr("role", "img").attr("aria-label", "interactive network graph");

  const width = () => svgElement.clientWidth || 960;
  const height = () => svgElement.clientHeight || 720;
  const updateViewBox = () => svg.attr("viewBox", `0 0 ${width()} ${height()}`);
  updateViewBox();

  const defs = svg.append("defs");
  defs.append("marker").attr("id", "emit-arrow").attr("viewBox", "0 -5 10 10").attr("refX", 16).attr("refY", 0).attr("markerWidth", 6).attr("markerHeight", 6).attr("orient", "auto").append("path").attr("d", "M0,-5L10,0L0,5");

  const root = svg
    .append("g")
    .attr("id", data.element_id || "graph-1")
    .attr("class", ["emit-root", ...graphClasses].join(" "));
  svg.call(
    d3
      .zoom()
      .scaleExtent([0.1, 8])
      .on("zoom", (event) => root.attr("transform", event.transform)),
  );

  const edgeGroups = root
    .append("g")
    .attr("class", "emit-edges")
    .selectAll("g")
    .data(links, (link) => link.element_id)
    .join("g")
    .attr("id", (link) => link.element_id)
    .attr("class", (link) => link.classes.join(" "))
    .attr("data-z", (link) => link.z);

  const edgeAnchors = edgeGroups
    .append("a")
    .attr("href", (link) => link.url || null)
    .attr("target", (link) => (link.url ? link.url_target : null))
    .attr("rel", (link) => (link.url && link.url_target === "_blank" ? "noopener noreferrer" : null));

  const edgeHit = edgeAnchors.append("line").attr("class", "edge-hit");
  const edgeLine = edgeAnchors
    .append("line")
    .attr("class", "edge-line")
    .attr("marker-end", data.directed ? "url(#emit-arrow)" : null);

  const edgeLabel = edgeAnchors
    .filter((link) => link.label !== null && link.label !== undefined)
    .append("text")
    .attr("class", "edge-label")
    .attr("text-anchor", "middle")
    .text((link) => link.label);

  const nodeGroups = root
    .append("g")
    .attr("class", "emit-nodes")
    .selectAll("g")
    .data(nodes, (node) => node.id)
    .join("g")
    .attr("id", (node) => node.element_id)
    .attr("class", (node) => node.classes.join(" "))
    .attr("data-node-id", (node) => node.id)
    .call(d3.drag().on("start", dragStarted).on("drag", dragged).on("end", dragEnded));

  nodeGroups.append("circle").attr("r", (node) => Math.max(9, node.font_size * 0.7));

  nodeGroups
    .append("text")
    .attr("font-size", (node) => node.font_size)
    .attr("text-anchor", "middle")
    .attr("dominant-baseline", "middle")
    .text((node) => node.label);

  function endpointId(endpoint) {
    return typeof endpoint === "object" ? endpoint.id : endpoint;
  }

  function showTip(event, text) {
    if (!tip) return;
    tip.style("display", "block").text(text);
    moveTip(event);
  }

  function moveTip(event) {
    if (!tip) return;
    tip.style("left", `${event.clientX + 12}px`).style("top", `${event.clientY + 12}px`);
  }

  function hideTip() {
    if (tip) tip.style("display", "none");
  }

  nodeGroups
    .on("mouseenter", (event, node) => showTip(event, `label: ${node.label}\nid: ${node.id}\ndf: ${node.df}\nidf: ${node.idf}` + `\nfq: ${node.fq ?? "NA"}\ndegree: ${node.degree}`))
    .on("mousemove", moveTip)
    .on("mouseleave", hideTip);

  edgeGroups
    .on("mouseenter", (event, link) => showTip(event, `source: ${endpointId(link.source)}\ntarget: ${endpointId(link.target)}` + `\nctf: ${link.ctf}\ncdf: ${link.cdf}\ncw: ${link.cw}\nz: ${link.z}` + `\nunit_ids: ${link.unit_ids.join(", ")}`))
    .on("mousemove", moveTip)
    .on("mouseleave", hideTip);

  function ticked() {
    edgeHit
      .attr("x1", (link) => link.source.x)
      .attr("y1", (link) => link.source.y)
      .attr("x2", (link) => link.target.x)
      .attr("y2", (link) => link.target.y);

    edgeLine
      .attr("x1", (link) => link.source.x)
      .attr("y1", (link) => link.source.y)
      .attr("x2", (link) => link.target.x)
      .attr("y2", (link) => link.target.y);

    edgeLabel.attr("x", (link) => (link.source.x + link.target.x) / 2).attr("y", (link) => (link.source.y + link.target.y) / 2);

    nodeGroups.attr("transform", (node) => `translate(${node.x},${node.y})`);
  }

  const simulation = d3
    .forceSimulation(nodes)
    .force(
      "link",
      d3
        .forceLink(links)
        .id((node) => node.id)
        .distance(data.link_distance || 30),
      //.distance(30),
    )
    .force("charge", d3.forceManyBody().strength(-80))
    .force("center", d3.forceCenter(width() / 2, height() / 2))
    .force(
      "collision",
      d3.forceCollide().radius((node) => Math.max(14, node.font_size * 0.9)),
    )
    .on("tick", ticked)
    .on("end", () => {
      for (const node of nodes) {
        node.fx = node.x;
        node.fy = node.y;
      }
      globalThis.dispatchEvent(
        new CustomEvent("emit-layout-ready", {
          detail: { data, nodes, links },
        }),
      );
    });

  function dragStarted(event, node) {
    node.fx = node.x;
    node.fy = node.y;
    if (!event.active) simulation.alphaTarget(0.12).restart();
  }

  function dragged(event, node) {
    node.fx = event.x;
    node.fy = event.y;
  }

  function releaseNodes() {
    nodes.forEach((node) => {
      node.fx = null;
      node.fy = null;
    });

    simulation.alpha(1).restart();
  }

  const reheatButton = document.getElementById("emit-reheat");

  if (reheatButton) {
    reheatButton.addEventListener("click", releaseNodes);
  }

  function dragEnded(event, node) {
    node.x = event.x;
    node.y = event.y;
    node.fx = event.x;
    node.fy = event.y;
    if (!event.active) simulation.alphaTarget(0);
    ticked();
  }

  globalThis.addEventListener("resize", updateViewBox);

  globalThis.emitGraph = Object.freeze({
    data,
    nodes,
    links,
    svg,
    root,
    edgeGroups,
    nodeGroups,
    simulation,
  });
})();
