"""Perfiles de instrucciones AOT ligados a la imagen exacta del módulo."""
import hashlib
import json
import struct
import re


def apply(module, path):
    with open(path, encoding="utf-8") as source:
        profile = json.load(source)
    if profile['module'] != module.name or profile['sha256'] != hashlib.sha256(module.image).hexdigest():
        raise ValueError('El perfil AOT no corresponde a esta imagen del módulo')
    pending = []
    seen = set()
    for patch in profile['instructions']:
        offset, expected, replacement = (int(patch[key], 0) for key in ('offset', 'expected', 'replacement'))
        if offset in seen or offset % 4 or not module.in_text(offset) or not module.in_text(offset + 3):
            raise ValueError('Dirección de parche AOT inválida o repetida')
        if module.u32(offset) != expected or not 0 <= replacement <= 0xffffffff:
            raise ValueError(f'Instrucción AOT inesperada en {offset:#x}')
        seen.add(offset)
        pending.append((offset, expected, replacement))
    hooks = {}
    for hook in profile.get("entry_hooks", []):
        offset = int(hook["offset"], 0)
        symbol = hook["symbol"]
        if offset % 4 or not module.in_text(offset) or offset in hooks or not re.fullmatch(r"[A-Za-z_][A-Za-z_0-9]*", symbol):
            raise ValueError("Hook de entrada AOT inválido")
        hooks[offset] = symbol
    wrappers = {}
    for wrapper in profile.get("wrappers", []):
        offset, symbol = int(wrapper["offset"], 0), wrapper["symbol"]
        if offset % 4 or not module.in_text(offset) or offset in wrappers or offset in hooks or not re.fullmatch(r"[A-Za-z_][A-Za-z_0-9]*", symbol):
            raise ValueError("Wrapper AOT inválido")
        wrappers[offset] = symbol
    for offset, expected, replacement in pending:
        struct.pack_into('<I', module.image, offset, replacement)
    module.instruction_patches = pending
    module.entry_hooks = hooks
    module.function_wrappers = wrappers
