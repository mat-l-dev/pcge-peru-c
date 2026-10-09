#!/usr/bin/env python3
"""Genera C99 estático desde los JSON fijados; no requiere dependencias ni red."""
from _source import ROOT, finish, load


def q(text):
    return '"' + ''.join('\\"' if b == 34 else '\\\\' if b == 92 else chr(b)
                         if 32 <= b < 127 else f'\\{b:03o}' for b in text.encode('utf-8')) + '"'

def s(text):
    return '{NULL, 0}' if text is None else '{'+q(text)+', '+str(len(text.encode('utf-8')))+'}'

unicode, catalogs = load()
lines = ['/* Generado por scripts/generate.py. No editar manualmente. */']
for d in catalogs:
    year = d['year']
    for doc in ('entries','metadata','source','anomalies'):
        raw = (ROOT / 'data' / year / (doc+'.json')).read_bytes() + b'\0'
        lines.append(f'static const unsigned char json_{year}_{doc}[] = {{')
        # Constantes enteras evitan el límite ISO de longitud de literales C.
        for offset in range(0, len(raw), 24):
            lines.append(','.join(str(b) for b in raw[offset:offset+24])+',')
        lines.append('};')
    lines.append(f'static const pcge_entry entries_{year}[] = {{')
    lines.extend('{'+', '.join(s(e[k]) for k in ('code','name','parent_code'))+'},' for e in d['entries'])
    lines.append('};')
    lines.append(f'static const pcge_string names_{year}[] = {{')
    lines.extend(s(name)+',' for name in d['search_names'])
    lines.append('};')
    lines.append(f'static const size_t index_{year}[] = {{'+','.join(map(str,d['by_code']))+'};')
    for i,a in enumerate(d['anomalies']):
        lines.append(f'static const pcge_string codes_{year}_{i}[] = {{'+','.join(s(code) for code in a.get('codes',[a.get('code')]))+'};')
        lines.append(f'static const pcge_occurrence occurrences_{year}_{i}[] = {{')
        for o in a['occurrences']:
            lines.append('{'+','.join(str(o[k]) for k in ('occurrence_index','pdf_page','printed_page'))+','+
                         ','.join(s(o[k]) for k in ('printed_code','printed_name','printed_parent_code','disposition'))+'},')
        lines.append('};')
    lines.append(f'static const pcge_anomaly anomalies_{year}[] = {{')
    for i,a in enumerate(d['anomalies']):
        count = len(a.get('codes',[a.get('code')]))
        lines.append('{'+s(a['id'])+','+s(a['type'])+f',codes_{year}_{i},{count},'+
                     ','.join(s(a[k]) for k in ('status','description','decision','confirmation_no_invented_code'))+
                     f',occurrences_{year}_{i},'+str(len(a['occurrences']))+'},')
    lines.append('};')
    lines.append(f'static const pcge_catalog catalog_{year} = {{')
    lines.append(f'entries_{year}, names_{year}, index_{year}, {len(d["entries"])},')
    m = d['metadata']
    lines.append('{'+s(year)+f',{m["schema_version"]},{m["dataset_revision"]},{m["entry_count"]}'+'},')
    p = d['source']
    lines.append('{'+','.join(s(p[k]) for k in ('title','authority','resolution','resolution_date','publication_date','mandatory_effective_date',
             'resolution_url','source_filename','source_sha256','dataset_sha256','catalog_chapter'))+','+
             ','.join('{'+str(p[k]['first'])+','+str(p[k]['last'])+'}' for k in ('catalog_pdf_pages','catalog_printed_pages'))+'},')
    lines.append(f'anomalies_{year}, {len(d["anomalies"])}, {{')
    lines.append(','.join('{'+f'(const char *)json_{year}_{doc}, sizeof(json_{year}_{doc}) - 1'+'}' for doc in ('entries','metadata','source','anomalies'))+'}};')
u = ['/* Generado desde Unicode '+unicode['unicode_version']+'. Véase licenses/LICENSE-UNICODE. */',
     '#define UNICODE_VERSION '+q(unicode['unicode_version']),
     'static const uint32_t unicode_whitespace[] = {'+','.join(map(str,unicode['whitespace_codepoints']))+'};',
     'static const unicode_mapping unicode_mappings[] = {']
u.extend('{'+str(code)+', '+s(replacement)+'},' for code,replacement in unicode['mappings'])
u.append('};')
finish({'src/data.inc':'\n'.join(lines)+'\n','src/unicode_data.inc':'\n'.join(u)+'\n'})
