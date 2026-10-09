#!/usr/bin/env python3
"""Paridad independiente: catálogos completos, navegación, búsqueda y Unicode.

Uso C:    python3 tests/parity.py build/pcge build/test_unicode
Uso Rust: python3 tests/parity.py target/debug/examples/pcge target/debug/examples/unicode_dump
Solo biblioteca estándar de Python 3.10+; no usa referencias fuera del repositorio.
"""
from __future__ import annotations
import collections
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
if len(sys.argv) not in (2, 3):
    raise SystemExit(__doc__)
cli = Path(sys.argv[1]).resolve()
unicode_cli = Path(sys.argv[2]).resolve() if len(sys.argv) == 3 else cli.with_name("test_unicode")
fixtures = json.loads((ROOT / "tests/golden-fixtures.json").read_text("utf-8"))
manifest = json.loads((ROOT / "data/manifest.json").read_text("utf-8"))
for path, expected in manifest["sha256"].items():
    assert hashlib.sha256((ROOT / path).read_bytes()).hexdigest() == expected, path

def run(edition, command, *args):
    result = subprocess.run([str(cli), edition, command, *args], check=True, capture_output=True)
    return json.loads(result.stdout.decode("utf-8"))

total = search_total = anomaly_total = 0
for edition, fixture in fixtures["editions"].items():
    source = json.loads((ROOT / "data" / edition / "entries.json").read_text("utf-8"))
    actual = run(edition, "dump")
    assert actual == source, f"{edition}: catálogo completo o texto/orden diferente"
    assert [e["code"] for e in actual] == fixture["ordered_codes"]
    assert len(actual) == fixture["entry_count"]
    counts = {str(k): v for k, v in collections.Counter(len(e["code"]) for e in actual).items()}
    assert counts == fixture["code_length_counts"]
    assert run(edition, "nav") == fixture["navigation"], f"{edition}: relaciones diferentes"
    for name in ("metadata", "source", "anomalies"):
        expected = json.loads((ROOT / "data" / edition / f"{name}.json").read_text("utf-8"))
        assert run(edition, name) == expected
    for case in fixture["search"]:
        assert run(edition, "search", case["query"]) == case["codes"], (edition, case["query"])
        search_total += 1
    for case in fixture["anomalies_for"]:
        assert run(edition, "anomalies-for", case["code"]) == case["ids"], (edition, case["code"])
        anomaly_total += 1
    for query in fixtures["invalid_search_queries"]:
        result = subprocess.run([str(cli), edition, "search", query], capture_output=True)
        assert result.returncode != 0, (edition, repr(query), "se esperaba error")
    # El recorte usa el conjunto de espacios Python y no elimina ZWSP/BOM.
    for case in fixtures["trim"]:
        assert run(edition, "search", case["input"]) == run(edition, "search", case["output"])
    total += len(actual)

for edition in fixtures["invalid_versions"]:
    # Los tipos dinámicos no son representables por la CLI; las APIs nativas usan enum.
    if not isinstance(edition, str):
        continue
    result = subprocess.run([str(cli), edition, "dump"], capture_output=True)
    assert result.returncode != 0, (edition, "se esperaba edición no disponible")

# Los comprobadores recorren todos los valores escalares y emiten los cambiados.
command = [str(unicode_cli)]
if unicode_cli.name == "test_unicode":
    command.append("--dump")
output = subprocess.run(command, capture_output=True, check=True).stdout.decode("ascii")
actual_mapping = []
for line in output.splitlines():
    code, value = line.split("\t")
    actual_mapping.append([int(code, 16), bytes.fromhex(value).decode("utf-8")])
expected_mapping = json.loads((ROOT / "data/unicode-normalization.json").read_text("utf-8"))["mappings"]
assert actual_mapping == expected_mapping, "tabla Unicode compilada diferente de la referencia"
mapping = dict(actual_mapping)
for case in fixtures["normalization"]:
    actual = "".join(mapping.get(ord(c), c) for c in case["input"])
    assert actual == case["output"], repr(case["input"])
print(f"Paridad correcta: {total} registros y navegaciones, {search_total} búsquedas, "
      f"{anomaly_total} consultas de anomalías; Unicode 16 completo (1.112.064 escalares, "
      f"{len(actual_mapping)} transformaciones).")
