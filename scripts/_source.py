"""Lectura y validación de entradas fijadas; solo biblioteca estándar de Python."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def strict(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"Clave JSON duplicada: {key}")
        result[key] = value
    return result

def read(path):
    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=strict)

def load():
    manifest = read(ROOT / "data/manifest.json")
    for name, expected in manifest["sha256"].items():
        actual = hashlib.sha256((ROOT / name).read_bytes()).hexdigest()
        if actual.lower() != expected.lower():
            raise ValueError(f"SHA-256 diferente para {name}: {actual}")
    unicode = read(ROOT / "data/unicode-normalization.json")
    mapping = dict(unicode["mappings"])
    assert list(mapping) == sorted(mapping) and len(mapping) == len(unicode["mappings"])
    assert all(0 <= c <= 0x10FFFF and not 0xD800 <= c <= 0xDFFF for c in mapping)
    normalize = lambda text: "".join(mapping.get(ord(c), c) for c in text)
    catalogs = []
    for year in ("2019", "2026"):
        docs = {name: read(ROOT / "data" / year / f"{name}.json")
                for name in ("entries", "metadata", "source", "anomalies")}
        entries = docs["entries"]
        codes = [e["code"] for e in entries]
        assert len(set(codes)) == len(codes)
        assert docs["metadata"]["entry_count"] == len(entries)
        assert docs["metadata"]["pcge_version"] == year
        digest = hashlib.sha256((ROOT / "data" / year / "entries.json").read_bytes()).hexdigest()
        assert digest.lower() == docs["source"]["dataset_sha256"].lower()
        for entry in entries:
            code, parent = entry["code"], entry["parent_code"]
            assert set(entry) == {"code", "name", "parent_code"}
            assert isinstance(code, str) and code.isascii() and code.isdigit() and 1 <= len(code) <= 6
            assert entry["name"].strip()
            assert parent == (code[:-1] if len(code) > 1 else None)
            assert parent is None or parent in codes
        docs.update(year=year, search_names=[normalize(e["name"]) for e in entries],
                    by_code=sorted(range(len(entries)), key=lambda i: codes[i]))
        catalogs.append(docs)
    return unicode, catalogs

def finish(outputs):
    parser = argparse.ArgumentParser(description="Regenera tablas desde datos fijados, sin red")
    parser.add_argument("--check", action="store_true", help="comprueba sin modificar archivos")
    args = parser.parse_args()
    for name, text in outputs.items():
        path = ROOT / name
        content = text.encode("utf-8")
        if args.check:
            if not path.exists() or path.read_bytes() != content:
                raise SystemExit(f"Archivo generado desactualizado: {name}")
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)
    print(f"{len(outputs)} archivos generados verificados" if args.check else f"{len(outputs)} archivos generados")
