# Edge URLs from unit IDs

`emit` can attach a URL to each DOT edge by combining a configured base URL
with the `unit_id` values already carried by the corresponding `cw` row.
Graphviz preserves the URL when it renders SVG, making both the edge line and
its label clickable in a browser.

## Configuration

The settings belong in the existing `edge` object:

```json
"edge": {
  "url_base": "wakalist.html",
  "url_parameter": "unit_id",
  "url_target": "_blank"
}
```

The default configuration contains:

```json
"url_base": null,
"url_parameter": "unit_id",
"url_target": "_blank"
```

`url_base: null` disables links and preserves the previous DOT and SVG output.

| Setting | Meaning |
| --- | --- |
| `url_base` | destination before the query string; `null` or an empty string disables links |
| `url_parameter` | GET parameter repeated for every trailing unit ID |
| `url_target` | Graphviz link target; `_blank` opens another tab or window; `null` or an empty string omits the target |

When `url_base` is enabled, `url_parameter` must be a nonempty string.

## Generated URL

For an edge with:

```text
unit_ids: 10032 10034 10035
```

and this configuration:

```json
"url_base": "wakalist.html",
"url_parameter": "unit_id"
```

`emit` writes conceptually:

```dot
URL="wakalist.html?unit_id=10032&unit_id=10034&unit_id=10035"
target="_blank"
```

If `url_base` already contains a query string, `emit` continues it with `&`:

```json
"url_base": "wakalist.html?view=edge"
```

produces:

```text
wakalist.html?view=edge&unit_id=10032&unit_id=10034&unit_id=10035
```

Unit IDs retain their input order and repetitions. Each value is percent
encoded as a UTF-8 query component. For example, a space becomes `%20`, `/`
becomes `%2F`, and Japanese UTF-8 bytes are encoded with `%HH` sequences.

## DOT and SVG structure

An edge with a generated URL receives the semantic class:

```text
has-url
```

Example DOT:

```dot
"A" -- "B" [
  id="edge-1",
  class="z-rank-6 z-positive has-url",
  URL="wakalist.html?unit_id=10032&unit_id=10034",
  target="_blank"
];
```

Graphviz SVG contains an anchor around the edge path and another anchor around
the edge label. The serialized XML escapes query separators as `&amp;`; the
browser resolves them to ordinary `&` characters when following the URL.

```xml
<a xlink:href="wakalist.html?unit_id=10032&amp;unit_id=10034"
   target="_blank">
```

## Local verification

Update and build:

```sh
git pull --ff-only
make clean
make emit
```

Set `edge.url_base` in the configuration, then preserve the intermediate DOT
for inspection:

```sh
... | ./emit -c config/emit-config.json -Tdot -Z 1.6 > t.dot

grep 'URL=' t.dot | head
neato -Gstylesheet=assets/svg.css -Tsvg t.dot -o t.svg
firefox t.svg
```

Click either an edge line or its numeric label. Firefox is the preferred viewer
for checking links and the external semantic stylesheet. `inkview` remains
useful for checking Graphviz geometry, but viewer support for external CSS and
interactive links differs.

## Implementation boundary

`src/emit-url.c` owns URL-setting extraction, UTF-8 percent encoding, and query
construction. `src/emit-dot.c` adds the resulting `URL`, `target`, and `has-url`
attributes to the edge. No measurement is recalculated: `unit_id` values remain
those supplied by `cw`.
