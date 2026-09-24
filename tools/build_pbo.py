#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
build_pbo.py — сборка и проверка DZP_PlayerLocker.pbo для DayZ 1.29.

Формат PBO (Arma/DayZ, без сжатия), воспроизведён по:
  * community.bistudio.com/wiki/PBO_File_Format (запись: Asciiz-имя, 5x uint32;
    первая properties-запись 'Vers' с prefix=...; завершающий блок из нулей);
  * реальным DayZ/Arma PBO-пакерам (Tyson89/RaG-DayZ-Tools pack_pbo):
      Vers-запись первой, файлы uncompressed (method=0), оригинал=размер,
      затем terminator (имя и поля нулевые), затем данные подряд,
      хвост: 0x00 + SHA1(заголовок || данные).

Проверяет критерии сборки:
  * 13 записей (1 Vers + 12 файлов, включая $PBOPREFIX$);
  * в архиве есть scripts\\4_world\\dzp_actionregistration.c;
  * prefix = DZP_PlayerLocker;
  * SHA1-подпись совпадает;
  * размер печатается в КБ.

Запуск:  python3 tools/build_pbo.py
"""

import hashlib
import struct
import sys
from pathlib import Path

VERS = 0x56657273  # 'Vers' — properties-запись
PREFIX = "DZP_PlayerLocker"

REPO_ROOT = Path(__file__).resolve().parent.parent
SRC_DIR = REPO_ROOT / "@DZP_PlayerLocker" / "src" / "DZP_PlayerLocker"
OUT_PBO = REPO_ROOT / "@DZP_PlayerLocker" / "DZP_PlayerLocker.pbo"

# (имя внутри PBO со слэшем '\', относительный путь в src/)
# Имена внутри PBO — в нижнем регистре, разделитель '\' (как в ванидных архивах).
PBO_FILES = [
    ("$PBOPREFIX$", "$PBOPREFIX$"),
    ("config.cpp", "config.cpp"),
    ("scripts\\3_game\\dzp_playerlocker.c", "Scripts/3_Game/DZP_PlayerLocker.c"),
    ("scripts\\4_world\\dzp_actionregistration.c", "Scripts/4_World/DZP_ActionRegistration.c"),
    ("scripts\\4_world\\dzp_expandstorageaction.c", "Scripts/4_World/DZP_ExpandStorageAction.c"),
    ("scripts\\4_world\\dzp_llockerpoint.c", "Scripts/4_World/DZP_LlockerPoint.c"),
    ("scripts\\4_world\\dzp_openstorageaction.c", "Scripts/4_World/DZP_OpenStorageAction.c"),
    ("scripts\\4_world\\dzp_playerstoragedata.c", "Scripts/4_World/DZP_PlayerStorageData.c"),
    ("scripts\\4_world\\dzp_storagecontainer.c", "Scripts/4_World/DZP_StorageContainer.c"),
    ("scripts\\4_world\\dzp_storagemanager.c", "Scripts/4_World/DZP_StorageManager.c"),
    ("scripts\\5_mission\\missiongameplay.c", "Scripts/5_Mission/MissionGameplay.c"),
    ("scripts\\5_mission\\missionserver.c", "Scripts/5_Mission/MissionServer.c"),
]


def read_cstr(buf: bytes, i: int):
    end = buf.index(b"\x00", i)
    return buf[i:end], end + 1


def build() -> bytes:
    entries = []
    for pbo_name, rel in PBO_FILES:
        path = SRC_DIR / rel
        if not path.is_file():
            raise SystemExit(f"ERROR: missing source file: {path}")
        entries.append((pbo_name, path.read_bytes()))

    # Детерминированный порядок как в пакерах (сортировка по имени)
    entries.sort(key=lambda e: e[0].lower())

    header = bytearray()

    # 1) Vers properties-запись (первая): пустое имя, method='Vers',
    #    orig/reserved/timestamp/datasize = 0, затем prefix=...\0 и \0-терминатор.
    header += b"\x00"
    header += struct.pack("<I", VERS)
    header += struct.pack("<IIII", 0, 0, 0, 0)
    header += b"prefix\x00" + PREFIX.encode("ascii") + b"\x00"
    header += b"\x00"

    # 2) Файловые записи: method=0 (uncompressed), OriginalSize=DataSize=размер,
    #    reserved=0, timestamp=0 (как в RaG-пакере).
    for name, data in entries:
        header += name.encode("ascii") + b"\x00"
        header += struct.pack("<IIIII", 0, len(data), 0, 0, len(data))

    # 3) Terminator: 21 байт нулей
    header += b"\x00" + struct.pack("<IIIII", 0, 0, 0, 0, 0)

    # 4) Данные файлов подряд в порядке записей
    body = bytearray(header)
    for _, data in entries:
        body += data

    # 5) Хвост: 0x00 + SHA1(заголовок || данные) — без самого хвоста
    sha = hashlib.sha1()
    sha.update(body)
    body += b"\x00" + sha.digest()

    OUT_PBO.write_bytes(bytes(body))
    return bytes(body)


def verify(payload: bytes) -> None:
    errors = []
    i = 0
    vers_seen = 0
    files_seen = 0
    prefix = None
    file_names = []

    while True:
        if i >= len(payload):
            errors.append("unexpected end of header")
            break

        name, i = read_cstr(payload, i)
        method, orig, reserved, timestamp, data_size = struct.unpack_from("<5I", payload, i)
        i += 20

        # terminator
        if name == b"" and method == 0 and orig == 0 and reserved == 0 and timestamp == 0 and data_size == 0:
            break

        if method == VERS:
            vers_seen += 1
            # properties: пары key\0value\0, конец — пустой ключ
            while True:
                key, i = read_cstr(payload, i)
                if key == b"":
                    break
                value, i = read_cstr(payload, i)
                if key == b"prefix":
                    prefix = value.decode("ascii", "replace")
            continue

        if method != 0:
            errors.append(f"entry {name!r}: unexpected packing method {method:#x}")

        file_names.append(name.decode("ascii", "replace"))
        files_seen += 1

    records = vers_seen + files_seen
    if records != 13:
        errors.append(f"expected 13 records (Vers + 12 files), got {records}")

    expected_path = "scripts\\4_world\\dzp_actionregistration.c"
    if expected_path not in file_names:
        errors.append(f"missing required entry: {expected_path}")

    if prefix != PREFIX:
        errors.append(f"prefix mismatch: {prefix!r} != {PREFIX!r}")

    # SHA1(hвсё до хвоста) == последние 20 байт; хвост начинается с 0x00
    if len(payload) < 22 or payload[-21] != 0x00:
        errors.append("bad trailer: expected 0x00 before SHA1")
    else:
        digest = payload[-20:]
        calc = hashlib.sha1(payload[:-21]).digest()
        if digest != calc:
            errors.append("SHA1 trailer mismatch")

    kb = len(payload) / 1024.0
    print(f"  records (Vers + files) : {records} (Vers={vers_seen}, files={files_seen})")
    print(f"  entry present          : scripts/4_world/dzp_actionregistration.c -> "
          f"{'OK' if expected_path in file_names else 'FAIL'}")
    print(f"  prefix                 : {prefix}")
    print(f"  size                   : {len(payload)} bytes ({kb:.1f} KB)")
    print(f"  SHA1 trailer           : "
          f"{'OK' if not any('SHA1' in e or 'trailer' in e for e in errors) else 'FAIL'}")

    if errors:
        print("VERIFY FAILED:")
        for e in errors:
            print("  -", e)
        raise SystemExit(1)
    print("VERIFY OK: 13 records, prefix=DZP_PlayerLocker, size and SHA1 OK")


def main() -> int:
    print(f"Source : {SRC_DIR}")
    print(f"Output : {OUT_PBO}")
    payload = build()
    verify(payload)
    return 0


if __name__ == "__main__":
    sys.exit(main())
