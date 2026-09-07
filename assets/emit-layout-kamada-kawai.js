(function () {
  "use strict";

  function endpointId(endpoint) {
    return typeof endpoint === "object" ? endpoint.id : endpoint;
  }

  function connectedComponents(nodes, links) {
    const indexById = new Map(nodes.map((node, index) => [node.id, index]));
    const adjacency = nodes.map(() => []);

    for (const link of links) {
      const s = indexById.get(endpointId(link.source));
      const t = indexById.get(endpointId(link.target));
      if (s === undefined || t === undefined || s === t) continue;
      adjacency[s].push(t);
      adjacency[t].push(s);
    }

    const seen = new Uint8Array(nodes.length);
    const components = [];

    for (let start = 0; start < nodes.length; start++) {
      if (seen[start]) continue;
      const queue = [start];
      const component = [];
      seen[start] = 1;

      for (let q = 0; q < queue.length; q++) {
        const v = queue[q];
        component.push(v);
        for (const next of adjacency[v]) {
          if (seen[next]) continue;
          seen[next] = 1;
          queue.push(next);
        }
      }
      components.push(component);
    }

    return { components, adjacency };
  }

  function shortestPaths(component, adjacency) {
    const n = component.length;
    const localIndex = new Map(component.map((globalIndex, local) => [globalIndex, local]));
    const distances = Array.from({ length: n }, () => new Float64Array(n));
    let diameter = 1;

    for (let source = 0; source < n; source++) {
      const row = distances[source];
      row.fill(Infinity);
      row[source] = 0;
      const queue = [component[source]];

      for (let q = 0; q < queue.length; q++) {
        const global = queue[q];
        const local = localIndex.get(global);
        const base = row[local];
        for (const nextGlobal of adjacency[global]) {
          const nextLocal = localIndex.get(nextGlobal);
          if (nextLocal === undefined || Number.isFinite(row[nextLocal])) continue;
          row[nextLocal] = base + 1;
          diameter = Math.max(diameter, row[nextLocal]);
          queue.push(nextGlobal);
        }
      }
    }

    return { distances, diameter };
  }

  function initializeCircle(component, nodes, edgeLength) {
    const n = component.length;
    if (n === 1) {
      nodes[component[0]].x = 0;
      nodes[component[0]].y = 0;
      return;
    }
    const radius = Math.max(edgeLength, (edgeLength * n) / (2 * Math.PI));
    for (let i = 0; i < n; i++) {
      const angle = (2 * Math.PI * i) / n;
      nodes[component[i]].x = radius * Math.cos(angle);
      nodes[component[i]].y = radius * Math.sin(angle);
    }
  }

  function derivatives(m, component, nodes, ideal, springs) {
    const nodeM = nodes[component[m]];
    let gx = 0;
    let gy = 0;
    let hxx = 0;
    let hyy = 0;
    let hxy = 0;

    for (let i = 0; i < component.length; i++) {
      if (i === m) continue;
      const nodeI = nodes[component[i]];
      const dx = nodeM.x - nodeI.x;
      const dy = nodeM.y - nodeI.y;
      const distance = Math.max(1e-9, Math.hypot(dx, dy));
      const distance3 = distance * distance * distance;
      const k = springs[m][i];
      const l = ideal[m][i];

      gx += k * (dx - (l * dx) / distance);
      gy += k * (dy - (l * dy) / distance);
      hxx += k * (1 - (l * dy * dy) / distance3);
      hyy += k * (1 - (l * dx * dx) / distance3);
      hxy += k * ((l * dx * dy) / distance3);
    }

    return { gx, gy, hxx, hyy, hxy, delta: Math.hypot(gx, gy) };
  }

  async function optimizeComponent(component, adjacency, nodes, options) {
    if (component.length <= 1) return;

    const edgeLength = options.edge_length;
    const springStrength = options.spring_strength;
    const epsilon = options.epsilon;
    const maxIterations = options.iterations;
    const innerIterations = options.inner_iterations;
    const yieldEvery = options.yield_every;

    initializeCircle(component, nodes, edgeLength);
    const { distances } = shortestPaths(component, adjacency);
    const n = component.length;
    const ideal = Array.from({ length: n }, () => new Float64Array(n));
    const springs = Array.from({ length: n }, () => new Float64Array(n));

    for (let i = 0; i < n; i++) {
      for (let j = i + 1; j < n; j++) {
        const d = Math.max(1, distances[i][j]);
        const l = edgeLength * d;
        const k = springStrength / (d * d);
        ideal[i][j] = ideal[j][i] = l;
        springs[i][j] = springs[j][i] = k;
      }
    }

    for (let outer = 0; outer < maxIterations; outer++) {
      let maxNode = -1;
      let maxDelta = -Infinity;

      for (let m = 0; m < n; m++) {
        const d = derivatives(m, component, nodes, ideal, springs);
        if (d.delta > maxDelta) {
          maxDelta = d.delta;
          maxNode = m;
        }
      }

      if (maxNode < 0 || maxDelta <= epsilon) break;

      for (let inner = 0; inner < innerIterations; inner++) {
        const d = derivatives(maxNode, component, nodes, ideal, springs);
        if (d.delta <= epsilon) break;
        const determinant = d.hxx * d.hyy - d.hxy * d.hxy;
        if (Math.abs(determinant) < 1e-12) break;

        const moveX = (d.hxy * d.gy - d.hyy * d.gx) / determinant;
        const moveY = (d.hxy * d.gx - d.hxx * d.gy) / determinant;
        if (!Number.isFinite(moveX) || !Number.isFinite(moveY)) break;

        const node = nodes[component[maxNode]];
        node.x += moveX;
        node.y += moveY;
      }

      if (yieldEvery > 0 && outer > 0 && outer % yieldEvery === 0) {
        await new Promise((resolve) => requestAnimationFrame(resolve));
      }
    }
  }

  function componentBox(component, nodes) {
    let minX = Infinity;
    let minY = Infinity;
    let maxX = -Infinity;
    let maxY = -Infinity;

    for (const index of component) {
      const node = nodes[index];
      minX = Math.min(minX, node.x);
      minY = Math.min(minY, node.y);
      maxX = Math.max(maxX, node.x);
      maxY = Math.max(maxY, node.y);
    }

    return {
      minX,
      minY,
      maxX,
      maxY,
      width: Math.max(1, maxX - minX),
      height: Math.max(1, maxY - minY),
    };
  }

  function packComponents(components, nodes, width, height, padding) {
    const records = components
      .map((component) => ({ component, box: componentBox(component, nodes) }))
      .sort((a, b) => b.box.width * b.box.height - a.box.width * a.box.height);

    const totalArea = records.reduce(
      (sum, record) => sum + (record.box.width + padding) * (record.box.height + padding),
      0,
    );
    const targetWidth = Math.max(width * 0.8, Math.sqrt(totalArea) * 1.25);
    let cursorX = 0;
    let cursorY = 0;
    let rowHeight = 0;

    for (const record of records) {
      const w = record.box.width + padding;
      const h = record.box.height + padding;
      if (cursorX > 0 && cursorX + w > targetWidth) {
        cursorX = 0;
        cursorY += rowHeight;
        rowHeight = 0;
      }

      const offsetX = cursorX - record.box.minX + padding / 2;
      const offsetY = cursorY - record.box.minY + padding / 2;
      for (const index of record.component) {
        nodes[index].x += offsetX;
        nodes[index].y += offsetY;
      }
      cursorX += w;
      rowHeight = Math.max(rowHeight, h);
    }

    let minX = Infinity;
    let minY = Infinity;
    let maxX = -Infinity;
    let maxY = -Infinity;
    for (const node of nodes) {
      minX = Math.min(minX, node.x);
      minY = Math.min(minY, node.y);
      maxX = Math.max(maxX, node.x);
      maxY = Math.max(maxY, node.y);
    }

    const spanX = Math.max(1, maxX - minX);
    const spanY = Math.max(1, maxY - minY);
    const margin = Math.max(30, Math.min(width, height) * 0.06);
    const scale = Math.min(
      1,
      Math.max(0.05, (width - 2 * margin) / spanX),
      Math.max(0.05, (height - 2 * margin) / spanY),
    );
    const usedWidth = spanX * scale;
    const usedHeight = spanY * scale;
    const originX = (width - usedWidth) / 2;
    const originY = (height - usedHeight) / 2;

    for (const node of nodes) {
      node.x = originX + (node.x - minX) * scale;
      node.y = originY + (node.y - minY) * scale;
      node.fx = node.x;
      node.fy = node.y;
    }
  }

  async function kamadaKawai(nodes, links, options = {}) {
    const resolved = {
      width: Math.max(1, Number(options.width) || 960),
      height: Math.max(1, Number(options.height) || 720),
      edge_length: Math.max(5, Number(options.edge_length) || 48),
      spring_strength: Math.max(1e-6, Number(options.spring_strength) || 1),
      epsilon: Math.max(1e-6, Number(options.epsilon) || 0.01),
      iterations: Math.max(1, Math.floor(Number(options.iterations) || 300)),
      inner_iterations: Math.max(1, Math.floor(Number(options.inner_iterations) || 80)),
      component_padding: Math.max(0, Number(options.component_padding) || 60),
      yield_every: Math.max(0, Math.floor(Number(options.yield_every) || 8)),
    };

    const { components, adjacency } = connectedComponents(nodes, links);
    for (const component of components) {
      await optimizeComponent(component, adjacency, nodes, resolved);
    }
    packComponents(
      components,
      nodes,
      resolved.width,
      resolved.height,
      resolved.component_padding,
    );

    return {
      type: "kamada-kawai",
      components: components.length,
      options: resolved,
    };
  }

  globalThis.emitLayouts = globalThis.emitLayouts || {};
  globalThis.emitLayouts.kamadaKawai = kamadaKawai;
})();
