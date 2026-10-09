/* Pruebas de internas Unicode sin ampliar el ABI público. */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/pcge.c"

static size_t encode(uint32_t cp, char *out) {
    unsigned char *s = (unsigned char *)out;
    if (cp < 0x80) { s[0] = (unsigned char)cp; return 1; }
    if (cp < 0x800) {
        s[0] = (unsigned char)(0xc0u | (cp >> 6)); s[1] = (unsigned char)(0x80u | (cp & 63)); return 2;
    }
    if (cp < 0x10000) {
        s[0] = (unsigned char)(0xe0u | (cp >> 12)); s[1] = (unsigned char)(0x80u | ((cp >> 6) & 63));
        s[2] = (unsigned char)(0x80u | (cp & 63)); return 3;
    }
    s[0] = (unsigned char)(0xf0u | (cp >> 18)); s[1] = (unsigned char)(0x80u | ((cp >> 12) & 63));
    s[2] = (unsigned char)(0x80u | ((cp >> 6) & 63)); s[3] = (unsigned char)(0x80u | (cp & 63)); return 4;
}
int main(int argc, char **argv) {
    uint32_t cp;
    size_t changed = 0, total = 0;
    int dump = argc == 2 && strcmp(argv[1], "--dump") == 0;
    for (cp = 0; cp <= 0x10ffff; ++cp) {
        char bytes[4], *normalized;
        size_t length, i = 0, normalized_length;
        uint32_t decoded;
        const pcge_string *expected;
        if (cp >= 0xd800 && cp <= 0xdfff) continue;
        length = encode(cp, bytes);
        assert(decode(bytes, length, &i, &decoded) == PCGE_OK && i == length && decoded == cp);
        assert(normalize(bytes, 0, length, &normalized, &normalized_length) == PCGE_OK);
        expected = mapping(cp);
        if (expected == NULL) assert(normalized_length == length && memcmp(normalized, bytes, length) == 0);
        else {
            assert(normalized_length == expected->len && memcmp(normalized, expected->data, normalized_length) == 0);
            ++changed;
            if (dump) {
                printf("%x\t", (unsigned int)cp);
                for (i = 0; i < normalized_length; ++i) printf("%02x", (unsigned int)(unsigned char)normalized[i]);
                putchar('\n');
            }
        }
        free(normalized);
        ++total;
    }
    assert(total == 1112064 && changed == 19026);
    if (!dump) printf("Unicode 16: %zu escalares UTF-8 y %zu transformaciones correctos\n", total, changed);
    return ferror(stdout) ? 1 : 0;
}
