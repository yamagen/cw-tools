#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "emit-d3.h"
#include "emit-util.h"

static void js_write_string(FILE *stream, const char *text)
{
    static const char hex[] = "0123456789abcdef";

    fputc('"', stream);
    for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; p++) {
        unsigned char c = *p;
        switch (c) {
        case '"': fputs("\\\"", stream); break;
        case '\\': fputs("\\\\", stream); break;
        case '\b': fputs("\\b", stream); break;
        case '\f': fputs("\\f", stream); break;
        case '\n': fputs("\\n", stream); break;
        case '\r': fputs("\\r", stream); break;
        case '\t': fputs("\\t", stream); break;
        case '<': fputs("\\u003c", stream); break;
        default:
            if (c < 0x20) {
                fputs("\\u00", stream);
                fputc(hex[c >> 4], stream);
                fputc(hex[c & 0x0f], stream);
            } else {
                fputc(c, stream);
            }
            break;
        }
    }
    fputc('"', stream);
}

static void write_graph_data(FILE *stream, const EdgeVec *edges,
                             const NodeVec *nodes, const Config *config)
{
    EmitNodeWeightRange range = emit_node_weight_range(nodes, config);

    fputs("{\"directed\":", stream);
    fputs(config->directed ? "true" : "false", stream);
    fputs(",\"nodes\":[", stream);

    for (size_t i = 0; i < nodes->len; i++) {
        const NodeRef *node = &nodes->items[i];
        char *label = emit_make_node_label(node->id, config);

        if (i > 0)
            fputc(',', stream);
        fputs("{\"id\":", stream);
        js_write_string(stream, node->id);
        fputs(",\"label\":", stream);
        js_write_string(stream, label);
        fprintf(stream, ",\"df\":%zu,\"idf\":%.17g,\"fq\":",
                node->df, node->idf);
        if (node->fq_available)
            fprintf(stream, "%zu", node->fq);
        else
            fputs("null", stream);
        fprintf(stream, ",\"degree\":%zu,\"font_size\":%.17g}",
                node->degree, emit_node_font_size(node, range, config));
        free(label);
    }

    fputs("],\"links\":[", stream);
    for (size_t i = 0; i < edges->len; i++) {
        const Edge *edge = &edges->items[i];

        if (i > 0)
            fputc(',', stream);
        fputs("{\"source\":", stream);
        js_write_string(stream, edge->source);
        fputs(",\"target\":", stream);
        js_write_string(stream, edge->target);
        fprintf(stream,
                ",\"ctf\":%zu,\"cdf\":%zu,\"cw\":%.17g,\"z\":%.17g,"
                "\"unit_ids\":[",
                edge->ctf, edge->cdf, edge->cw, edge->z);
        for (size_t j = 0; j < edge->unit_count; j++) {
            if (j > 0)
                fputc(',', stream);
            js_write_string(stream, edge->unit_ids[j]);
        }
        fputs("]}", stream);
    }
    fputs("]}", stream);
}

void emit_d3_write(FILE *stream, const EdgeVec *edges,
                   const NodeVec *nodes, const Config *config)
{
    fputs(
        "<!doctype html>\n"
        "<html lang=\"en\">\n"
        "<head>\n"
        "<meta charset=\"utf-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
        "<title>cw-tools graph</title>\n"
        "<style>\n"
        "html,body{margin:0;width:100%;height:100%;overflow:hidden;"
        "font-family:system-ui,sans-serif}\n"
        "#graph{width:100%;height:100%;background:#fff}\n"
        ".link{stroke:#777;stroke-opacity:.55}\n"
        ".node circle{fill:#fff;stroke:#222;stroke-width:1.2px}\n"
        ".node text{pointer-events:none;text-anchor:middle;"
        "dominant-baseline:middle}\n"
        "#tip{position:fixed;display:none;max-width:34rem;padding:.55rem .7rem;"
        "border:1px solid #aaa;border-radius:.35rem;"
        "background:rgba(255,255,255,.96);box-shadow:0 2px 8px rgba(0,0,0,.18);"
        "font-size:.85rem;white-space:pre-wrap;pointer-events:none}\n"
        "</style>\n"
        "</head>\n"
        "<body>\n"
        "<svg id=\"graph\" aria-label=\"interactive network graph\"></svg>\n"
        "<div id=\"tip\"></div>\n"
        "<script src=\"https://cdn.jsdelivr.net/npm/d3@7\"></script>\n"
        "<script>\n"
        "\"use strict\";\n"
        "const graph=", stream);

    write_graph_data(stream, edges, nodes, config);

    fputs(
        ";\n"
        "const svg=d3.select(\"#graph\"),tip=d3.select(\"#tip\");\n"
        "const defs=svg.append(\"defs\");\n"
        "defs.append(\"marker\").attr(\"id\",\"arrow\").attr(\"viewBox\",\"0 -5 10 10\")"
        ".attr(\"refX\",16).attr(\"refY\",0).attr(\"markerWidth\",6)"
        ".attr(\"markerHeight\",6).attr(\"orient\",\"auto\")"
        ".append(\"path\").attr(\"d\",\"M0,-5L10,0L0,5\")"
        ".attr(\"fill\",\"#777\");\n"
        "const root=svg.append(\"g\");\n"
        "svg.call(d3.zoom().scaleExtent([.1,8])"
        ".on(\"zoom\",e=>root.attr(\"transform\",e.transform)));\n"
        "const width=()=>svg.node().clientWidth||960;\n"
        "const height=()=>svg.node().clientHeight||720;\n"
        "const maxAbsZ=d3.max(graph.links,d=>Math.abs(d.z))||1;\n"
        "const edgeWidth=d3.scaleLinear().domain([0,maxAbsZ]).range([.6,4]);\n"
        "const link=root.append(\"g\").selectAll(\"line\").data(graph.links)"
        ".join(\"line\").attr(\"class\",\"link\")"
        ".attr(\"stroke-width\",d=>edgeWidth(Math.abs(d.z)))"
        ".attr(\"marker-end\",graph.directed?\"url(#arrow)\":null);\n"
        "const node=root.append(\"g\").selectAll(\"g\").data(graph.nodes)"
        ".join(\"g\").attr(\"class\",\"node\")"
        ".call(d3.drag().on(\"start\",dragstarted)"
        ".on(\"drag\",dragged).on(\"end\",dragended));\n"
        "node.append(\"circle\").attr(\"r\",d=>Math.max(9,d.font_size*.7));\n"
        "node.append(\"text\").text(d=>d.label)"
        ".style(\"font-size\",d=>`${d.font_size}px`);\n"
        "function showTip(event,text){tip.style(\"display\",\"block\")"
        ".text(text);moveTip(event)}\n"
        "function moveTip(event){tip.style(\"left\",`${event.clientX+12}px`)"
        ".style(\"top\",`${event.clientY+12}px`)}\n"
        "function hideTip(){tip.style(\"display\",\"none\")}\n"
        "node.on(\"mouseenter\",(e,d)=>showTip(e,"
        "`label: ${d.label}\\nid: ${d.id}\\ndf: ${d.df}\\nidf: ${d.idf}"
        "\\nfq: ${d.fq??\"NA\"}\\ndegree: ${d.degree}`))"
        ".on(\"mousemove\",moveTip).on(\"mouseleave\",hideTip);\n"
        "link.on(\"mouseenter\",(e,d)=>showTip(e,"
        "`source: ${d.source.id??d.source}\\ntarget: ${d.target.id??d.target}"
        "\\nctf: ${d.ctf}\\ncdf: ${d.cdf}\\ncw: ${d.cw}\\nz: ${d.z}"
        "\\nunit_ids: ${d.unit_ids.join(\", \")}`))"
        ".on(\"mousemove\",moveTip).on(\"mouseleave\",hideTip);\n"
        "const simulation=d3.forceSimulation(graph.nodes)"
        ".force(\"link\",d3.forceLink(graph.links).id(d=>d.id).distance(", stream);

    fprintf(stream, "%.17g",
            config->edge_len_set ? config->edge_len * 55.0 : 78.0);

    fputs(
        "))"
        ".force(\"charge\",d3.forceManyBody().strength(-180))"
        ".force(\"center\",d3.forceCenter(width()/2,height()/2))"
        ".force(\"collision\",d3.forceCollide()"
        ".radius(d=>Math.max(14,d.font_size*.9)))"
        ".on(\"tick\",()=>{"
        "link.attr(\"x1\",d=>d.source.x).attr(\"y1\",d=>d.source.y)"
        ".attr(\"x2\",d=>d.target.x).attr(\"y2\",d=>d.target.y);"
        "node.attr(\"transform\",d=>`translate(${d.x},${d.y})`)});\n"
        "function dragstarted(e,d){if(!e.active)"
        "simulation.alphaTarget(.3).restart();d.fx=d.x;d.fy=d.y}\n"
        "function dragged(e,d){d.fx=e.x;d.fy=e.y}\n"
        "function dragended(e,d){if(!e.active)simulation.alphaTarget(0);"
        "d.fx=null;d.fy=null}\n"
        "window.addEventListener(\"resize\",()=>simulation.force(\"center\","
        "d3.forceCenter(width()/2,height()/2)).alpha(.2).restart());\n"
        "</script>\n"
        "</body>\n"
        "</html>\n", stream);
}
