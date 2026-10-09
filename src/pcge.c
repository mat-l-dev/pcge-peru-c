/* SPDX-License-Identifier: Apache-2.0 */
#include "pcge.h"
#include <stdlib.h>
#include <string.h>

struct pcge_catalog {
    const pcge_entry *entries;
    const pcge_string *search_names;
    const size_t *by_code;
    size_t count;
    pcge_metadata metadata;
    pcge_provenance provenance;
    const pcge_anomaly *anomalies;
    size_t anomaly_count;
    pcge_string json[4];
};
typedef struct { uint32_t codepoint; pcge_string replacement; } unicode_mapping;
#include "data.inc"
#include "unicode_data.inc"

static int valid_catalog(const pcge_catalog *catalog) {
    return catalog == &catalog_2019 || catalog == &catalog_2026;
}
static int valid_text(const char *text, size_t length) { return text != NULL || length == 0; }
static int equal(pcge_string value, const char *text, size_t length) {
    return value.len == length && (length == 0 || memcmp(value.data, text, length) == 0);
}
static int compare(pcge_string value, const char *text, size_t length) {
    size_t n = value.len < length ? value.len : length;
    int result = n == 0 ? 0 : memcmp(value.data, text, n);
    return result != 0 ? result : ((value.len > length) - (value.len < length));
}
static const pcge_entry *lookup(const pcge_catalog *catalog, const char *code, size_t length) {
    size_t low = 0, high = catalog->count;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        const pcge_entry *entry = &catalog->entries[catalog->by_code[mid]];
        int order = compare(entry->code, code, length);
        if (order == 0) return entry;
        if (order < 0) low = mid + 1;
        else high = mid;
    }
    return NULL;
}
pcge_status pcge_load(pcge_edition edition, const pcge_catalog **out) {
    if (out == NULL) return PCGE_INVALID_ARGUMENT;
    *out = NULL;
    if (edition == PCGE_2019) *out = &catalog_2019;
    else if (edition == PCGE_2026) *out = &catalog_2026;
    else return PCGE_UNSUPPORTED_EDITION;
    return PCGE_OK;
}
uint32_t pcge_abi_version(void) { return PCGE_ABI_VERSION; }
const char *pcge_unicode_version(void) { return UNICODE_VERSION; }
const char *pcge_status_message(pcge_status status) {
    switch (status) {
        case PCGE_OK: return "correcto";
        case PCGE_INVALID_ARGUMENT: return "argumento inválido";
        case PCGE_UNSUPPORTED_EDITION: return "edición no disponible";
        case PCGE_NOT_FOUND: return "código no encontrado";
        case PCGE_BUFFER_TOO_SMALL: return "capacidad insuficiente";
        case PCGE_INVALID_UTF8: return "UTF-8 inválido";
        case PCGE_EMPTY_QUERY: return "consulta vacía";
        case PCGE_OUT_OF_MEMORY: return "sin memoria";
        case PCGE_SIZE_OVERFLOW: return "longitud fuera de rango";
        default: return "estado desconocido";
    }
}
const pcge_entry *pcge_entries(const pcge_catalog *catalog, size_t *count) {
    if (count != NULL) *count = valid_catalog(catalog) ? catalog->count : 0;
    return valid_catalog(catalog) ? catalog->entries : NULL;
}
const pcge_metadata *pcge_get_metadata(const pcge_catalog *catalog) {
    return valid_catalog(catalog) ? &catalog->metadata : NULL;
}
const pcge_provenance *pcge_get_provenance(const pcge_catalog *catalog) {
    return valid_catalog(catalog) ? &catalog->provenance : NULL;
}
const pcge_anomaly *pcge_anomalies(const pcge_catalog *catalog, size_t *count) {
    if (count != NULL) *count = valid_catalog(catalog) ? catalog->anomaly_count : 0;
    return valid_catalog(catalog) ? catalog->anomalies : NULL;
}
pcge_level pcge_entry_level(const pcge_entry *entry) {
    if (entry == NULL || entry->code.len < 1 || entry->code.len > 5) return PCGE_LEVEL_UNSPECIFIED;
    return (pcge_level)entry->code.len;
}
pcge_status pcge_json(const pcge_catalog *catalog, pcge_json_document document, pcge_string *out) {
    if (out == NULL) return PCGE_INVALID_ARGUMENT;
    out->data = NULL; out->len = 0;
    if (!valid_catalog(catalog) || document < PCGE_JSON_ENTRIES || document > PCGE_JSON_ANOMALIES)
        return PCGE_INVALID_ARGUMENT;
    *out = catalog->json[document];
    return PCGE_OK;
}
pcge_status pcge_get(const pcge_catalog *catalog, const char *code, size_t length, const pcge_entry **out) {
    if (out == NULL) return PCGE_INVALID_ARGUMENT;
    *out = NULL;
    if (!valid_catalog(catalog) || !valid_text(code, length)) return PCGE_INVALID_ARGUMENT;
    *out = lookup(catalog, code, length);
    return *out == NULL ? PCGE_NOT_FOUND : PCGE_OK;
}
pcge_status pcge_parent(const pcge_catalog *catalog, const char *code, size_t length, const pcge_entry **out) {
    const pcge_entry *entry = NULL;
    pcge_status status;
    if (out == NULL) return PCGE_INVALID_ARGUMENT;
    *out = NULL;
    status = pcge_get(catalog, code, length, &entry);
    if (status != PCGE_OK) return status;
    if (entry->parent_code.data != NULL) *out = lookup(catalog, entry->parent_code.data, entry->parent_code.len);
    return PCGE_OK;
}
static pcge_status validate_collection(const pcge_catalog *catalog, const char *text, size_t length,
                                       const void *out, size_t capacity, size_t *out_count) {
    if (out_count == NULL) return PCGE_INVALID_ARGUMENT;
    *out_count = 0;
    if (!valid_catalog(catalog) || !valid_text(text, length) || (out == NULL && capacity != 0))
        return PCGE_INVALID_ARGUMENT;
    return PCGE_OK;
}
pcge_status pcge_children(const pcge_catalog *catalog, const char *code, size_t length,
                          const pcge_entry **out, size_t capacity, size_t *out_count) {
    size_t i, count = 0;
    pcge_status status = validate_collection(catalog, code, length, out, capacity, out_count);
    if (status != PCGE_OK) return status;
    if (lookup(catalog, code, length) == NULL) return PCGE_NOT_FOUND;
    for (i = 0; i < catalog->count; ++i)
        if (equal(catalog->entries[i].parent_code, code, length)) ++count;
    *out_count = count;
    if (capacity < count) return PCGE_BUFFER_TOO_SMALL;
    count = 0;
    for (i = 0; i < catalog->count; ++i)
        if (equal(catalog->entries[i].parent_code, code, length)) out[count++] = &catalog->entries[i];
    return PCGE_OK;
}
pcge_status pcge_ancestors(const pcge_catalog *catalog, const char *code, size_t length,
                           const pcge_entry **out, size_t capacity, size_t *out_count) {
    const pcge_entry *entry, *ancestors[6];
    size_t count = 0, i;
    pcge_status status = validate_collection(catalog, code, length, out, capacity, out_count);
    if (status != PCGE_OK) return status;
    entry = lookup(catalog, code, length);
    if (entry == NULL) return PCGE_NOT_FOUND;
    while (entry->parent_code.data != NULL) {
        entry = lookup(catalog, entry->parent_code.data, entry->parent_code.len);
        ancestors[count++] = entry;
    }
    *out_count = count;
    if (capacity < count) return PCGE_BUFFER_TOO_SMALL;
    for (i = 0; i < count; ++i) out[i] = ancestors[i];
    return PCGE_OK;
}
static size_t descendants(const pcge_catalog *catalog, pcge_string code,
                          const pcge_entry **out, size_t offset) {
    size_t i;
    for (i = 0; i < catalog->count; ++i) {
        const pcge_entry *child = &catalog->entries[i];
        if (equal(child->parent_code, code.data, code.len)) {
            if (out != NULL) out[offset] = child;
            ++offset;
            offset = descendants(catalog, child->code, out, offset);
        }
    }
    return offset;
}
pcge_status pcge_descendants(const pcge_catalog *catalog, const char *code, size_t length,
                             const pcge_entry **out, size_t capacity, size_t *out_count) {
    const pcge_entry *entry;
    size_t count;
    pcge_status status = validate_collection(catalog, code, length, out, capacity, out_count);
    if (status != PCGE_OK) return status;
    entry = lookup(catalog, code, length);
    if (entry == NULL) return PCGE_NOT_FOUND;
    count = descendants(catalog, entry->code, NULL, 0);
    *out_count = count;
    if (capacity < count) return PCGE_BUFFER_TOO_SMALL;
    if (count != 0) (void)descendants(catalog, entry->code, out, 0);
    return PCGE_OK;
}

/* Decode estricto: rechaza sobrelongitudes, sustitutos y valores > U+10FFFF. */
static pcge_status decode(const char *text, size_t length, size_t *offset, uint32_t *cp) {
    const unsigned char *s = (const unsigned char *)text;
    size_t i = *offset, remaining;
    uint32_t value, minimum;
    unsigned char first = s[i++];
    if (first < 0x80u) { *cp = first; *offset = i; return PCGE_OK; }
    if (first >= 0xc2u && first <= 0xdfu) { remaining = 1; value = first & 0x1fu; minimum = 0x80u; }
    else if (first >= 0xe0u && first <= 0xefu) { remaining = 2; value = first & 0x0fu; minimum = 0x800u; }
    else if (first >= 0xf0u && first <= 0xf4u) { remaining = 3; value = first & 0x07u; minimum = 0x10000u; }
    else return PCGE_INVALID_UTF8;
    if (remaining > length - i) return PCGE_INVALID_UTF8;
    while (remaining-- != 0) {
        unsigned char next = s[i++];
        if ((next & 0xc0u) != 0x80u) return PCGE_INVALID_UTF8;
        value = (value << 6) | (next & 0x3fu);
    }
    if (value < minimum || value > 0x10ffffu || (value >= 0xd800u && value <= 0xdfffu)) return PCGE_INVALID_UTF8;
    *cp = value; *offset = i;
    return PCGE_OK;
}
static int whitespace(uint32_t cp) {
    size_t low = 0, high = sizeof(unicode_whitespace) / sizeof(unicode_whitespace[0]);
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (unicode_whitespace[mid] == cp) return 1;
        if (unicode_whitespace[mid] < cp) low = mid + 1; else high = mid;
    }
    return 0;
}
static const pcge_string *mapping(uint32_t cp) {
    size_t low = 0, high = sizeof(unicode_mappings) / sizeof(unicode_mappings[0]);
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (unicode_mappings[mid].codepoint == cp) return &unicode_mappings[mid].replacement;
        if (unicode_mappings[mid].codepoint < cp) low = mid + 1; else high = mid;
    }
    return NULL;
}
static pcge_status trim(const char *text, size_t length, size_t *start, size_t *end) {
    size_t i = 0;
    *start = length; *end = 0;
    while (i < length) {
        size_t before = i;
        uint32_t cp;
        pcge_status status = decode(text, length, &i, &cp);
        if (status != PCGE_OK) return status;
        if (!whitespace(cp)) {
            if (*start == length) *start = before;
            *end = i;
        }
    }
    return PCGE_OK;
}
static pcge_status normalize(const char *text, size_t start, size_t end, char **out, size_t *out_length) {
    size_t pass, needed = 0, written = 0;
    *out = NULL; *out_length = 0;
    for (pass = 0; pass < 2; ++pass) {
        size_t i = start;
        while (i < end) {
            size_t before = i;
            uint32_t cp;
            const pcge_string *replacement;
            const char *bytes;
            size_t n;
            pcge_status status = decode(text, end, &i, &cp);
            if (status != PCGE_OK) { free(*out); *out = NULL; return status; }
            replacement = mapping(cp);
            bytes = replacement == NULL ? text + before : replacement->data;
            n = replacement == NULL ? i - before : replacement->len;
            if (pass == 0) {
                if (n > SIZE_MAX - needed) return PCGE_SIZE_OVERFLOW;
                needed += n;
            } else if (n != 0) {
                memcpy(*out + written, bytes, n);
                written += n;
            }
        }
        if (pass == 0) {
            if (needed == SIZE_MAX) return PCGE_SIZE_OVERFLOW;
            *out = (char *)malloc(needed + 1);
            if (*out == NULL) return PCGE_OUT_OF_MEMORY;
        }
    }
    (*out)[needed] = '\0'; *out_length = needed;
    return PCGE_OK;
}
static int contains(pcge_string text, const char *query, size_t length) {
    size_t i;
    if (length == 0) return 1;
    if (length > text.len) return 0;
    for (i = 0; i <= text.len - length; ++i)
        if (memcmp(text.data + i, query, length) == 0) return 1;
    return 0;
}
pcge_status pcge_search(const pcge_catalog *catalog, const char *query, size_t length,
                        const pcge_entry **out, size_t capacity, size_t *out_count) {
    size_t start, end, normalized_length, i, count = 0;
    char *normalized;
    pcge_status status = validate_collection(catalog, query, length, out, capacity, out_count);
    if (status != PCGE_OK) return status;
    status = trim(query, length, &start, &end);
    if (status != PCGE_OK) return status;
    if (end == 0) return PCGE_EMPTY_QUERY;
    status = normalize(query, start, end, &normalized, &normalized_length);
    if (status != PCGE_OK) return status;
    for (i = 0; i < catalog->count; ++i)
        if (contains(catalog->entries[i].code, normalized, normalized_length) ||
            contains(catalog->search_names[i], normalized, normalized_length)) ++count;
    *out_count = count;
    if (capacity < count) { free(normalized); return PCGE_BUFFER_TOO_SMALL; }
    count = 0;
    for (i = 0; i < catalog->count; ++i)
        if (contains(catalog->entries[i].code, normalized, normalized_length) ||
            contains(catalog->search_names[i], normalized, normalized_length)) out[count++] = &catalog->entries[i];
    free(normalized);
    return PCGE_OK;
}
static int anomaly_matches(const pcge_anomaly *anomaly, const char *code, size_t length) {
    size_t i;
    for (i = 0; i < anomaly->code_count; ++i) if (equal(anomaly->codes[i], code, length)) return 1;
    for (i = 0; i < anomaly->occurrence_count; ++i) if (equal(anomaly->occurrences[i].printed_code, code, length)) return 1;
    return 0;
}
pcge_status pcge_anomalies_for(const pcge_catalog *catalog, const char *code, size_t length,
                               const pcge_anomaly **out, size_t capacity, size_t *out_count) {
    size_t start, end, count = 0, i;
    pcge_status status = validate_collection(catalog, code, length, out, capacity, out_count);
    if (status != PCGE_OK) return status;
    status = trim(code, length, &start, &end);
    if (status != PCGE_OK) return status;
    if (length == 0 || start != 0 || end != length) return PCGE_INVALID_ARGUMENT;
    for (i = 0; i < catalog->anomaly_count; ++i)
        if (anomaly_matches(&catalog->anomalies[i], code, length)) ++count;
    *out_count = count;
    if (capacity < count) return PCGE_BUFFER_TOO_SMALL;
    count = 0;
    for (i = 0; i < catalog->anomaly_count; ++i)
        if (anomaly_matches(&catalog->anomalies[i], code, length)) out[count++] = &catalog->anomalies[i];
    return PCGE_OK;
}
