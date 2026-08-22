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
  const sourcePanel = document.getElementById("emit-source-panel");
  const sourceTitle = document.getElementById("emit-source-title");
  const sourceContent = document.getElementById("emit-source-content");
  const sourceClose = document.getElementById("emit-source-close");
  const svg = d3.select(svgElement);
  const tip = tipElement ? d3.select(tipElement) : null;
  const nodes = data.nodes.map((node) => ({ ...node }));
  const links = data.links.map((link) => ({ ...link }));

  let sourceTexts = null;
  let sourceTextsError = null;
  const sourceTextsPromise = fetch("emit-texts.json")
    .then((response) => {
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      return response.json();
    })
    .then((texts) => {
      sourceTexts = texts;
      return texts;
    })
    .catch((error) => {
      sourceTextsError = error;
      return null;
    });

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

  function clearSourceContent() {
    if (!sourceContent) return;
    while (sourceContent.firstChild) sourceContent.removeChild(sourceContent.firstChild);
  }

  function appendSourceRecord(unitId, record) {
    const article = document.createElement("article");
    article.className = "emit-source-record";

    const heading = document.createElement("div");
    heading.className = "emit-source-unit-id";
    heading.textContent = unitId;
    article.appendChild(heading);

    if (record && typeof record === "object") {
      for (const [name, value] of Object.entries(record)) {
        const field = document.createElement("div");
        field.className = `emit-source-field emit-source-field-${name}`;

        if (name !== "surface") {
          const label = document.createElement("span");
          label.className = "emit-source-field-name";
          label.textContent = `${name}: `;
          field.appendChild(label);
        }

        const text = document.createElement("span");
        text.className = "emit-source-field-value";
        text.textContent = value == null ? "" : String(value);
        field.appendChild(text);
        article.appendChild(field);
      }
    } else {
      const missing = document.createElement("div");
      missing.className = "emit-source-missing";
      missing.textContent = "Source text unavailable";
      article.appendChild(missing);
    }

    sourceContent.appendChild(article);
  }

  async function showSources(unitIds, title = "Source texts") {
    if (!sourcePanel || !sourceContent) return;

    const ids = [...new Set(Array.isArray(unitIds) ? unitIds : [])];
    clearSourceContent();
    sourcePanel.hidden = false;
    if (sourceTitle) sourceTitle.textContent = `${title} (${ids.length})`;

    if (!sourceTexts && !sourceTextsError) await sourceTextsPromise;

    if (sourceTextsError) {
      const error = document.createElement("div");
      error.className = "emit-source-error";
      error.textContent = `emit-texts.json could not be loaded: ${sourceTextsError.message}`;
      sourceContent.appendChild(error);
      return;
    }

    for (const unitId of ids) appendSourceRecord(unitId, sourceTexts?.[unitId]);
  }

  function visibleUnitIdsForNode(node) {
    const ids = new Set();

    for (const link of links) {
      const sourceId = endpointId(link.source);
      const targetId = endpointId(link.target);
      if (sourceId !== node.id && targetId !== node.id) continue;

      const element = document.getElementById(link.element_id);
      if (element && element.classList.contains("is-hidden")) continue;

      if (Array.isArray(link.unit_ids)) {
        for (const unitId of link.unit_ids) ids.add(unitId);
      }
    }

    return [...ids];
  }

  if (sourceClose && sourcePanel) {
    sourceClose.addEventListener("click", () => {
      sourcePanel.hidden = true;
    });
  }

  nodeGroups
    .on("mouseenter", (event, node) => showTip(event, `label: ${node.label}\nid: ${node.id}\ndf: ${node.df}\nidf: ${node.idf}` + `\nfq: ${node.fq ?? "NA"}\ndegree: ${node.degree}`))
    .on("mousemove", moveTip)
    .on("mouseleave", hideTip)
    .on("click", (event, node) => {
      const unitIds = visibleUnitIdsForNode(node);
      if (unitIds.length === 0) return;
      event.preventDefault();
      event.stopPropagation();
      hideTip();
      showSources(unitIds, `Source texts — ${node.label}`);
    });

  edgeGroups
    .on("mouseenter", (event, link) => showTip(event, `source: ${endpointId(link.source)}\ntarget: ${endpointId(link.target)}` + `\nctf: ${link.ctf}\ncdf: ${link.cdf}\ncw: ${link.cw}\nz: ${link.z}` + `\nunit_ids: ${link.unit_ids.join(", ")}`))
    .on("mousemove", moveTip)
    .on("mouseleave", hideTip)
    .on("click", (event, link) => {
      if (!Array.isArray(link.unit_ids) || link.unit_ids.length === 0) return;
      event.preventDefault();
      event.stopPropagation();
      hideTip();
      showSources(link.unit_ids);
    });

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
        .distance((link) => {
          const s = Number(link.source.degree ?? 0);
          const t = Number(link.target.degree ?? 0);
          const maxDegree = Math.max(s, t);

          if (maxDegree >= 12) return 50;
          if (maxDegree >= 3) return 30;
          return 10;
        }),
    )
    .force("charge", d3.forceManyBody().strength(-50))
    .force("x", d3.forceX(width() / 1.4).strength(0.02))
    .force("y", d3.forceY(height() / 2).strength(0.04))
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
  if (reheatButton) reheatButton.addEventListener("click", releaseNodes);

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
    showSources,
    visibleUnitIdsForNode,
  });
})();
