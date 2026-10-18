"""Function bounds from .eh_frame_hdr / .eh_frame (every C++ function with unwind info has an FDE)."""
import struct
from dataclasses import dataclass

DW_EH_PE_omit = 0xFF
DW_EH_PE_absptr, DW_EH_PE_uleb128, DW_EH_PE_udata2, DW_EH_PE_udata4, DW_EH_PE_udata8 = 0x00, 0x01, 0x02, 0x03, 0x04
DW_EH_PE_sleb128, DW_EH_PE_sdata2, DW_EH_PE_sdata4, DW_EH_PE_sdata8 = 0x09, 0x0A, 0x0B, 0x0C
DW_EH_PE_pcrel, DW_EH_PE_datarel = 0x10, 0x30


@dataclass
class Fde:
    start: int
    size: int
    lsda: int  # 0 when absent
    personality: int  # from the CIE, 0 when absent
    personality_indirect: bool = False


def _uleb(b, p):
    r = s = 0
    while True:
        x = b[p]
        p += 1
        r |= (x & 0x7F) << s
        s += 7
        if not x & 0x80:
            return r, p


def _sleb(b, p):
    r = s = 0
    while True:
        x = b[p]
        p += 1
        r |= (x & 0x7F) << s
        s += 7
        if not x & 0x80:
            if x & 0x40:
                r -= 1 << s
            return r, p


def _read(b, p, enc, datarel=0):
    """Read an encoded pointer at p; returns (value, new p)."""
    if enc == DW_EH_PE_omit:
        return 0, p
    fmt = enc & 0x0F
    at = p
    if fmt == DW_EH_PE_absptr or fmt == DW_EH_PE_udata8:
        v, p = struct.unpack_from("<Q", b, p)[0], p + 8
    elif fmt == DW_EH_PE_sdata8:
        v, p = struct.unpack_from("<q", b, p)[0], p + 8
    elif fmt == DW_EH_PE_udata4:
        v, p = struct.unpack_from("<I", b, p)[0], p + 4
    elif fmt == DW_EH_PE_sdata4:
        v, p = struct.unpack_from("<i", b, p)[0], p + 4
    elif fmt == DW_EH_PE_udata2:
        v, p = struct.unpack_from("<H", b, p)[0], p + 2
    elif fmt == DW_EH_PE_sdata2:
        v, p = struct.unpack_from("<h", b, p)[0], p + 2
    elif fmt == DW_EH_PE_uleb128:
        v, p = _uleb(b, p)
    elif fmt == DW_EH_PE_sleb128:
        v, p = _sleb(b, p)
    else:
        raise ValueError(f"pointer encoding {enc:#x}")
    app = enc & 0x70
    if app == DW_EH_PE_pcrel:
        v += at
    elif app == DW_EH_PE_datarel:
        v += datarel
    elif app:
        raise ValueError(f"pointer application {enc:#x}")
    return v & 0xFFFFFFFFFFFFFFFF, p


def _cie(b, p):
    """Parse the CIE at p; returns (fde pointer encoding, lsda encoding, personality)."""
    length = struct.unpack_from("<I", b, p)[0]
    q = p + 8  # length + CIE id
    version = b[q]
    q += 1
    end = b.index(b"\0", q)
    aug = b[q:end].decode()
    q = end + 1
    _, q = _uleb(b, q)  # code alignment
    _, q = _sleb(b, q)  # data alignment
    if version == 1:
        q += 1
    else:
        _, q = _uleb(b, q)
    fde_enc, lsda_enc, pers, indirect = DW_EH_PE_absptr, DW_EH_PE_omit, 0, False
    if aug.startswith("z"):
        _, q = _uleb(b, q)
        for c in aug[1:]:
            if c == "R":
                fde_enc = b[q]
                q += 1
            elif c == "L":
                lsda_enc = b[q]
                q += 1
            elif c == "P":
                penc = b[q]
                indirect = bool(penc & 0x80)
                pers, q = _read(b, q + 1, penc)
            elif c in "SB":
                pass
            else:
                break
    return fde_enc, lsda_enc, pers, indirect


def parse(m):
    """All FDEs of the module, sorted by start address."""
    b = m.image
    hdr = m.eh_frame_hdr[0]
    if not hdr:
        return []
    version, frame_enc, count_enc, table_enc = b[hdr:hdr + 4]
    if version != 1:
        raise ValueError(f"eh_frame_hdr version {version}")
    p = hdr + 4
    _, p = _read(b, p, frame_enc)
    count, p = _read(b, p, count_enc)
    cies = {}
    fdes = []
    for _ in range(count):
        _, p = _read(b, p, table_enc, hdr)
        fde, p = _read(b, p, table_enc, hdr)
        cie_ptr = fde + 4 - struct.unpack_from("<I", b, fde + 4)[0]
        if cie_ptr not in cies:
            cies[cie_ptr] = _cie(b, cie_ptr)
        fde_enc, lsda_enc, pers, indirect = cies[cie_ptr]
        q = fde + 8
        start, q = _read(b, q, fde_enc)
        size, q = _read(b, q, fde_enc & 0x0F)
        lsda = 0
        if lsda_enc != DW_EH_PE_omit:
            _, q = _uleb(b, q)
            lsda, q = _read(b, q, lsda_enc)
        fdes.append(Fde(start, size, lsda, pers, indirect))
    fdes.sort(key=lambda f: f.start)
    return fdes


def landing_pads(m, fde):
    """Lee los destinos de limpieza/captura; la personality original decide tipos y acciones."""
    b, p = m.image, fde.lsda
    try:
        encoding = b[p]
        p += 1
        landing_base = fde.start
        if encoding != DW_EH_PE_omit:
            landing_base, p = _read(b, p, encoding)
        type_encoding = b[p]
        p += 1
        if type_encoding != DW_EH_PE_omit:
            _, p = _uleb(b, p)
        call_encoding = b[p]
        p += 1
        if call_encoding == DW_EH_PE_omit or call_encoding & 0xf0:
            raise ValueError("Codificación LSDA de llamadas no válida")
        length, p = _uleb(b, p)
        end = p + length
        if end > len(b):
            raise ValueError("Tabla LSDA truncada")
        pads = set()
        while p < end:
            _, p = _read(b, p, call_encoding)
            _, p = _read(b, p, call_encoding)
            landing, p = _read(b, p, call_encoding)
            _, p = _uleb(b, p)
            if landing:
                target = landing_base + landing
                if not fde.start <= target < fde.start + fde.size or target % 4:
                    raise ValueError("Destino LSDA fuera de su función")
                pads.add(target)
        if p != end:
            raise ValueError("Entrada LSDA supera su tabla")
        return sorted(pads)
    except (IndexError, struct.error) as error:
        raise ValueError("Tabla LSDA truncada") from error
