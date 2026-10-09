/* SPDX-License-Identifier: Apache-2.0 */
#undef NDEBUG
#include "pcge.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int eq(pcge_string s, const char *text) {
    return s.len == strlen(text) && memcmp(s.data, text, s.len) == 0;
}
static void test_catalog(pcge_edition edition, size_t expected, size_t anomalies_expected) {
    const pcge_catalog *catalog;
    const pcge_entry *entries, *found, *parent;
    const pcge_entry **buffer;
    const pcge_anomaly *anomalies;
    size_t count, n, i;
    pcge_string raw;
    assert(pcge_load(edition, &catalog) == PCGE_OK);
    entries = pcge_entries(catalog, &count);
    assert(count == expected && entries != NULL);
    assert(pcge_get_metadata(catalog)->entry_count == expected);
    assert(pcge_get_provenance(catalog)->dataset_sha256.len == 64);
    anomalies = pcge_anomalies(catalog, &n);
    assert(anomalies != NULL && n == anomalies_expected);
    buffer = (const pcge_entry **)malloc(count * sizeof(*buffer));
    assert(buffer != NULL);
    for (i = 0; i < count; ++i) {
        size_t j;
        const pcge_entry *entry = &entries[i];
        assert(entry->code.len >= 1 && entry->code.len <= 6);
        for (j = 0; j < entry->code.len; ++j) assert(entry->code.data[j] >= '0' && entry->code.data[j] <= '9');
        assert(pcge_get(catalog, entry->code.data, entry->code.len, &found) == PCGE_OK && found == entry);
        assert(pcge_parent(catalog, entry->code.data, entry->code.len, &parent) == PCGE_OK);
        if (entry->code.len == 1) assert(parent == NULL);
        else {
            assert(parent != NULL && parent->code.len + 1 == entry->code.len);
            assert(memcmp(entry->code.data, parent->code.data, parent->code.len) == 0);
        }
        assert(pcge_entry_level(entry) == (entry->code.len <= 5 ? (pcge_level)entry->code.len : PCGE_LEVEL_UNSPECIFIED));
        assert(pcge_ancestors(catalog, entry->code.data, entry->code.len, buffer, count, &n) == PCGE_OK);
        assert(n + 1 == entry->code.len);
        if (n > 0) assert(buffer[0] == parent && buffer[n - 1]->code.len == 1);
        assert(pcge_search(catalog, entry->name.data, entry->name.len, buffer, count, &n) == PCGE_OK);
        for (j = 0; j < n && buffer[j] != entry; ++j) { }
        assert(j != n);
    }
    assert(pcge_get(catalog, "101", 3, &found) == PCGE_OK && eq(found->name, "Caja"));
    assert(pcge_children(catalog, "101", 3, NULL, 0, &n) == PCGE_OK && n == 0);
    assert(pcge_children(catalog, "10", 2, NULL, 0, &n) == PCGE_BUFFER_TOO_SMALL && n > 0);
    buffer[0] = found;
    assert(pcge_children(catalog, "10", 2, buffer, n - 1, &n) == PCGE_BUFFER_TOO_SMALL && buffer[0] == found);
    assert(pcge_search(catalog, "DEPOSITOS", 9, buffer, count, &n) == PCGE_OK && n > 0);
    assert(pcge_search(catalog, "\xcc\x81", 2, buffer, count, &n) == PCGE_OK && n == count);
    assert(pcge_search(catalog, "\xe2\x80\x83\x1c", 4, buffer, count, &n) == PCGE_EMPTY_QUERY && n == 0);
    assert(pcge_search(catalog, "caja\0x", 6, buffer, count, &n) == PCGE_OK && n == 0);
    assert(pcge_search(catalog, "\xff", 1, NULL, 0, &n) == PCGE_INVALID_UTF8 && n == 0);
    assert(pcge_search(catalog, "\xc0\x80", 2, NULL, 0, &n) == PCGE_INVALID_UTF8);
    assert(pcge_search(catalog, "\xed\xa0\x80", 3, NULL, 0, &n) == PCGE_INVALID_UTF8);
    assert(pcge_search(catalog, "\xf4\x90\x80\x80", 4, NULL, 0, &n) == PCGE_INVALID_UTF8);
    assert(pcge_search(catalog, "\xe2\x82", 2, NULL, 0, &n) == PCGE_INVALID_UTF8);
    assert(pcge_get(catalog, " 101", 4, &found) == PCGE_NOT_FOUND && found == NULL);
    assert(pcge_parent(catalog, "no", 2, &parent) == PCGE_NOT_FOUND && parent == NULL);
    assert(pcge_children(catalog, "no", 2, buffer, count, &n) == PCGE_NOT_FOUND && n == 0);
    assert(pcge_descendants(catalog, "no", 2, buffer, count, &n) == PCGE_NOT_FOUND && n == 0);
    assert(pcge_ancestors(catalog, "no", 2, buffer, count, &n) == PCGE_NOT_FOUND && n == 0);
    assert(pcge_json(catalog, PCGE_JSON_ENTRIES, &raw) == PCGE_OK && raw.len > 0 && raw.data[0] == '[');
    assert(raw.data[raw.len] == '\0');
    for (i = 0; i < count; ++i) {
        assert(entries[i].code.data[entries[i].code.len] == '\0');
        assert(entries[i].name.data[entries[i].name.len] == '\0');
    }
    free(buffer);
}
static void test_errors(void) {
    const pcge_catalog *catalog;
    const pcge_entry *entry;
    const pcge_anomaly *anomaly;
    pcge_string json;
    size_t n = 99;
    assert(pcge_load((pcge_edition)2025, &catalog) == PCGE_UNSUPPORTED_EDITION && catalog == NULL);
    assert(pcge_load(PCGE_2026, NULL) == PCGE_INVALID_ARGUMENT);
    assert(pcge_get(NULL, "101", 3, &entry) == PCGE_INVALID_ARGUMENT && entry == NULL);
    assert(pcge_entries(NULL, &n) == NULL && n == 0);
    assert(pcge_get_metadata(NULL) == NULL && pcge_get_provenance(NULL) == NULL);
    assert(pcge_anomalies(NULL, &n) == NULL && n == 0);
    assert(pcge_entry_level(NULL) == PCGE_LEVEL_UNSPECIFIED);
    assert(pcge_load(PCGE_2026, &catalog) == PCGE_OK);
    assert(pcge_get(catalog, NULL, 1, &entry) == PCGE_INVALID_ARGUMENT);
    assert(pcge_get(catalog, NULL, 0, &entry) == PCGE_NOT_FOUND);
    assert(pcge_get(catalog, "101", 3, NULL) == PCGE_INVALID_ARGUMENT);
    assert(pcge_parent(catalog, "101", 3, NULL) == PCGE_INVALID_ARGUMENT);
    assert(pcge_children(catalog, "10", 2, NULL, 1, &n) == PCGE_INVALID_ARGUMENT && n == 0);
    assert(pcge_search(catalog, "caja", 4, NULL, 0, NULL) == PCGE_INVALID_ARGUMENT);
    assert(pcge_search(catalog, NULL, 0, NULL, 0, &n) == PCGE_EMPTY_QUERY);
    assert(pcge_search(catalog, NULL, 1, NULL, 0, &n) == PCGE_INVALID_ARGUMENT);
    assert(pcge_json(catalog, (pcge_json_document)99, &json) == PCGE_INVALID_ARGUMENT && json.data == NULL && json.len == 0);
    assert(pcge_json(catalog, PCGE_JSON_ENTRIES, NULL) == PCGE_INVALID_ARGUMENT);
    assert(pcge_anomalies_for(catalog, "", 0, NULL, 0, &n) == PCGE_INVALID_ARGUMENT);
    assert(pcge_anomalies_for(catalog, " 70992", 6, NULL, 0, &n) == PCGE_INVALID_ARGUMENT);
    assert(pcge_anomalies_for(catalog, "70992", 5, NULL, 0, &n) == PCGE_BUFFER_TOO_SMALL && n == 1);
    assert(pcge_anomalies_for(catalog, "70992", 5, &anomaly, 1, &n) == PCGE_OK && eq(anomaly->id, "ANOMALY-2026-70992"));
    assert(anomaly->occurrence_count == 2 && eq(anomaly->occurrences[0].printed_parent_code, "7090"));
    assert(pcge_get(catalog, "70902", 5, &entry) == PCGE_NOT_FOUND);
    assert(pcge_load(PCGE_2019, &catalog) == PCGE_OK);
    assert(pcge_anomalies_for(catalog, "33404", 5, &anomaly, 1, &n) == PCGE_OK && n == 1);
    assert(pcge_get(catalog, "33404", 5, &entry) == PCGE_NOT_FOUND);
    assert(pcge_get(catalog, "36404", 5, &entry) == PCGE_NOT_FOUND);
    assert(pcge_abi_version() == 1 && strcmp(pcge_unicode_version(), "16.0.0") == 0);
}
int main(void) {
    test_catalog(PCGE_2019, 1757, 10);
    test_catalog(PCGE_2026, 1636, 1);
    test_errors();
    puts("API C: 3393 registros, relaciones, búsquedas, anomalías y errores correctos");
    return 0;
}
