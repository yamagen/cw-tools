<?php
declare(strict_types=1);

header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store');

function fail_request(int $status, string $message): never
{
    http_response_code($status);
    echo json_encode(['error' => $message], JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
    exit;
}

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    fail_request(405, 'POST required');
}

$raw = file_get_contents('php://input');
$request = json_decode($raw === false ? '' : $raw, true);
if (!is_array($request)) {
    fail_request(400, 'invalid JSON request');
}

$root = getenv('CW_TOOLS_ROOT') ?: dirname(__DIR__);
$corpus = getenv('CW_SUITE_CORPUS') ?: $root . '/tests/data/hachidaishu-bg-split.txt';
$idf = getenv('CW_SUITE_IDF') ?: $root . '/tests/data/hachidaishu-bg-16-split.idf';
$emitConfig = getenv('CW_SUITE_EMIT_CONFIG') ?: $root . '/config/emit-config.json';

$pair = $root . '/pair';
$cw = $root . '/cw';
$emit = $root . '/emit';

foreach ([$pair, $cw, $emit] as $binary) {
    if (!is_file($binary) || !is_executable($binary)) {
        fail_request(500, 'cw-tools binaries are not available');
    }
}
foreach ([$corpus, $idf, $emitConfig] as $path) {
    if (!is_file($path) || !is_readable($path)) {
        fail_request(500, 'cw-suite data files are not available');
    }
}

$mode = (string)($request['mode'] ?? 'free');
if (!in_array($mode, ['free', 'exact', 'all'], true)) {
    fail_request(400, 'mode must be free, exact, or all');
}

$key = trim((string)($request['key'] ?? ''));
if ($mode !== 'all' && $key === '') {
    fail_request(400, 'key is required');
}
if (strlen($key) > 512) {
    fail_request(400, 'key is too long');
}

$p = filter_var($request['p'] ?? 5, FILTER_VALIDATE_INT);
$substr = filter_var($request['substr'] ?? 16, FILTER_VALIDATE_INT);
$max = filter_var($request['max'] ?? 16, FILTER_VALIDATE_INT);
if ($p === false || $p < 1 || $p > 32) {
    fail_request(400, 'p must be between 1 and 32');
}
if ($substr === false || $substr < 0 || $substr > 128) {
    fail_request(400, 'substr must be between 0 and 128');
}
if ($max === false || $max < 1 || $max > 128) {
    fail_request(400, 'max must be between 1 and 128');
}

$cwArgs = [
    escapeshellarg($cw),
    '-p', (string)$p,
    '--substr', (string)$substr,
    '-M', (string)$max,
];

if ($mode === 'free') {
    $cwArgs[] = '--free-key';
    $cwArgs[] = escapeshellarg($key);
} elseif ($mode === 'exact') {
    $cwArgs[] = '--exact-key';
    $cwArgs[] = escapeshellarg($key);
}

$cwArgs[] = '--idf-in';
$cwArgs[] = escapeshellarg($idf);

$command =
    'grep ' . escapeshellarg('^1') . ' ' . escapeshellarg($corpus) .
    ' | ' . escapeshellarg($pair) .
    ' | ' . implode(' ', $cwArgs) .
    ' | ' . escapeshellarg($emit) . ' -T js -c ' . escapeshellarg($emitConfig);

$descriptorSpec = [
    0 => ['pipe', 'r'],
    1 => ['pipe', 'w'],
    2 => ['pipe', 'w'],
];

$process = proc_open(['/bin/sh', '-c', $command], $descriptorSpec, $pipes, $root);
if (!is_resource($process)) {
    fail_request(500, 'could not start cw-tools pipeline');
}

fclose($pipes[0]);
$stdout = stream_get_contents($pipes[1]);
$stderr = stream_get_contents($pipes[2]);
fclose($pipes[1]);
fclose($pipes[2]);
$status = proc_close($process);

if ($status !== 0 || $stdout === false) {
    $message = trim($stderr === false ? '' : $stderr);
    fail_request(500, $message !== '' ? $message : 'cw-tools pipeline failed');
}

if (!preg_match('/\A"use strict";\s*globalThis\.emitData=(.*);\s*\z/s', $stdout, $matches)) {
    fail_request(500, 'unexpected emit -T js output');
}

$graph = json_decode($matches[1], true);
if (!is_array($graph) || !isset($graph['nodes'], $graph['links'])) {
    fail_request(500, 'emit returned invalid graph data');
}

echo json_encode($graph, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
