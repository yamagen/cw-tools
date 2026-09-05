#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "emit-d3.h"
#include "emit-url.h"
#include "emit-util.h"

static bool data_only_output;

void emit_d3_set_data_only(bool enabled)
{
    data_only_output = enabled;
}

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

static const char *z_sign_name(double z)
{
    if (z < 0.0)
        return "negative";
    if (z > 0.0)
        return "positive";
    return "zero";
}

static void write_string_array(FILE *stream, char *const *items,
                               size_t count)
{
    fputc('[', stream);
    for (size_t i = 0; i < count; i++) {
        if (i > 0)
            fputc(',', stream);
        js_write_string(stream, items[i]);
    }
    fputc(']', stream);
}

static void write_edge_label(FILE *stream, const Edge *edge,
                             const Config *config)
{
    char label[128];

    switch (config->edge_label) {
    case EDGE_LABEL_NONE:
        fputs("null", stream);
        return;
    case EDGE_LABEL_CTF:
        snprintf(label, sizeof(label), "%zu", edge->ctf);
        break;
    case EDGE_LABEL_CDF:
        snprintf(label, sizeof(label), "%zu", edge->cdf);
        break;
    case EDGE_LABEL_CW:
        snprintf(label, sizeof(label), "%.6g", edge->cw);
        break;
    case EDGE_LABEL_Z:
        snprintf(label, sizeof(label), "%.6g", edge->z);
        break;
    }
    js_write_string(stream, label);
}

static void write_component(FILE *stream, const Edge *edge, double value)
{
    if (edge->components_available)
        fprintf(stream, "%.17g", value);
    else
        fputs("null", stream);
}

static void write_graph_data(FILE *stream, const EdgeVec *edges,
                             const NodeVec *nodes, const Config *config)
{
    EmitNodeWeightRange node_range = emit_node_weight_range(nodes, config);
    EmitValueRange z_range = emit_edge_z_magnitude_range(edges);
    EmitUrlConfig url_config = emit_url_config_load(config->config_path);
    const char *font_source = emit_font_size_by_name(config->node_font_size_by);
    const char *direction_class = config->directed ? "directed" : "undirected";
    const double link_distance = config->edge_len_set ?
        config->edge_len * 55.0 : 78.0;

    fputs("{\"element_id\":\"graph-1\",\"directed\":", stream);
    fputs(config->directed ? "true" : "false", stream);
    fputs(",\"font_size_by\":", stream);
    js_write_string(stream, font_source);
    fprintf(stream, ",\"rank_count\":%d,\"link_distance\":%.17g,\"classes\":[",
            EMIT_RANK_COUNT, link_distance);
    js_write_string(stream, "graph");
    fputc(',', stream);
    char font_class[64];
    snprintf(font_class, sizeof(font_class), "font-by-%s", font_source);
    js_write_string(stream, font_class);
    fputc(',', stream);
    js_write_string(stream, direction_class);
    fputs("],\"nodes\":[", stream);

    for (size_t i = 0; i < nodes->len; i++) {
        const NodeRef *node = &nodes->items[i];
        char *label = emit_make_node_label(node->id, config);
        char element_id[64];
        char rank_class[64];
        size_t font_rank = emit_node_font_rank(node, node_range, config);

        snprintf(element_id, sizeof(element_id), "node-%zu", i + 1);
        snprintf(rank_class, sizeof(rank_class), "font-rank-%zu", font_rank);

        if (i > 0)
            fputc(',', stream);
        fputs("{\"id\":", stream);
        js_write_string(stream, node->id);
        fputs(",\"element_id\":", stream);
        js_write_string(stream, element_id);
        fputs(",\"label\":", stream);
        js_write_string(stream, label);
        fprintf(stream, ",\"df\":%zu,\"idf\":%.17g,\"fq\":",
                node->df, node->idf);
        if (node->fq_available)
            fprintf(stream, "%zu", node->fq);
        else
            fputs("null", stream);
        fprintf(stream,
                ",\"degree\":%zu,\"font_size\":%.17g,\"font_size_by\":",
                node->degree, emit_node_font_size(node, node_range, config));
        js_write_string(stream, font_source);
        fprintf(stream, ",\"font_rank\":%zu,\"classes\":[", font_rank);
        js_write_string(stream, "node");
        fputc(',', stream);
        js_write_string(stream, font_class);
        fputc(',', stream);
        js_write_string(stream, rank_class);
        fputs("]}", stream);
        free(label);
    }

    fputs("],\"links\":[", stream);
    for (size_t i = 0; i < edges->len; i++) {
        const Edge *edge = &edges->items[i];
        char element_id[64];
        char rank_class[64];
        char sign_class[64];
        size_t z_rank = emit_edge_z_rank(edge, z_range);
        const char *z_sign = z_sign_name(edge->z);
        char *url = emit_edge_url(edge, &url_config);

        snprintf(element_id, sizeof(element_id), "edge-%zu", i + 1);
        snprintf(rank_class, sizeof(rank_class), "z-rank-%zu", z_rank);
        snprintf(sign_class, sizeof(sign_class), "z-%s", z_sign);

        if (i > 0)
            fputc(',', stream);
        fputs("{\"element_id\":", stream);
        js_write_string(stream, element_id);
        fputs(",\"source\":", stream);
        js_write_string(stream, edge->source);
        fputs(",\"target\":", stream);
        js_write_string(stream, edge->target);
        fprintf(stream, ",\"ctf\":%zu,\"cdf\":%zu,\"g\":",
                edge->ctf, edge->cdf);
        write_component(stream, edge, edge->g);
        fputs(",\"c\":", stream);
        write_component(stream, edge, edge->c);
        fputs(",\"p\":", stream);
        write_component(stream, edge, edge->p);
        fprintf(stream, ",\"cw\":%.17g,\"z\":%.17g,\"z_rank\":%zu,\"z_sign\":",
                edge->cw, edge->z, z_rank);
        js_write_string(stream, z_sign);
        fputs(",\"label\":", stream);
        write_edge_label(stream, edge, config);
        fputs(",\"unit_ids\":", stream);
        write_string_array(stream, edge->unit_ids, edge->unit_count);
        fputs(",\"url\":", stream);
        if (url == NULL)
            fputs("null", stream);
        else
            js_write_string(stream, url);
        fputs(",\"url_target\":", stream);
        if (url == NULL || url_config.target == NULL ||
            url_config.target[0] == '\0')
            fputs("null", stream);
        else
            js_write_string(stream, url_config.target);
        fputs(",\"classes\":[", stream);
        js_write_string(stream, "edge");
        fputc(',', stream);
        js_write_string(stream, rank_class);
        fputc(',', stream);
        js_write_string(stream, sign_class);
        if (url != NULL) {
            fputc(',', stream);
            js_write_string(stream, "has-url");
        }
        fputs("]}", stream);
        free(url);
    }
    fputs("]}", stream);
    emit_url_config_free(&url_config);
}

void emit_d3_write(FILE *stream, const EdgeVec *edges,
                   const NodeVec *nodes, const Config *config)
{
    if (data_only_output) {
        fputs("\"use strict\";\nglobalThis.emitData=", stream);
        write_graph_data(stream, edges, nodes, config);
        fputs(";\n", stream);
        return;
    }

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
        ".edge line{stroke:#777;stroke-opacity:.55}\n"
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
        "<script src=\"https://cdn.jsdelivr.net/npm/d3@7.9.0/dist/d3.min.js\"></script>\n"
        "<script>\n"
        "\"use strict\";\n"
        "const graph=", stream);

    write_graph_data(stream, edges, nodes, config);

    fputs(
        ";\n"
        "const svg=d3.select(\"#graph\").attr(\"class\",graph.classes.join(\" \")),"
        "tip=d3.select(\"#tip\");\n"
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
        "const link=root.append(\"g\").selectAll(\"g\").data(graph.links)"
        ".join(\"g\").attr(\"id\",d=>d.element_id)"
        ".attr(\"class\",d=>d.classes.join(\" \"));\n"
        "const linkLine=link.append(\"line\").attr(\"stroke-width\",d=>edgeWidth(Math.abs(d.z)))"
        ".attr(\"marker-end\",graph.directed?\"url(#arrow)\":null);\n"
        "const node=root.append(\"g\").selectAll(\"g\").data(graph.nodes)"
        ".join(\"g\").attr(\"id\",d=>d.element_id)"
        ".attr(\"class\",d=>d.classes.join(\" \"))"
        ".call(d3.drag().on(\"start\",dragstarted)"
        ".on(\"drag\",dragged).on(\"end\",dragended));\n"
        "node.append(\"circle\").attr(\"r\",d=>Math.max(9,d.font_size*.7));\n"
        "node.append(\"text\").text(d=>d.label)"
        ".attr(\"font-size\",d=>d.font_size);\n"
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
        "\\nctf: ${d.ctf}\\ncdf: ${d.cdf}\\nG: ${d.g??\"NA\"}\\nC: ${d.c??\"NA\"}\\nP: ${d.p??\"NA\"}"
        "\\ncw: ${d.cw}\\nz: ${d.z}\\nunit_ids: ${d.unit_ids.join(\", \")}`))"
        ".on(\"mousemove\",moveTip).on(\"mouseleave\",hideTip);\n"
        "const simulation=d3.forceSimulation(graph.nodes)"
        ".force(\"link\",d3.forceLink(graph.links).id(d=>d.id).distance(graph.link_distance))"
        ".force(\"charge\",d3.forceManyBody().strength(-180))"
        ".force(\"center\",d3.forceCenter(width()/2,height()/2))"
        ".force(\"collision\",d3.forceCollide()"
        ".radius(d=>Math.max(14,d.font_size*.9)))"
        ".on(\"tick\",()=>{"
        "linkLine.attr(\"x1\",d=>d.source.x).attr(\"y1\",d=>d.source.y)"
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
