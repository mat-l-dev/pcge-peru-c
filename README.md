# PCGE Perú para C

Biblioteca C99 de consulta del Plan Contable General Empresarial, con ediciones 2019 y 2026 explícitas. Incluye los catálogos canónicos completos, jerarquía, búsqueda Unicode, procedencia documental y anomalías conocidas. No utiliza Rust, red, archivos externos ni un parser JSON durante la ejecución.

- Edición 2019: 1.757 registros y 10 anomalías documentadas
- Edición 2026: 1.636 registros y 1 anomalía documentada
- Datos fijados al commit [`e9e69ed`](https://github.com/mat-l-dev/pcge-peru/tree/e9e69ed9075c09e8d1e5475760e5b691aa73b251) del proyecto de origen
- Unicode 16.0.0 completo para búsqueda; sin dependencia de la configuración regional
- Implementación C99 sin dependencias externas y sin estado global mutable

## Compilar

Se necesitan un compilador C99, `make` y `ar`. Python no es necesario para compilar, instalar, ejecutar o consumir la biblioteca.

```sh
make
make test
./build/pcge 2026 get 101
./build/pcge 2019 search "depósitos"
```

El resultado es `build/libpcge.a`, el encabezado público es `include/pcge.h` y `build/pcge` es un ejemplo de CLI JSON.

```sh
cc -std=c99 -Iinclude mi_programa.c build/libpcge.a -o mi_programa
make install PREFIX="$HOME/.local"
```

La instalación incluye la biblioteca, el encabezado y los avisos de licencia. `DESTDIR` permite preparar un paquete sin instalar en el sistema.

También se incluye configuración CMake (3.16 o posterior):

```sh
cmake -S . -B build/cmake
cmake --build build/cmake
ctest --test-dir build/cmake --output-on-failure
cmake --install build/cmake --prefix "$HOME/.local"
```

Un consumidor CMake puede usar `find_package(pcge CONFIG REQUIRED)` y enlazar `pcge::pcge`. La integración CMake se proporciona, pero no se ejecutó en el entorno inicial de comprobación, que no tenía CMake.

## Consulta mínima

```c
#include <pcge.h>
#include <stdio.h>

int main(void) {
    const pcge_catalog *catalog;
    const pcge_entry *entry;
    if (pcge_load(PCGE_2026, &catalog) != PCGE_OK) return 1;
    if (pcge_get(catalog, "101", 3, &entry) != PCGE_OK) return 1;
    printf("%s: %s\n", entry->code.data, entry->name.data);
    return 0;
}
```

## Resultados y memoria

El encabezado documenta cada función. Las longitudes de texto siempre están expresadas en bytes; las capacidades de resultados cuentan punteros.

- El catálogo, sus registros, cadenas y metadatos son inmutables y viven durante toda la ejecución. No se liberan ni modifican
- Las cadenas de salida incluyen NUL final, excluido de `len`. Un padre ausente tiene `data == NULL` y `len == 0`
- Las entradas son puntero más longitud; no necesitan NUL final. `NULL` solo representa una entrada textual de longitud cero
- El llamante proporciona los arreglos para colecciones. Una primera llamada con `out == NULL`, `capacity == 0` obtiene la cantidad necesaria. Si es mayor que cero devuelve `PCGE_BUFFER_TOO_SMALL`; si es cero devuelve `PCGE_OK`
- `PCGE_BUFFER_TOO_SMALL` no modifica los elementos del arreglo. En otros errores, `out_count` queda en cero
- Solo la búsqueda reserva memoria temporal internamente, y siempre la libera antes de retornar. Devuelve `PCGE_OUT_OF_MEMORY` si falla la reserva
- Las consultas pueden usarse en varios hilos con arreglos de salida independientes. No hay inicialización compartida mutable

Ejemplo de colección:

```c
#include <pcge.h>
#include <stdlib.h>

int buscar(const pcge_catalog *catalog) {
    const pcge_entry **matches = NULL;
    size_t count = 0;
    pcge_status status = pcge_search(catalog, "caja", 4, NULL, 0, &count);
    if (status == PCGE_OK) return 0; /* Sin coincidencias. */
    if (status != PCGE_BUFFER_TOO_SMALL) return 1;
    matches = malloc(count * sizeof(*matches));
    if (matches == NULL) return 1;
    status = pcge_search(catalog, "caja", 4, matches, count, &count);
    /* Usar matches[0..count) si status == PCGE_OK. */
    free(matches); /* Los registros apuntados no se liberan. */
    return status == PCGE_OK ? 0 : 1;
}
```

## Contrato de consulta

`pcge_get` conserva códigos como texto y compara exactamente. `"0101"`, `" 101"` y `"101"` son claves distintas. Un código desconocido produce `PCGE_NOT_FOUND`; consultar el padre de una raíz conocida produce `PCGE_OK` y `NULL`.

`pcge_children` respeta el orden documental. `pcge_ancestors` comienza por el padre inmediato. `pcge_descendants` usa recorrido DFS en preorden, sin incluir el registro consultado. Los códigos de seis dígitos tienen `PCGE_LEVEL_UNSPECIFIED`; no se inventa un sexto nivel PCGE.

`pcge_search`:

1. Valida estrictamente UTF-8, incluidos límites, sobrelongitudes y sustitutos
2. Recorta los espacios reconocidos por Python, incluidos U+001C–U+001F
3. Aplica NFKD, elimina caracteres con clase combinante canónica distinta de cero y aplica casefold completo, en ese orden
4. Busca la subcadena en código o nombre normalizado y conserva el orden documental

Así, `depósitos`, `DEPOSITOS` y la forma con acento descompuesto normalizan de forma equivalente. No se elimina toda la categoría Unicode de marcas: las de clase combinante cero se conservan. BOM y espacio de ancho cero no se recortan. Una consulta vacía o solo de espacios es un error; una consulta formada solo por marcas eliminables coincide con todos los registros, como en el proyecto de origen. El API acepta NUL embebido y no trunca la consulta. No hay límite de resultados ni búsqueda aproximada.

`pcge_anomalies_for` consulta tanto códigos afectados como códigos impresos excluidos del catálogo. Permite investigar una anomalía aunque el código no tenga registro canónico. `pcge_json` expone los bytes JSON originales embebidos para integración o auditoría, sin reserialización.

## Procedencia y límites

Los ocho JSON en `data/2019` y `data/2026` se conservan byte por byte. `data/manifest.json` fija sus SHA-256 y los de las tablas y fixtures. Los hashes PDF en `source.json` se heredan del origen: esta biblioteca no distribuye los PDF ni afirma haberlos verificado de nuevo.

La edición 2026 declara en sus metadatos de origen una fecha obligatoria de 2028-01-01. La biblioteca no selecciona una edición según la fecha y no decide qué norma corresponde a una operación.

La anomalía `70992` conserva el registro canónico “Contrato de consultoría TI” bajo `7099` y ambas apariciones documentales. No se crea `70902`. Las demás anomalías se conservan sin correcciones inventadas.

El alcance es consultar estos dos catálogos fijados. No admite cargar catálogos personalizados, editar registros, actualizarse automáticamente ni ejecutar asientos. No calcula impuestos ni determina tratamientos contables. Los metadatos y anomalías complementan los códigos; no sustituyen la revisión de la norma aplicable.

## Pruebas y regeneración

```sh
make test                 # Solo C: API, errores, todos los registros y escalares Unicode
make sanitize             # AddressSanitizer + UndefinedBehaviorSanitizer, si el compilador los ofrece
make parity               # Requiere Python 3.10+; comparación con fixtures independientes
make check-generated      # Verifica tablas e integridad, sin escribir
python3 scripts/generate.py
```

Las tablas de C están versionadas y se regeneran únicamente a partir de archivos del mismo repositorio. El generador comprueba hashes, recuentos, códigos únicos, padres y nombres antes de generar. No consulta la red ni depende de los datos Unicode de la versión local de Python.

`tests/parity.py` compara los 3.393 registros, todas sus relaciones y el orden de resultados, 44 búsquedas y las consultas de anomalías de referencia. También contrasta las 19.026 transformaciones Unicode compiladas, obtenidas recorriendo los 1.112.064 valores escalares. `data/unicode-normalization.json` contiene las fuentes oficiales Unicode y sus hashes.

En el entorno inicial, LeakSanitizer no pudo ejecutarse por la restricción de ptrace. AddressSanitizer y UndefinedBehaviorSanitizer pueden comprobarse con `ASAN_OPTIONS=detect_leaks=0 make sanitize`; ese modo no comprueba fugas.

Comprobado localmente con GCC 14.2.0 en Linux x86-64. No se afirma compatibilidad binaria entre arquitecturas ni compiladores. El ABI público se identifica mediante `PCGE_ABI_VERSION`; esta versión inicial no promete estabilidad ABI futura. Se incluyen pruebas locales, sin workflows de CI.

## Licencia

Software: Apache-2.0, con el `LICENSE` del origen preservado. Tablas Unicode: Unicode-3.0, en `licenses/LICENSE-UNICODE`. Véase `NOTICE` para atribuciones. La licencia del software no se atribuye a los textos oficiales del Estado peruano.
