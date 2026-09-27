"""Check Persian UI strings for recurring spelling and bidirectional issues.

Run from the repository root with: python3 client/translations/check_fa_ir.py
"""

from collections import Counter
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET


CATALOG = Path(__file__).with_name("amneziavpn_fa_IR.ts")
PLACEHOLDER = re.compile(r"%(?:[1-9][0-9]*|n)")
BROKEN_TEXT = ("راهانداز", "نرمافزار", "ویپی", "تونل تقسیم", "تقسیم تونل", "تونل‌سازی تقسیم")
# These spellings are valid Persian, but in this app they were hard to read at
# the small UI font size. Prefer a clear phrase with ordinary word spacing.
HARD_TO_READ_UI_TERMS = ("اشتراک‌گذاری", "پنهان‌سازی", "ارائه‌دهندهٔ")
BAD_DIRECTION_MARKS = "\u200e\u200f\u202a\u202b\u202c"
ISOLATE_STARTS = "\u2066\u2067\u2068"
ISOLATE_END = "\u2069"


def main() -> int:
    root = ET.parse(CATALOG).getroot()
    errors = []
    for context in root.findall("context"):
        name = context.findtext("name") or "?"
        for message in context.findall("message"):
            source = message.findtext("source") or ""
            translation = message.find("translation")
            if translation is None:
                errors.append(f"{name}: missing translation for {source!r}")
                continue
            rendered = "".join(translation.itertext())
            label = f"{name}: {source[:65]!r}"
            if Counter(PLACEHOLDER.findall(source)) != Counter(PLACEHOLDER.findall(rendered)):
                errors.append(f"{label}: Qt placeholders differ")
            if any(mark in rendered for mark in BAD_DIRECTION_MARKS):
                errors.append(f"{label}: outdated direction mark in Persian text")
            if sum(rendered.count(ch) for ch in ISOLATE_STARTS) != rendered.count(ISOLATE_END):
                errors.append(f"{label}: unbalanced bidirectional isolates")
            for term in BROKEN_TEXT + HARD_TO_READ_UI_TERMS:
                if term in rendered:
                    errors.append(f"{label}: broken or unclear term {term!r}")

    for error in errors:
        print(error, file=sys.stderr)
    print(f"Checked {len(root.findall('.//message'))} Persian messages; {len(errors)} issues")
    return bool(errors)


if __name__ == "__main__":
    sys.exit(main())
