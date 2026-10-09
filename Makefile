CC ?= cc
AR ?= ar
CFLAGS ?= -O2 -std=c99 -Wall -Wextra -Wpedantic
CPPFLAGS ?=
LDFLAGS ?=
LDLIBS ?=
PREFIX ?= /usr/local
BUILD ?= build
INCLUDES = -Iinclude

.PHONY: all test sanitize parity check-generated clean install
all: $(BUILD)/libpcge.a $(BUILD)/pcge

$(BUILD):
	mkdir -p $@

$(BUILD)/pcge.o: src/pcge.c src/data.inc src/unicode_data.inc include/pcge.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD)/libpcge.a: $(BUILD)/pcge.o
	$(AR) rcs $@ $^

$(BUILD)/pcge: examples/pcge.c $(BUILD)/libpcge.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $< $(BUILD)/libpcge.a $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD)/test_pcge: tests/test_pcge.c $(BUILD)/libpcge.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $< $(BUILD)/libpcge.a $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD)/test_unicode: tests/test_unicode.c src/pcge.c src/data.inc src/unicode_data.inc include/pcge.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $< $(LDFLAGS) $(LDLIBS) -o $@

test: $(BUILD)/test_pcge $(BUILD)/test_unicode
	$(BUILD)/test_pcge
	$(BUILD)/test_unicode

# Los comprobadores de mantenimiento usan Python 3.10+; la compilación normal no.
parity: all test
	python3 tests/parity.py $(BUILD)/pcge

check-generated:
	python3 scripts/generate.py --check

sanitize:
	$(MAKE) BUILD=build/sanitize CFLAGS='-O1 -g -std=c99 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' test

install: $(BUILD)/libpcge.a
	install -d $(DESTDIR)$(PREFIX)/include $(DESTDIR)$(PREFIX)/lib $(DESTDIR)$(PREFIX)/share/licenses/pcge
	install -m 644 include/pcge.h $(DESTDIR)$(PREFIX)/include/pcge.h
	install -m 644 $(BUILD)/libpcge.a $(DESTDIR)$(PREFIX)/lib/libpcge.a
	install -m 644 LICENSE NOTICE licenses/LICENSE-UNICODE $(DESTDIR)$(PREFIX)/share/licenses/pcge/

clean:
	rm -rf $(BUILD)
