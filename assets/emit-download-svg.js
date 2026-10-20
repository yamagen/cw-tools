(function () {
  "use strict";

  const button = document.getElementById("emit-download-svg");
  const svg = document.getElementById("emit-graph");
  if (!button || !svg) return;

  const embeddedStyle = `
    .edge .edge-line { stroke: #000; stroke-width: 0.1; }
    .edge .edge-hit { display: none; }
    .edge .edge-label { display: none; font-size: 6px; }

    #emit-arrow path { fill: #777; }
    .node.font-rank-0 text { opacity: 0.55; font-weight: 400; }
    .node.font-rank-1 text { opacity: 0.6; font-weight: 400; }
    .node.font-rank-2 text { opacity: 0.65; font-weight: 400; }
    .node.font-rank-3 text { opacity: 0.7; font-weight: 400; }
    .node.font-rank-4 text { opacity: 0.75; font-weight: 400; }
    .node.font-rank-5 text { opacity: 0.8; font-weight: 500; }
    .node.font-rank-6 text { opacity: 0.85; font-weight: 500; }
    .node.font-rank-7 text { opacity: 0.9; font-weight: 600; }
    .node.font-rank-8 text { opacity: 0.95; font-weight: 700; }
    .node.font-rank-9 text { opacity: 1; font-weight: 700; }
    .node circle { fill: #fff; stroke: #000; stroke-width: 0.1; }
    .edge.z-negative .edge-line { stroke-dasharray: 5 3; }
    .edge.z-zero .edge-line { stroke-dasharray: 1 3; }
    .is-hidden { display: none; }
  `;

  function safeFilePart(value) {
    return String(value || "graph")
      .trim()
      .replace(/[\\/:*?"<>|]+/g, "-")
      .replace(/\s+/g, "_") || "graph";
  }

  button.addEventListener("click", () => {
    const clone = svg.cloneNode(true);
    const width = svg.clientWidth || 960;
    const height = svg.clientHeight || 720;

    clone.setAttribute("xmlns", "http://www.w3.org/2000/svg");
    clone.setAttribute("width", String(width));
    clone.setAttribute("height", String(height));
    if (!clone.getAttribute("viewBox")) {
      clone.setAttribute("viewBox", `0 0 ${width} ${height}`);
    }

    const defs = clone.querySelector("defs");
    const style = document.createElementNS("http://www.w3.org/2000/svg", "style");
    style.textContent = embeddedStyle;
    if (defs) {
      defs.prepend(style);
    } else {
      clone.prepend(style);
    }

    const source = '<?xml version="1.0" encoding="UTF-8"?>\n' +
      new XMLSerializer().serializeToString(clone);
    const blob = new Blob([source], { type: "image/svg+xml;charset=utf-8" });
    const url = URL.createObjectURL(blob);
    const anchor = document.createElement("a");
    const key = document.getElementById("emit-query-key")?.value || "graph";

    anchor.href = url;
    anchor.download = `cw-${safeFilePart(key)}.svg`;
    document.body.appendChild(anchor);
    anchor.click();
    anchor.remove();
    setTimeout(() => URL.revokeObjectURL(url), 0);
  });
})();
