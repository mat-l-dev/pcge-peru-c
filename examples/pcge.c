/* Ejemplo CLI JSON. SPDX-License-Identifier: Apache-2.0 */
#include "pcge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void json_string(pcge_string s) {
    size_t i;
    if (s.data == NULL) { fputs("null", stdout); return; }
    putchar('"');
    for (i = 0; i < s.len; ++i) {
        unsigned char c = (unsigned char)s.data[i];
        if (c == '"' || c == '\\') { putchar('\\'); putchar(c); }
        else if (c < 0x20u) printf("\\u%04x", (unsigned int)c);
        else putchar(c);
    }
    putchar('"');
}
static void entry_json(const pcge_entry *entry) {
    if (entry == NULL) { fputs("null", stdout); return; }
    fputs("{\"code\":", stdout); json_string(entry->code);
    fputs(",\"name\":", stdout); json_string(entry->name);
    fputs(",\"parent_code\":", stdout); json_string(entry->parent_code);
    putchar('}');
}
static void codes_json(const pcge_entry **entries, size_t count) {
    size_t i;
    putchar('[');
    for (i = 0; i < count; ++i) { if (i != 0) putchar(','); json_string(entries[i]->code); }
    putchar(']');
}
typedef pcge_status (*query_fn)(const pcge_catalog *, const char *, size_t,
                               const pcge_entry **, size_t, size_t *);
static int report(pcge_status status) {
    if (status == PCGE_OK) return 0;
    fprintf(stderr, "%s\n", pcge_status_message(status));
    return 1;
}
static int usage(void) {
    fputs("Uso: pcge {2019|2026} {dump|nav|metadata|source|anomalies}\n"
          "     pcge {2019|2026} {get|parent|children|ancestors|descendants|anomalies-for} CÓDIGO\n"
          "     pcge {2019|2026} search CONSULTA\n", stderr);
    return 2;
}
int main(int argc, char **argv) {
    const pcge_catalog *catalog;
    const pcge_entry *entries, **results, *one;
    const char *command;
    size_t count, n = 0, i;
    pcge_status status = PCGE_OK;
    query_fn query = NULL;
    if (argc < 3 || argc > 4) return usage();
    if (strcmp(argv[1], "2019") == 0) status = pcge_load(PCGE_2019, &catalog);
    else if (strcmp(argv[1], "2026") == 0) status = pcge_load(PCGE_2026, &catalog);
    else return usage();
    if (report(status)) return 1;
    command = argv[2];
    entries = pcge_entries(catalog, &count);
    results = (const pcge_entry **)malloc(count * sizeof(*results));
    if (results == NULL) return report(PCGE_OUT_OF_MEMORY);
    if (strcmp(command, "dump") == 0 && argc == 3) {
        putchar('[');
        for (i = 0; i < count; ++i) { if (i != 0) putchar(','); entry_json(&entries[i]); }
        putchar(']');
    } else if (strcmp(command, "nav") == 0 && argc == 3) {
        putchar('[');
        for (i = 0; i < count; ++i) {
            const pcge_entry *entry = &entries[i];
            if (i != 0) putchar(',');
            fputs("{\"code\":", stdout); json_string(entry->code);
            fputs(",\"parent\":", stdout); json_string(entry->parent_code);
            fputs(",\"children\":", stdout);
            status = pcge_children(catalog, entry->code.data, entry->code.len, results, count, &n);
            if (status != PCGE_OK) break;
            codes_json(results, n);
            fputs(",\"ancestors\":", stdout);
            status = pcge_ancestors(catalog, entry->code.data, entry->code.len, results, count, &n);
            if (status != PCGE_OK) break;
            codes_json(results, n);
            fputs(",\"descendants\":", stdout);
            status = pcge_descendants(catalog, entry->code.data, entry->code.len, results, count, &n);
            if (status != PCGE_OK) break;
            codes_json(results, n);
            putchar('}');
        }
        putchar(']');
    } else if (argc == 3 && (strcmp(command, "metadata") == 0 || strcmp(command, "source") == 0 || strcmp(command, "anomalies") == 0)) {
        pcge_string json;
        pcge_json_document doc = strcmp(command, "metadata") == 0 ? PCGE_JSON_METADATA :
                                 strcmp(command, "source") == 0 ? PCGE_JSON_PROVENANCE : PCGE_JSON_ANOMALIES;
        status = pcge_json(catalog, doc, &json);
        if (status == PCGE_OK) fwrite(json.data, 1, json.len, stdout);
    } else if (argc == 4 && (strcmp(command, "get") == 0 || strcmp(command, "parent") == 0)) {
        status = strcmp(command, "get") == 0 ? pcge_get(catalog, argv[3], strlen(argv[3]), &one) :
                 pcge_parent(catalog, argv[3], strlen(argv[3]), &one);
        if (status == PCGE_OK) entry_json(one);
    } else if (argc == 4 && strcmp(command, "anomalies-for") == 0) {
        const pcge_anomaly **anomalies;
        size_t total;
        (void)pcge_anomalies(catalog, &total);
        anomalies = (const pcge_anomaly **)malloc(total * sizeof(*anomalies));
        if (anomalies == NULL) status = PCGE_OUT_OF_MEMORY;
        else {
            status = pcge_anomalies_for(catalog, argv[3], strlen(argv[3]), anomalies, total, &n);
            if (status == PCGE_OK) {
                putchar('[');
                for (i = 0; i < n; ++i) { if (i != 0) putchar(','); json_string(anomalies[i]->id); }
                putchar(']');
            }
            free(anomalies);
        }
    } else if (argc == 4) {
        if (strcmp(command, "search") == 0) query = pcge_search;
        else if (strcmp(command, "children") == 0) query = pcge_children;
        else if (strcmp(command, "ancestors") == 0) query = pcge_ancestors;
        else if (strcmp(command, "descendants") == 0) query = pcge_descendants;
        if (query == NULL) { free(results); return usage(); }
        status = query(catalog, argv[3], strlen(argv[3]), results, count, &n);
        if (status == PCGE_OK) codes_json(results, n);
    } else { free(results); return usage(); }
    free(results);
    if (report(status)) return 1;
    putchar('\n');
    return ferror(stdout) ? 1 : 0;
}
