<?php
declare(strict_types=1);

$mode = $_GET['mode'] ?? 'free';
if (!in_array($mode, ['free', 'exact', 'all'], true)) {
    $mode = 'free';
}
$key = $_GET['key'] ?? '桜';
$p = (int)($_GET['p'] ?? 5);
$substr = (int)($_GET['substr'] ?? 16);
$max = (int)($_GET['max'] ?? 16);

$params = [
    'mode' => $mode,
    'key' => $key,
    'p' => $p,
    'substr' => $substr,
    'max' => $max,
];
?>
<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>cw-tools dynamic graph</title>
    <link rel="stylesheet" href="../assets/emit-d3.css" />
    <link rel="stylesheet" href="../assets/emit-weight.css" />
    <link rel="stylesheet" href="../assets/emit-retention.css" />
    <style>
      #emit-query-panel {
        position: absolute;
        top: 0;
        left: 0.75rem;
        z-index: 12;
        box-sizing: border-box;
        width: min(21rem, calc(100vw - 1.5rem));
        padding: 0.7rem 0.8rem;
        border: 1px solid rgba(0, 0, 0, 0.18);
        border-radius: 0.55rem;
        background: rgba(255, 255, 255, 0.95);
        box-shadow: 0 2px 10px rgba(0, 0, 0, 0.16);
      }
      #emit-query-form {
        display: grid;
        grid-template-columns: 5rem 1fr;
        gap: 0.45rem 0.6rem;
        align-items: center;
        font-size: 0.78rem;
      }
      #emit-query-form input,
      #emit-query-form select,
      #emit-query-form button {
        box-sizing: border-box;
        width: 100%;
        font: inherit;
      }
      #emit-query-numbers {
        display: grid;
        grid-template-columns: repeat(3, 1fr);
        gap: 0.4rem;
      }
      #emit-query-status {
        grid-column: 1 / -1;
        color: #555;
        min-height: 1.2em;
      }
      @media (max-width: 46rem) {
        #emit-query-panel {
          left: 0.4rem;
          width: calc(100vw - 0.8rem);
        }
      }
    </style>
  </head>
  <body>
    <section id="emit-controls" aria-label="Z alpha beta observation controls">
      <div id="emit-control-summary">
        <div>
          <output id="emit-z-value" for="emit-z-slider">Z threshold</output>
          <div id="emit-weight-values">
            <output id="emit-alpha-value" for="emit-alpha-slider">α(C) 1.00</output>
            <output id="emit-beta-value" for="emit-beta-slider">β(P) 1.00</output>
            <output id="emit-weight-state">PCG · M16</output>
          </div>
        </div>
        <span id="emit-z-count" aria-live="polite"></span>
      </div>
      <svg id="emit-z-distribution"></svg>
      <div id="emit-slider-row">
        <span id="emit-z-min"></span>
        <input id="emit-z-slider" type="range" aria-label="minimum Z value" />
        <span id="emit-z-max"></span>
        <div class="emit-weight-row">
          <label for="emit-alpha-slider">α · C</label>
          <input id="emit-alpha-slider" type="range" aria-label="C contribution alpha" />
        </div>
        <div class="emit-weight-row">
          <label for="emit-beta-slider">β · P</label>
          <input id="emit-beta-slider" type="range" aria-label="P contribution beta" />
        </div>
        <div id="emit-button-row">
          <button id="emit-reheat">Reheat</button>
          <button id="emit-z-reset">Reset Z</button>
          <button id="emit-command-toggle">Request</button>
          <button id="emit-retention-toggle" type="button" aria-pressed="false">Retention</button>
        </div>
      </div>
      <pre id="emit-command-panel" hidden></pre>
    </section>

    <aside id="emit-retention-panel" hidden aria-label="Retention curves">
      <div id="emit-retention-header">
        <strong>Retention</strong>
        <output id="emit-retention-value"></output>
      </div>
      <svg id="emit-retention-chart"></svg>
    </aside>

    <aside id="emit-source-panel" hidden aria-label="Source texts">
      <header id="emit-source-header">
        <strong id="emit-source-title">Source texts</strong>
        <button id="emit-source-close" type="button">Close</button>
      </header>
      <div id="emit-source-content"></div>
    </aside>

    <aside id="emit-query-panel" aria-label="cw calculation parameters">
      <form id="emit-query-form" method="get">
        <label for="emit-query-mode">Key mode</label>
        <select id="emit-query-mode" name="mode">
          <option value="free"<?= $mode === 'free' ? ' selected' : '' ?>>free</option>
          <option value="exact"<?= $mode === 'exact' ? ' selected' : '' ?>>exact</option>
          <option value="all"<?= $mode === 'all' ? ' selected' : '' ?>>all</option>
        </select>

        <label for="emit-query-key">Key</label>
        <input id="emit-query-key" name="key" value="<?= htmlspecialchars((string)$key, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8') ?>" />

        <span>p / substr / M</span>
        <div id="emit-query-numbers">
          <input name="p" type="number" min="1" max="32" value="<?= $p ?>" aria-label="p" />
          <input name="substr" type="number" min="0" max="128" value="<?= $substr ?>" aria-label="substr" />
          <input name="max" type="number" min="1" max="128" value="<?= $max ?>" aria-label="M" />
        </div>

        <span></span>
        <button type="submit">Run</button>
        <div id="emit-query-status" aria-live="polite">Loading…</div>
      </form>
    </aside>

    <svg id="emit-graph" aria-label="interactive network graph"></svg>
    <div id="emit-tip"></div>

    <script src="https://cdn.jsdelivr.net/npm/d3@7.9.0/dist/d3.min.js"></script>
    <script>
      "use strict";

      const request = <?= json_encode($params, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES) ?>;
      const statusElement = document.getElementById("emit-query-status");
      const commandPanel = document.getElementById("emit-command-panel");
      const commandButton = document.getElementById("emit-command-toggle");
      const controlsPanel = document.getElementById("emit-controls");
      const queryPanel = document.getElementById("emit-query-panel");

      function positionQueryPanel() {
        const controlsRect = controlsPanel.getBoundingClientRect();
        const gap = window.matchMedia("(max-width: 46rem)").matches ? 6.4 : 12;
        queryPanel.style.top = `${controlsRect.bottom + gap}px`;
      }

      positionQueryPanel();
      window.addEventListener("resize", positionQueryPanel);
      if ("ResizeObserver" in window) {
        new ResizeObserver(positionQueryPanel).observe(controlsPanel);
      }

      commandPanel.textContent = JSON.stringify(request, null, 2);
      commandButton.addEventListener("click", () => {
        commandPanel.hidden = !commandPanel.hidden;
        positionQueryPanel();
      });

      function loadScript(src) {
        return new Promise((resolve, reject) => {
          const script = document.createElement("script");
          script.src = src;
          script.onload = resolve;
          script.onerror = () => reject(new Error(`could not load ${src}`));
          document.body.appendChild(script);
        });
      }

      async function start() {
        try {
          const response = await fetch("cw-suite.php", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(request),
            cache: "no-store",
          });
          const graph = await response.json();
          if (!response.ok) {
            throw new Error(graph.error || `HTTP ${response.status}`);
          }

          globalThis.emitData = graph;
          statusElement.textContent = `${graph.nodes.length} nodes · ${graph.links.length} edges`;

          await loadScript("../assets/emit-d3.js");
          await loadScript("../assets/emit-interaction.js");
          await loadScript("../assets/emit-slider.js");
          await loadScript("../assets/emit-weight.js");
          await loadScript("../assets/emit-retention.js");
          positionQueryPanel();
        } catch (error) {
          statusElement.textContent = `Error: ${error.message}`;
        }
      }

      start();
    </script>
  </body>
</html>
