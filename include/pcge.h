/* SPDX-License-Identifier: Apache-2.0 */
#ifndef PCGE_H
#define PCGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PCGE_VERSION "0.1.0"
#define PCGE_ABI_VERSION 1u
#define PCGE_SOURCE_COMMIT "e9e69ed9075c09e8d1e5475760e5b691aa73b251"

/* Todas las longitudes son bytes. Los resultados son préstamos inmutables de
 * duración estática: no modificarlos ni liberarlos. Las cadenas resultantes
 * tienen además NUL final, que no está incluido en len. Una cadena ausente
 * tiene data == NULL y len == 0 (padre de un elemento raíz).
 * Las entradas aceptan bytes UTF-8 + longitud; no requieren NUL final.
 * NULL solo es válido para una entrada textual de longitud cero. */
typedef struct { const char *data; size_t len; } pcge_string;
typedef enum { PCGE_2019 = 2019, PCGE_2026 = 2026 } pcge_edition;
typedef enum {
    PCGE_OK = 0,
    PCGE_INVALID_ARGUMENT = 1,
    PCGE_UNSUPPORTED_EDITION = 2,
    PCGE_NOT_FOUND = 3,
    PCGE_BUFFER_TOO_SMALL = 4,
    PCGE_INVALID_UTF8 = 5,
    PCGE_EMPTY_QUERY = 6,
    PCGE_OUT_OF_MEMORY = 7,
    PCGE_SIZE_OVERFLOW = 8
} pcge_status;
typedef enum {
    PCGE_LEVEL_UNSPECIFIED = 0, /* Seis dígitos: no se inventa nivel. */
    PCGE_ELEMENT = 1, PCGE_ACCOUNT = 2, PCGE_SUBACCOUNT = 3,
    PCGE_DIVISIONARY = 4, PCGE_SUBDIVISIONARY = 5
} pcge_level;
typedef struct {
    pcge_string code;
    pcge_string name;
    pcge_string parent_code;
} pcge_entry;
typedef struct {
    pcge_string pcge_version;
    uint32_t schema_version;
    uint32_t dataset_revision;
    size_t entry_count;
} pcge_metadata;
typedef struct { uint32_t first, last; } pcge_page_range;
typedef struct {
    pcge_string title, authority, resolution;
    pcge_string resolution_date, publication_date, mandatory_effective_date;
    pcge_string resolution_url, source_filename, source_sha256, dataset_sha256;
    pcge_string catalog_chapter;
    pcge_page_range catalog_pdf_pages, catalog_printed_pages;
} pcge_provenance;
typedef struct {
    uint32_t occurrence_index, pdf_page, printed_page;
    pcge_string printed_code, printed_name, printed_parent_code, disposition;
} pcge_occurrence;
typedef struct {
    pcge_string id, kind;
    const pcge_string *codes;
    size_t code_count;
    pcge_string status, description, decision, confirmation_no_invented_code;
    const pcge_occurrence *occurrences;
    size_t occurrence_count;
} pcge_anomaly;
typedef struct pcge_catalog pcge_catalog;
typedef enum {
    PCGE_JSON_ENTRIES = 0, PCGE_JSON_METADATA = 1,
    PCGE_JSON_PROVENANCE = 2, PCGE_JSON_ANOMALIES = 3
} pcge_json_document;

/* Carga explícita, sin red, E/S, asignaciones ni estado global mutable. */
pcge_status pcge_load(pcge_edition edition, const pcge_catalog **out);
const char *pcge_status_message(pcge_status status);
const char *pcge_unicode_version(void);
uint32_t pcge_abi_version(void);

/* Un catálogo inválido devuelve NULL/0 en los accesores sin status. */
const pcge_entry *pcge_entries(const pcge_catalog *catalog, size_t *count);
const pcge_metadata *pcge_get_metadata(const pcge_catalog *catalog);
const pcge_provenance *pcge_get_provenance(const pcge_catalog *catalog);
const pcge_anomaly *pcge_anomalies(const pcge_catalog *catalog, size_t *count);
pcge_level pcge_entry_level(const pcge_entry *entry);
pcge_status pcge_json(const pcge_catalog *catalog, pcge_json_document document,
                      pcge_string *out);

/* Búsqueda exacta. NOT_FOUND es diferente de un padre raíz (OK + NULL).
 * No recortan ni normalizan códigos, ni convierten códigos a números. */
pcge_status pcge_get(const pcge_catalog *catalog, const char *code, size_t length,
                     const pcge_entry **out);
pcge_status pcge_parent(const pcge_catalog *catalog, const char *code, size_t length,
                        const pcge_entry **out);

/* Colecciones: out_count es obligatorio. Primero llamar con out == NULL y
 * capacity == 0: devuelve BUFFER_TOO_SMALL y la cantidad necesaria si hay
 * resultados, o bien OK y cero. Repetir con un arreglo propio de punteros.
 * En BUFFER_TOO_SMALL no se escribe ningún elemento. En otros errores,
 * out_count queda en cero. capacity cuenta punteros, no bytes.
 * Los arreglos son del llamante; cada elemento es un préstamo estático.
 * Hijos conservan orden documental; ancestros van del padre a la raíz;
 * descendientes usan preorden DFS con hermanos en orden documental. */
pcge_status pcge_children(const pcge_catalog *catalog, const char *code, size_t length,
                          const pcge_entry **out, size_t capacity, size_t *out_count);
pcge_status pcge_ancestors(const pcge_catalog *catalog, const char *code, size_t length,
                           const pcge_entry **out, size_t capacity, size_t *out_count);
pcge_status pcge_descendants(const pcge_catalog *catalog, const char *code, size_t length,
                             const pcge_entry **out, size_t capacity, size_t *out_count);

/* Consulta UTF-8 estricta. Espacios en extremos según Python; NFKD, eliminación
 * de clase combinante no nula y full casefold con Unicode fijado. Devuelve
 * EMPTY_QUERY si está vacía ANTES de normalizar. Una consulta solo de marcas
 * coincide con todo, igual que el origen. Admite NUL embebido: no se trunca.
 * Única operación que usa malloc/free internamente (memoria temporal). */
pcge_status pcge_search(const pcge_catalog *catalog, const char *query, size_t length,
                        const pcge_entry **out, size_t capacity, size_t *out_count);

/* Incluye códigos impresos excluidos del catálogo. Código vacío o con espacios
 * en extremos: INVALID_ARGUMENT. No exige que el código exista en entries. */
pcge_status pcge_anomalies_for(const pcge_catalog *catalog, const char *code, size_t length,
                               const pcge_anomaly **out, size_t capacity, size_t *out_count);

#ifdef __cplusplus
}
#endif
#endif
