#pragma once

#include <Arduino.h>

/**
 * Pure text helpers - no hardware, no globals, no Arduino peripherals.
 *
 * Kept apart from the display and calendar modules so this logic can be
 * exercised without a board attached: everything here is a total function of
 * its arguments. The trickiest formatting bugs live in code like this, and
 * reflashing is an expensive way to test a string transform.
 */
namespace TextUtils {

/** Two-digit zero-padded number, e.g. 7 -> "07". */
inline String twoDigits(int value) {
  char buf[4];
  snprintf(buf, sizeof(buf), "%02d", value);
  return String(buf);
}

/** Shortens to maxLength, marking elision with "..." when it truncates. */
inline String truncate(const String &text, int maxLength) {
  if (maxLength <= 0) {
    return String();
  }
  if ((int)text.length() <= maxLength) {
    return text;
  }
  if (maxLength <= 3) {
    return text.substring(0, maxLength);
  }
  return text.substring(0, maxLength - 3) + "...";
}

/**
 * Maps UTF-8 Cyrillic onto the single-byte codepage the LED font uses.
 *
 * Only the U+0400 block is translated; every other byte passes through. Input
 * that ends mid-sequence is truncated rather than read past the end.
 */
inline String utf8ToDisplayCodepage(const String &source) {
  String target;
  target.reserve(source.length());

  const int len = source.length();
  for (int i = 0; i < len; ++i) {
    unsigned char n = source[i];

    if (n >= 0xC0) {
      // Two-byte sequence: bail out rather than read past the end.
      if (i + 1 >= len) {
        break;
      }

      switch (n) {
      case 0xD0: {
        n = source[++i];
        if (n == 0x81) {
          n = 0xA8;  // Ё
        } else if (n >= 0x90 && n <= 0xBF) {
          n += 0x30;
        }
        break;
      }
      case 0xD1: {
        n = source[++i];
        if (n == 0x91) {
          n = 0xB8;  // ё
        } else if (n >= 0x80 && n <= 0x8F) {
          n += 0x70;
        }
        break;
      }
      default:
        break;
      }
    }

    target += char(n);
  }

  return target;
}

} // namespace TextUtils
