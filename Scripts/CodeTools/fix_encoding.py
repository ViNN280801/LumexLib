#!/usr/bin/env python3
"""
Universal encoding fixer for Russian text files
Handles various encoding issues including:
- Windows-1251/CP866/KOI8-R files incorrectly saved as UTF-8
- Corrupted UTF-8 with replacement characters
- Mixed encodings
- BOM issues
"""

import re
import os
import sys


def detect_encoding_issues(content):
    """Detect what kind of encoding issues we have"""
    issues = []

    # Check for UTF-8 replacement characters
    if "Fffd" in content:
        issues.append("utf8_replacement_chars")

    # Check for common corrupted UTF-8 patterns
    if "пїЅ" in content:
        issues.append("corrupted_utf8_patterns")

    # Check for mixed encodings
    if re.search(r"[А-Яа-я].*[^\x00-\x7F]", content):
        issues.append("mixed_encodings")

    # Check for BOM
    if content.startswith("\ufeff"):
        issues.append("utf8_bom")

    return issues


def try_decode_as_encoding(data, encoding):
    """Try to decode bytes as specific encoding"""
    try:
        decoded = data.decode(encoding, errors="strict")
        return decoded, True
    except UnicodeDecodeError:
        try:
            decoded = data.decode(encoding, errors="ignore")
            return decoded, False
        except:
            return None, False


def is_result_clean(text):
    """Check if the result contains any corrupted characters (кракозябры)"""
    # List of common corrupted patterns that indicate encoding issues
    corrupted_patterns = [
        # UTF-8 replacement characters
        "пїЅ",  # Common UTF-8 corruption pattern
        # Windows-1251 -> UTF-8 corruption patterns
        "Р°",
        "Р±",
        "Р²",
        "Р³",
        "Рґ",
        "Рµ",
        "С'",
        "Р·",
        "Рё",
        "Р№",
        "Рє",
        "Р»",
        "Рј",
        "РЅ",
        "Рѕ",
        "Рї",
        "СЂ",
        "СЃ",
        "С‚",
        "Сѓ",
        "С„",
        "С…",
        "С†",
        "С‡",
        "С€",
        "С‰",
        "СЉ",
        "С‹",
        "СЊ",
        "СЌ",
        "СЋ",
        "СЏ",
        "Рђ",
        "Р'",
        "Р'",
        "Рђ",
        'Р"',
        "Р•",
        "Р–",
        "Р—",
        "Р�",
        "Р™",
        "Рљ",
        "Р›",
        "Рњ",
        "Рќ",
        "Рћ",
        "Рџ",
        "Р ",
        "РЎ",
        "Р¢",
        "РЈ",
        "Р¤",
        "РҐ",
        "Р¦",
        "Р§",
        "РЁ",
        "Р©",
        "РЄ",
        "Р«",
        "РЬ",
        "РЄ",
        "РЮ",
        "РЇ",
        # CP866 -> UTF-8 corruption patterns
        "РђРѕ",
        "РђР±",
        "РђРІ",
        "РђРі",
        "РђРґ",
        "РђРµ",
        "РђС'",
        "РђР·",
        "РђРё",
        "РђР№",
        "РђРє",
        "РђР»",
        "РђРј",
        "РђРЅ",
        "РђРѕ",
        "РђРї",
        "РђСЂ",
        "РђСЃ",
        "РђС‚",
        "РђСѓ",
        # KOI8-R -> UTF-8 corruption patterns
        "РЎС‰",
        "РЎР±",
        "РЎРІ",
        "РЎРі",
        "РЎРґ",
        "РЎРµ",
        "РЎС'",
        "РЎР·",
        "РЎРё",
        "РЎР№",
        "РЎРє",
        "РЎР»",
        "РЎРј",
        "РЎРЅ",
        "РЎРѕ",
        "РЎРї",
        "РЎСЂ",
        "РЎСЃ",
        "РЎС‚",
        "РЎСѓ",
        # ISO-8859-5 -> UTF-8 corruption patterns
        "РђС™",
        "РђС›",
        "РђСќ",
        "РђСџ",
        "РђС'",
        "РђСѓ",
        "РђСµ",
        "РђС·",
        "РђС№",
        "РђС»",
        # Double-encoded corruption patterns
        "Р РЎР‚Р С™Р РЎР‚Р С›",
        "Р РЎР‚Р С™Р РЎР‚Р СќР РЎР‚Р С™",
        "РЎР‚Р С™РЎР‚Р С›",
        "РЎР‚Р С™РЎР‚Р СќР РЎР‚Р С™",
        # Triple-encoded patterns
        "РЎРѓР С›РЎРѓР СќР РЎР‚Р С™",
        # Mixed corruption patterns
        "РІС‹РµРјРѕРіСѓС‚",
        "РґР°РЅРЅС‹С…",
        "РїСЂРёРЅСЏС‚С‹С…",
        "РєРѕС‚РѕСЂС‹Рµ",
        "РїР°РєРµС‚С‹",
        "РїСЂРѕС‚РѕРєРѕР»",
        # Specific patterns from your files
        "пїЅпїЅпїЅпїЅпїЅ",
        "пїЅпїЅпїЅпїЅпїЅпїЅ",
        "┐╜",
        "©╫",
        "яПН",
        "пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ",
        "РїР°РєРµС‚",
        "РїРѕРґРґРµСЂР¶РёРІР°СЋС‰РёР№",
        "РЅРµРѕР±С…РѕРґРёРјС‹Р№",
        "Р±СѓС„РµСЂРµ",
        "РїСЂРёРЅСЏС‚С‹С…",
        "РґР°РЅРЅС‹С…",
        "РІС‹РґРµР»СЏСЋС‚СЃСЏ",
        "РґРµРєРѕРґРёСЂСѓСЋС‚СЃСЏ",
        "РїРѕРјРµС‰Р°СЋС‚СЃСЏ",
        "СЃРїРёСЃРѕРє",
        "Р'РѕР·РІСЂР°С‰Р°РµС‚СЃСЏ",
        "С‡РёСЃР»Рѕ",
        "РїСЂРёРЅСЏС‚С‹С…",
        "РїР°РєРµС‚РѕРІ",
        "РЎРѕРґРµСЂР¶РёС‚",
        "СЃС‚Р°С‚РёСЃС‚РёРєСѓ",
        "РѕР±РјРµРЅРѕРІ",
        "РєРѕРЅС‚СЂРѕР»СЊРЅР°СЏ",
        "СЃСѓРјРјР°",
        "Р¤Р»Р°РіРё",
        "СѓРїСЂР°РІР»РµРЅРёСЏ",
        "Р±Р»РѕРєРѕРј",
        # Byte-level corruption patterns
        "\xef\xbf\xbd",  # UTF-8 replacement character bytes
        "\ufffd",  # Unicode replacement character
        # Sequences that indicate encoding problems
        "РЎР‚",
        "РЎРѓ",
        "РЎР‹",
        "РЎР†",
        "РЎР‡",
        "РЎРё",
        "РЎР°",
        "РЎР±",
    ]

    for pattern in corrupted_patterns:
        if pattern in text:
            return False

    # Additional check: if we have Russian text, it should be readable
    # Check for sequences of repeated similar characters (often indicates corruption)
    import re

    if re.search(r"[А-Яа-я]{2,}", text):  # If we have Russian text
        # Check for suspicious repeated patterns
        if re.search(r"([А-Яа-я])\1{3,}", text):  # Same character repeated 4+ times
            return False
        # Check for sequences that look like corrupted UTF-8
        if re.search(
            r"[А-Яа-я][^\w\s]{2,}[А-Яа-я]", text
        ):  # Russian chars separated by multiple non-word chars
            return False

    return True


def fix_encoding_issues(file_path):
    """Main function to fix encoding issues"""

    # Read file as binary
    try:
        with open(file_path, "rb") as f:
            data = f.read()
    except Exception as e:
        print(f"Error reading file: {e}")
        return False

    # Create backup
    backup_path = f"{file_path}.backup"
    try:
        with open(backup_path, "wb") as f:
            f.write(data)
        print(f"Created backup: {backup_path}")
    except Exception as e:
        print(f"Error creating backup: {e}")
        return False

    # Try to decode as UTF-8 first to see current state
    try:
        current_content = data.decode("utf-8", errors="replace")
        issues = detect_encoding_issues(current_content)
        print(
            f"Current file issues: {', '.join(issues) if issues else 'None detected'}"
        )
    except:
        current_content = None
        issues = ["unknown_encoding"]

    # Common encodings to try
    encodings = ["windows-1251", "cp866", "koi8-r", "iso-8859-5"]

    # Strategy 1: Try direct decoding as different encodings (like VSCode does)
    print("\nTrying direct encoding detection (VSCode method)...")
    for encoding in encodings:
        decoded, strict = try_decode_as_encoding(data, encoding)
        if decoded:
            # Check if we got readable Russian text
            russian_chars = sum(1 for c in decoded if 0x0400 <= ord(c) <= 0x04FF)
            total_chars = len([c for c in decoded if c.isalpha()])

            if russian_chars > 0 and total_chars > 0:
                russian_ratio = russian_chars / total_chars
                print(
                    f"  {encoding}: {russian_chars} Russian chars ({russian_ratio:.1%})"
                )

                if russian_ratio > 0.1:  # More than 10% Russian characters
                    # STRICT CHECK: NO кракозябры allowed!
                    if is_result_clean(decoded):
                        # This is the key: if we can decode as Windows-1251 and get Russian text,
                        # it means the file was originally in that encoding but got mangled
                        # Just like VSCode does - interpret the bytes as the original encoding
                        try:
                            with open(file_path, "w", encoding="utf-8") as f:
                                f.write(decoded)
                            print(
                                f"✓ Successfully fixed using {encoding} (VSCode method)"
                            )
                            return True
                        except Exception as e:
                            print(f"  Error writing file: {e}")
                    else:
                        print(f"  {encoding}: Result contains кракозябры, REJECTED")

    # Strategy 2: Try to fix corrupted UTF-8 by intelligent byte analysis
    print("\nTrying intelligent byte analysis...")

    # Analyze the byte patterns to understand the corruption
    # Look for sequences that suggest Windows-1251 was incorrectly interpreted as UTF-8

    # Count different byte patterns
    byte_counts = {}
    for i in range(len(data) - 1):
        byte_pair = data[i : i + 2]
        byte_counts[byte_pair] = byte_counts.get(byte_pair, 0) + 1

    # Look for patterns that suggest UTF-8 corruption
    suspicious_patterns = []
    for byte_pair, count in byte_counts.items():
        if count > 5:  # Pattern appears multiple times
            if byte_pair[0] == 0xD0 and byte_pair[1] >= 0x80:
                suspicious_patterns.append(byte_pair)

    if suspicious_patterns:
        print(f"  Found {len(suspicious_patterns)} suspicious byte patterns")

        # Try to reconstruct by interpreting the data as if it was originally
        # in a different encoding but got mangled through UTF-8 conversion

        # Method 1: Try to extract the original bytes from UTF-8 sequences
        try:
            # If we see many 0xD0 bytes, this suggests Windows-1251 was interpreted as UTF-8
            if data.count(b"\xd0") > len(data) * 0.1:  # More than 10% are 0xD0
                print("  Detected potential Windows-1251 misinterpreted as UTF-8")

                # Extract the second byte of each UTF-8 sequence
                reconstructed_bytes = bytearray()
                i = 0
                while i < len(data):
                    if data[i] == 0xD0 and i + 1 < len(data):
                        # This looks like a UTF-8 sequence, extract the second byte
                        second_byte = data[i + 1]
                        if 0x80 <= second_byte <= 0xFF:
                            reconstructed_bytes.append(second_byte)
                        i += 2
                    else:
                        # Keep ASCII characters as-is
                        if data[i] < 0x80:
                            reconstructed_bytes.append(data[i])
                        i += 1

                if len(reconstructed_bytes) > 0:
                    # Try to decode as Windows-1251
                    try:
                        decoded = bytes(reconstructed_bytes).decode(
                            "windows-1251", errors="ignore"
                        )
                        if any(
                            0x0400 <= ord(c) <= 0x04FF for c in decoded
                        ) and is_result_clean(decoded):
                            with open(file_path, "w", encoding="utf-8") as f:
                                f.write(decoded)
                            print("✓ Successfully reconstructed using byte analysis")
                            return True
                    except:
                        pass
        except Exception as e:
            print(f"  Byte analysis failed: {e}")

    # Method 2: Try to fix UTF-8 replacement characters by context
    if b"\xef\xbf\xbd" in data:  # UTF-8 replacement character
        print("  Found UTF-8 replacement characters, attempting context-based fix...")

        try:
            # Try to interpret the file as if it was originally in Windows-1251
            # but got corrupted through UTF-8 conversion
            fixed_data = bytearray()
            i = 0
            while i < len(data):
                if i + 2 < len(data) and data[i : i + 3] == b"\xef\xbf\xbd":
                    # This is a replacement character, try to reconstruct
                    # Look at surrounding context to guess the original character
                    if i > 0 and i + 3 < len(data):
                        # Try to interpret the next byte as Windows-1251
                        next_byte = data[i + 3]
                        if 0x80 <= next_byte <= 0xFF:
                            fixed_data.append(next_byte)
                            i += 4
                            continue
                    i += 3
                else:
                    fixed_data.append(data[i])
                    i += 1

            if len(fixed_data) > 0:
                # Try to decode as Windows-1251
                try:
                    decoded = bytes(fixed_data).decode("windows-1251", errors="ignore")
                    if any(
                        0x0400 <= ord(c) <= 0x04FF for c in decoded
                    ) and is_result_clean(decoded):
                        with open(file_path, "w", encoding="utf-8") as f:
                            f.write(decoded)
                        print("✓ Successfully fixed using context-based reconstruction")
                        return True
                except:
                    pass
        except Exception as e:
            print(f"  Context-based fix failed: {e}")

    # Strategy 3: Try to reconstruct from byte patterns
    print("\nTrying byte pattern reconstruction...")

    # Look for patterns like 0xD0 0xD0 0xD0... which suggest Windows-1251
    # was interpreted as UTF-8
    if b"\xd0\xd0" in data:
        print("  Found Windows-1251 byte patterns, attempting reconstruction...")

        # Try to interpret every other byte as Windows-1251
        # This is a heuristic for when UTF-8 interpretation went wrong
        try:
            # Extract bytes that might be Windows-1251
            potential_windows1251 = bytearray()
            i = 0
            while i < len(data):
                if data[i] == 0xD0 and i + 1 < len(data):
                    potential_windows1251.append(data[i + 1])
                    i += 2
                else:
                    i += 1

            if potential_windows1251:
                # Try to decode as Windows-1251
                decoded = bytes(potential_windows1251).decode(
                    "windows-1251", errors="ignore"
                )
                if any(0x0400 <= ord(c) <= 0x04FF for c in decoded) and is_result_clean(
                    decoded
                ):
                    # Reconstruct the file with proper encoding
                    with open(file_path, "w", encoding="utf-8") as f:
                        f.write(decoded)
                    print("✓ Successfully reconstructed from byte patterns")
                    return True
        except Exception as e:
            print(f"  Byte reconstruction failed: {e}")

    # Strategy 4: Try to fix the specific corruption pattern
    print("\nTrying specific corruption pattern fix...")

    # The file appears to have Windows-1251 bytes that were incorrectly
    # interpreted as UTF-8, creating sequences like 0xEF 0xBF 0xBD
    # Let's try to reverse this process

    try:
        # Convert the corrupted UTF-8 back to bytes, then try to interpret
        # as if it was originally Windows-1251
        corrupted_utf8 = data.decode("utf-8", errors="replace")

        # Replace the replacement characters with their original byte values
        # This is a heuristic approach
        fixed_bytes = bytearray()
        i = 0
        while i < len(data):
            if (
                i + 2 < len(data)
                and data[i] == 0xEF
                and data[i + 1] == 0xBF
                and data[i + 2] == 0xBD
            ):
                # This is a replacement character, try to reconstruct
                # Look ahead for more context
                if i + 3 < len(data):
                    # Try to interpret the next byte as Windows-1251
                    potential_char = data[i + 3]
                    if 0x80 <= potential_char <= 0xFF:
                        fixed_bytes.append(potential_char)
                        i += 4
                        continue
                i += 3
            else:
                fixed_bytes.append(data[i])
                i += 1

        if len(fixed_bytes) > 0:
            # Try to decode as Windows-1251
            try:
                decoded = bytes(fixed_bytes).decode("windows-1251", errors="ignore")
                # STRICT CHECK: NO кракозябры allowed!
                if any(0x0400 <= ord(c) <= 0x04FF for c in decoded) and is_result_clean(
                    decoded
                ):
                    with open(file_path, "w", encoding="utf-8") as f:
                        f.write(decoded)
                    print("✓ Successfully fixed using byte reconstruction")
                    return True
                else:
                    print("  Byte reconstruction produced кракозябры, REJECTED")
            except:
                pass
    except Exception as e:
        print(f"  Specific pattern fix failed: {e}")

    # Strategy 5: Last resort - try to restore from git if available
    print("\nTrying git restoration...")

    try:
        import subprocess

        result = subprocess.run(
            ["git", "status", "--porcelain", file_path],
            capture_output=True,
            text=True,
            cwd=os.path.dirname(file_path),
        )

        if result.returncode == 0 and not result.stdout.strip():
            # File is tracked by git, try to restore
            print("  File is tracked by git, attempting restoration...")

            # Use relative path for git commands
            relative_path = os.path.basename(file_path)
            restore_result = subprocess.run(
                ["git", "checkout", "HEAD", "--", relative_path],
                capture_output=True,
                text=True,
                cwd=os.path.dirname(file_path),
            )

            if restore_result.returncode == 0:
                print("  ✓ Successfully restored from git")

                # Now try to convert the restored file
                with open(file_path, "rb") as f:
                    restored_data = f.read()

                # Try to decode as Windows-1251 (most common for Russian text)
                try:
                    decoded = restored_data.decode("windows-1251", errors="ignore")
                    if any(
                        0x0400 <= ord(c) <= 0x04FF for c in decoded
                    ) and is_result_clean(decoded):
                        with open(file_path, "w", encoding="utf-8") as f:
                            f.write(decoded)
                        print("✓ Successfully converted restored file to UTF-8")
                        return True
                except:
                    pass
            else:
                print(f"  Git restoration failed: {restore_result.stderr}")
        else:
            print("  File not tracked by git or has uncommitted changes")
    except Exception as e:
        print(f"  Git restoration failed: {e}")

    print("\n❌ Could not automatically fix encoding issues")
    print("The file may need manual intervention or the encoding is too complex")
    return False


def main():
    if len(sys.argv) != 2:
        print("Usage: python3 fix_encoding.py <file_path>")
        print("Example: python3 fix_encoding.py DChannel/CommDataBlock.h")
        sys.exit(1)

    file_path = sys.argv[1]

    if not os.path.exists(file_path):
        print(f"Error: File '{file_path}' not found")
        sys.exit(1)

    print(f"Fixing encoding for: {file_path}")
    print("=" * 50)

    success = fix_encoding_issues(file_path)

    if success:
        print("\n✅ File successfully fixed!")
        print(f"Original backed up as: {file_path}.backup")
    else:
        print("\n❌ Failed to fix file automatically")
        print(
            "You may need to manually restore from backup or try different approaches"
        )

    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
