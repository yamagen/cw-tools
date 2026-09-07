(function () {
  "use strict";

  const d3 = globalThis.d3;

  function endpointId(endpoint) {
    return typeof endpoint === "object" && endpoint !== null ? endpoint.id : endpoint;
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

  function seededRandom(seed) {
    let state = (Math.floor(Number(seed) || 1) >>> 0) || 1;
    return function random() {
      state ^= state << 13;
      state ^= state >>> 17;
      state ^= state << 5;
      return (state >>> 0) / 4294967296;
    };
  }

  function initializeRandom(component, nodes, diameter, seed) {
    const random = seededRandom(seed);
    const span = Math.max(20, diameter * 0.35);
    for (let i = 0; i < component.length; i++) {
      const node = nodes[component[i]];
      node.x = (random() - 0.5) * span;
      node.y = (random() - 0.5) * span;
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

  async function optimizeComponent(component, adjacency, nodes, options, targetDiameter, seedOffset) {
    if (component.length <= 1) return;

    const { distances, diameter } = shortestPaths(component, adjacency);
    const n = component.length;
    const baseLength = options.edge_length > 0
      ? options.edge_length
      : Math.max(8, targetDiameter / Math.max(1, diameter));
    const ideal = Array.from({ length: n }, () => new Float64Array(n));
    const springs = Array.from({ length: n }, () => new Float64Array(n));

    initializeRandom(component, nodes, targetDiameter, options.seed + seedOffset);

    for (let i = 0; i < n; i++) {
      for (let j = i + 1; j < n; j++) {
        const graphDistance = Math.max(1, distances[i][j]);
        ideal[i][j] = ideal[j][i] = baseLength * graphDistance;
        springs[i][j] = springs[j][i] = options.spring_strength / (graphDistance * graphDistance);
      }
    }

    const epsilon = options.epsilon > 0 ? options.epsilon : 0.0001 * n;
    const maxIterations = options.iterations > 0
      ? options.iterations
      : Math.min(100 * n, options.max_auto_iterations);

    for (let outer = 0; outer < maxIterations; outer++) {
      let maxNode = -1;
      let maxDelta = -Infinity;
      for (let m = 0; m < n; m++) {
        const value = derivatives(m, component, nodes, ideal, springs);
        if (value.delta > maxDelta) {
          maxDelta = value.delta;
          maxNode = m;
        }
      }
      if (maxNode < 0 || maxDelta <= epsilon) break;

      for (let inner = 0; inner < options.inner_iterations; inner++) {
        const value = derivatives(maxNode, component, nodes, ideal, springs);
        if (value.delta <= epsilon) break;
        const determinant = value.hxx * value.hyy - value.hxy * value.hxy;
        if (Math.abs(determinant) < 1e-12) break;
        const moveX = (value.hxy * value.gy - value.hyy * value.gx) / determinant;
        const moveY = (value.hxy * value.gx - value.hxx * value.gy) / determinant;
        if (!Number.isFinite(moveX) || !Number.isFinite(moveY)) break;
        const node = nodes[component[maxNode]];
        node.x += moveX;
        node.y += moveY;
      }

      if (options.yield_every > 0 && outer > 0 && outer % options.yield_every === 0) {
        await new Promise((resolve) => requestAnimationFrame(resolve));
      }
    }
  }

  function nodeBox(node, padding) {
    const fontSize = Math.max(1, Number(node.font_size) || 12);
    const label = node.label == null ? "" : String(node.label);
    const chars = Math.max(1, [...label].length);
    const width = Math.max(18, fontSize * chars * 0.95) + 2 * padding;
    const height = Math.max(18, fontSize * 1.35) + 2 * padding;
    return { width, height, halfWidth: width / 2, halfHeight: height / 2 };
  }

  function componentBounds(component, nodes) {
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

  function scaleAboutCentroid(component, nodes, factor) {
    if (!(factor > 1)) return;
    let cx = 0;
    let cy = 0;
    for (const index of component) {
      cx += nodes[index].x;
      cy += nodes[index].y;
    }
    cx /= component.length;
    cy /= component.length;
    for (const index of component) {
      const node = nodes[index];
      node.x = cx + (node.x - cx) * factor;
      node.y = cy + (node.y - cy) * factor;
    }
  }

  function rectanglesOverlap(a, boxA, b, boxB) {
    const dx = b.x - a.x;
    const dy = b.y - a.y;
    const overlapX = boxA.halfWidth + boxB.halfWidth - Math.abs(dx);
    const overlapY = boxA.halfHeight + boxB.halfHeight - Math.abs(dy);
    return { overlapX, overlapY, overlaps: overlapX > 0 && overlapY > 0, dx, dy };
  }

  function pushPair(a, boxA, b, boxB, amount = 1) {
    const hit = rectanglesOverlap(a, boxA, b, boxB);
    if (!hit.overlaps) return false;

    if (hit.overlapX < hit.overlapY) {
      const sign = hit.dx === 0 ? 1 : Math.sign(hit.dx);
      const shift = (hit.overlapX * 0.5 + 0.05) * amount;
      a.x -= sign * shift;
      b.x += sign * shift;
    } else {
      const sign = hit.dy === 0 ? 1 : Math.sign(hit.dy);
      const shift = (hit.overlapY * 0.5 + 0.05) * amount;
      a.y -= sign * shift;
      b.y += sign * shift;
    }
    return true;
  }

  function removeOverlapsPrismLike(component, nodes, options) {
    if (component.length <= 1 || options.overlap !== false) return;

    const boxes = component.map((index) => nodeBox(nodes[index], options.overlap_padding));
    const bounds = componentBounds(component, nodes);
    const averageSize = boxes.reduce((sum, box) => sum + Math.hypot(box.width, box.height), 0) / boxes.length;
    const span = Math.max(bounds.width, bounds.height, 1);
    const initialScale = 1 + Math.max(0, options.overlap_scaling) * averageSize / span;
    scaleAboutCentroid(component, nodes, Math.min(initialScale, 2.5));

    for (let pass = 0; pass < options.overlap_iterations; pass++) {
      let moved = false;
      const points = component.map((index) => [nodes[index].x, nodes[index].y]);

      if (d3 && d3.Delaunay && component.length >= 3) {
        const delaunay = d3.Delaunay.from(points);
        const seen = new Set();
        for (let i = 0; i < component.length; i++) {
          for (const j of delaunay.neighbors(i)) {
            const a = Math.min(i, j);
            const b = Math.max(i, j);
            const key = `${a}:${b}`;
            if (seen.has(key)) continue;
            seen.add(key);
            moved = pushPair(nodes[component[a]], boxes[a], nodes[component[b]], boxes[b], options.overlap_damping) || moved;
          }
        }
      } else {
        for (let a = 0; a < component.length; a++) {
          for (let b = a + 1; b < component.length; b++) {
            moved = pushPair(nodes[component[a]], boxes[a], nodes[component[b]], boxes[b], options.overlap_damping) || moved;
          }
        }
      }

      if (!moved) break;
    }

    // Final conservative cleanup for any residual collision that is not a
    // current Delaunay neighbor. This pass is intentionally weak so that the
    // KK geometry remains dominant.
    for (let pass = 0; pass < 8; pass++) {
      let moved = false;
      for (let a = 0; a < component.length; a++) {
        for (let b = a + 1; b < component.length; b++) {
          moved = pushPair(nodes[component[a]], boxes[a], nodes[component[b]], boxes[b], 0.35) || moved;
        }
      }
      if (!moved) break;
    }
  }

  function packComponents(components, nodes, width, height, padding) {
    const records = components
      .map((component) => ({ component, box: componentBounds(component, nodes) }))
      .sort((a, b) => b.box.width * b.box.height - a.box.width * a.box.height);

    const totalArea = records.reduce(
      (sum, record) => sum + (record.box.width + padding) * (record.box.height + padding),
      0,
    );
    const targetWidth = Math.max(width * 0.82, Math.sqrt(totalArea) * 1.2);
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

    const all = componentBounds(nodes.map((_, index) => index), nodes);
    const margin = Math.max(30, Math.min(width, height) * 0.05);
    const fit = Math.min(
      1,
      (width - 2 * margin) / Math.max(1, all.width),
      (height - 2 * margin) / Math.max(1, all.height),
    );
    const usedWidth = all.width * fit;
    const usedHeight = all.height * fit;
    const originX = (width - usedWidth) / 2;
    const originY = (height - usedHeight) / 2;

    for (const node of nodes) {
      node.x = originX + (node.x - all.minX) * fit;
      node.y = originY + (node.y - all.minY) * fit;
    }
  }

  function centerNodes(nodes, width, height) {
    if (nodes.length === 0) return;
    const all = componentBounds(nodes.map((_, index) => index), nodes);
    const dx = width / 2 - (all.minX + all.maxX) / 2;
    const dy = height / 2 - (all.minY + all.maxY) / 2;
    for (const node of nodes) {
      node.x += dx;
      node.y += dy;
      node.fx = node.x;
      node.fy = node.y;
    }
  }

  async function kamadaKawai(nodes, links, options = {}) {
    const width = Math.max(1, Number(options.width) || 960);
    const height = Math.max(1, Number(options.height) || 720);
    const resolved = {
      width,
      height,
      edge_length: Number(options.edge_length) > 0 ? Number(options.edge_length) : 0,
      spring_strength: Math.max(1e-6, Number(options.spring_strength) || 1),
      epsilon: Number(options.epsilon) > 0 ? Number(options.epsilon) : 0,
      iterations: Number(options.iterations) > 0 ? Math.floor(Number(options.iterations)) : 0,
      max_auto_iterations: Math.max(100, Math.floor(Number(options.max_auto_iterations) || 3000)),
      inner_iterations: Math.max(1, Math.floor(Number(options.inner_iterations) || 80)),
      component_padding: Math.max(0, Number(options.component_padding) || 60),
      yield_every: Math.max(0, Math.floor(Number(options.yield_every) || 8)),
      seed: Math.floor(Number(options.seed) || 1),
      overlap: options.overlap === undefined ? false : Boolean(options.overlap),
      overlap_padding: Math.max(0, Number(options.overlap_padding) || 4),
      overlap_iterations: Math.max(1, Math.floor(Number(options.overlap_iterations) || 120)),
      overlap_scaling: Math.max(0, Number(options.overlap_scaling) || 4),
      overlap_damping: Math.max(0.05, Math.min(1, Number(options.overlap_damping) || 0.75)),
    };

    const { components, adjacency } = connectedComponents(nodes, links);
    const largest = Math.max(1, ...components.map((component) => component.length));
    const canvasDiameter = Math.max(100, Math.min(width, height) * 0.72);

    for (let i = 0; i < components.length; i++) {
      const component = components[i];
      const fraction = Math.sqrt(component.length / largest);
      const targetDiameter = canvasDiameter * Math.max(0.22, fraction);
      await optimizeComponent(component, adjacency, nodes, resolved, targetDiameter, i * 997);
      removeOverlapsPrismLike(component, nodes, resolved);
    }

    packComponents(components, nodes, width, height, resolved.component_padding);

    // Packing may scale the coordinates down while labels keep their screen
    // size. Run the proximity-based overlap pass once more in the final
    // coordinate system, then only translate the result; do not scale it again.
    for (const component of components) {
      removeOverlapsPrismLike(component, nodes, {
        ...resolved,
        overlap_scaling: 0,
        overlap_iterations: Math.min(60, resolved.overlap_iterations),
      });
    }
    centerNodes(nodes, width, height);

    return {
      type: "kamada-kawai",
      model: "shortpath",
      start: "deterministic-random",
      overlap: resolved.overlap === false ? "prism-like" : "true",
      components: components.length,
      options: resolved,
    };
  }

  globalThis.emitLayouts = globalThis.emitLayouts || {};
  globalThis.emitLayouts.kamadaKawai = kamadaKawai;
})();
