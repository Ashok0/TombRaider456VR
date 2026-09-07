"""Regenerate src/DefaultIni.h from the repo's TombRaiderVR.ini template.

The DLL writes that template out when no TombRaiderVR.ini exists next to it, so
a fresh install lands on a working, documented configuration instead of bare
defaults. The template is the single source: edit TombRaiderVR.ini, run this,
rebuild.

    python tools\\gen_default_ini.py

Run it whenever the template changes. Nothing enforces that automatically -- the
header carries the template's byte count and line count so a stale one is at
least visible in a diff.
"""
import io
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "TombRaiderVR.ini")
DST = os.path.join(ROOT, "src", "DefaultIni.h")
DELIM = "INI"

text = io.open(SRC, encoding="utf-8-sig", newline="").read().replace("\r\n", "\n")

bad = [c for c in text if ord(c) > 126]
if bad:
    sys.exit("template is not ASCII: %r" % bad[:8])
if (")" + DELIM + '"') in text:
    sys.exit("template contains the raw-string terminator; pick another delimiter")

header = '''// DefaultIni.h -- the stock TombRaiderVR.ini, embedded.
//
// GENERATED FILE. Do not edit by hand: change TombRaiderVR.ini in the repo root
// and run  python tools\\\\gen_default_ini.py
//
// The DLL writes this out when no ini exists beside it, so a fresh install gets
// the documented, working configuration rather than bare struct defaults -- the
// comments in the template carry most of what was learned tuning this thing,
// and a generated key=value dump would throw all of it away.
//
// Source: TombRaiderVR.ini, %d bytes, %d lines.
#pragma once

namespace tr {

// Newlines are LF here; the writer expands them to CRLF on the way out.
inline const char* DefaultIniText() {
    return R"%s(%s)%s";
}

} // namespace tr
''' % (len(text.encode()), text.count("\n"), DELIM, text, DELIM)

io.open(DST, "w", encoding="utf-8", newline="\n").write(header)
print("wrote %s  (%d bytes, %d lines from the template)"
      % (os.path.relpath(DST, ROOT), len(text.encode()), text.count("\n")))
