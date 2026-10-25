"""Converte o libsre_imports.json do lift_prx.py ({biblioteca: {nid: stub}}) no
formato que o ppu_lifter.py espera em --hle-stubs ([{library, nid, stub}], hex
minúsculo).

uso: python scripts/libsre_stubs_json.py fw/lle/libsre_imports.json fw/lle/libsre_hle_stubs.json"""
import json
import sys

ent, sai = sys.argv[1], sys.argv[2]
imp = json.load(open(ent, encoding='utf-8'))
out = [{'library': lib, 'nid': nid.lower(), 'stub': stub.lower()}
       for lib, nids in imp.items() for nid, stub in nids.items()]
json.dump(out, open(sai, 'w', encoding='utf-8'), indent=2)
print(len(out), 'stubs ->', sai)
