#include "TxtDataHelpers.h"

void StrHelper::strncpy(char* dest, const char* src, size_t buf_sz) {
  while (buf_sz > 1 && *src) {
    *dest++ = *src++;
    buf_sz--;
  }
  *dest = 0;  // truncates if needed
}

void StrHelper::strzcpy(char* dest, const char* src, size_t buf_sz) {
  while (buf_sz > 1 && *src) {
    *dest++ = *src++;
    buf_sz--;
  }
  while (buf_sz > 0) {  // pad remaining with nulls
    *dest++ = 0;
    buf_sz--;
  }
}

bool StrHelper::isBlank(const char* str) {
  while (*str) {
    if (*str++ != ' ') return false;
  }
  return true;
}

#include <Arduino.h>

union int32_Float_t 
{
  int32_t Long;
  float Float;
};

#ifndef FLT_MIN_EXP
#define FLT_MIN_EXP (-999)
#endif
 
#ifndef FLT_MAX_EXP
#define FLT_MAX_EXP (999)
#endif
 
#define _FTOA_TOO_LARGE -2  // |input| > 2147483520 
#define _FTOA_TOO_SMALL -1  // |input| < 0.0000001 
 
//precision 0-9
#define PRECISION 7
 
//_ftoa function 
static void _ftoa(float f, char *p, int *status) 
{
  int32_t mantissa, int_part, frac_part;
  int16_t exp2;
  int32_Float_t x;
 
  *status = 0;
  if (f == 0.0) 
  {
    *p++ = '0';
    *p++ = '.';
    *p++ = '0';
    *p = 0;
    return;
  }
 
  x.Float = f;
  exp2 = (unsigned char)(x.Long>>23) - 127;
  mantissa = (x.Long&0xFFFFFF) | 0x800000;
  frac_part = 0;
  int_part = 0;
 
  if (exp2 >= 31) 
  {
    *status = _FTOA_TOO_LARGE;
    return;
  } 
  else if (exp2 < -23) 
  {
    *status = _FTOA_TOO_SMALL;
    return;
  } 
  else if (exp2 >= 23)
  { 
    int_part = mantissa<<(exp2 - 23);
  }
  else if (exp2 >= 0) 
  {
    int_part = mantissa>>(23 - exp2);
    frac_part = (mantissa<<(exp2 + 1))&0xFFFFFF;
  } 
  else 
  {
    //if (exp2 < 0)
    frac_part = (mantissa&0xFFFFFF)>>-(exp2 + 1);
  }
 
  if (x.Long < 0)
      *p++ = '-';
  if (int_part == 0)
    *p++ = '0';
  else 
  {
    ltoa(int_part, p, 10);
    while (*p)
      p++;
  }
  *p++ = '.';
  if (frac_part == 0)
    *p++ = '0';
  else 
  {
    char m;
 
    for (m=0; m<PRECISION; m++) 
    {
      //frac_part *= 10;
      frac_part = (frac_part<<3) + (frac_part<<1); 
      *p++ = (frac_part>>24) + '0';
      frac_part &= 0xFFFFFF;
    }
 
    //delete ending zeroes
    for (--p; p[0] == '0' && p[-1] != '.'; --p)
      ;
    ++p;
  }
  *p = 0;
}

const char* StrHelper::ftoa(float f) {
  static char tmp[16];
  int status;
  _ftoa(f, tmp, &status);
  if (status) {
    tmp[0] = '0';  // fallback/error value
    tmp[1] = 0;
  }
  return tmp;
}

const char* StrHelper::ftoa3(float f) {
  static char s[16];
  int v = (int)(f * 1000.0f + (f >= 0 ? 0.5f : -0.5f)); // rounded ×1000
  int w = v / 1000;                                     // whole
  int d = abs(v % 1000);                                // decimals
  snprintf(s, sizeof(s), "%d.%03d", w, d);
  for (int i = strlen(s) - 1; i > 0 && s[i] == '0'; i--)
      s[i] = 0;
  int L = strlen(s);
  if (s[L - 1] == '.') s[L - 1] = 0;
  return s;
}

uint32_t StrHelper::fromHex(const char* src) {
  uint32_t n = 0;
  while (*src) {
    if (*src >= '0' && *src <= '9') {
      n <<= 4;
      n |= (*src - '0');
    } else if (*src >= 'A' && *src <= 'F') {
      n <<= 4;
      n |= (*src - 'A' + 10);
    } else if (*src >= 'a' && *src <= 'f') {
      n <<= 4;
      n |= (*src - 'a' + 10);
    } else {
      break;  // non-hex char encountered, stop parsing
    }
    src++;
  }
  return n;
}

int StrHelper::transliterateUtf8(char* dest, const char* src, size_t buf_sz) {
  // Transliteration tables for Russian/Cyrillic UTF-8 → Latin ASCII
  // Indexed by (second_byte - 0x80) for 0xD0 and 0xD1 prefixes

  static const char* d0_table[64] = {
    NULL,       // 0x80
    "Yo",       // 0x81 Ё
    NULL, NULL, // 0x82-0x83
    "Ye",       // 0x84 Є (Ukrainian)
    NULL,       // 0x85
    "I",        // 0x86 І (Ukrainian)
    "Yi",       // 0x87 Ї (Ukrainian)
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, // 0x88-0x8F
    "A",        // 0x90 А
    "B",        // 0x91 Б
    "V",        // 0x92 В
    "G",        // 0x93 Г
    "D",        // 0x94 Д
    "E",        // 0x95 Е
    "Zh",       // 0x96 Ж
    "Z",        // 0x97 З
    "I",        // 0x98 И
    "Y",        // 0x99 Й
    "K",        // 0x9A К
    "L",        // 0x9B Л
    "M",        // 0x9C М
    "N",        // 0x9D Н
    "O",        // 0x9E О
    "P",        // 0x9F П
    "R",        // 0xA0 Р
    "S",        // 0xA1 С
    "T",        // 0xA2 Т
    "U",        // 0xA3 У
    "F",        // 0xA4 Ф
    "Kh",       // 0xA5 Х
    "Ts",       // 0xA6 Ц
    "Ch",       // 0xA7 Ч
    "Sh",       // 0xA8 Ш
    "Shch",     // 0xA9 Щ
    "",         // 0xAA Ъ (hard sign - omitted)
    "Y",        // 0xAB Ы
    "'",        // 0xAC Ь (soft sign)
    "E",        // 0xAD Э
    "Yu",       // 0xAE Ю
    "Ya",       // 0xAF Я
    "a",        // 0xB0 а
    "b",        // 0xB1 б
    "v",        // 0xB2 в
    "g",        // 0xB3 г
    "d",        // 0xB4 д
    "e",        // 0xB5 е
    "zh",       // 0xB6 ж
    "z",        // 0xB7 з
    "i",        // 0xB8 и
    "y",        // 0xB9 й
    "k",        // 0xBA к
    "l",        // 0xBB л
    "m",        // 0xBC м
    "n",        // 0xBD н
    "o",        // 0xBE о
    "p",        // 0xBF п
  };

  static const char* d1_table[64] = {
    "r",        // 0x80 р
    "s",        // 0x81 с
    "t",        // 0x82 т
    "u",        // 0x83 у
    "f",        // 0x84 ф
    "kh",       // 0x85 х
    "ts",       // 0x86 ц
    "ch",       // 0x87 ч
    "sh",       // 0x88 ш
    "shch",     // 0x89 щ
    "",         // 0x8A ъ (hard sign - omitted)
    "y",        // 0x8B ы
    "'",        // 0x8C ь (soft sign)
    "e",        // 0x8D э
    "yu",       // 0x8E ю
    "ya",       // 0x8F я
    NULL,       // 0x90
    "yo",       // 0x91 ё
    NULL, NULL, // 0x92-0x93
    "ye",       // 0x94 є (Ukrainian)
    NULL,       // 0x95
    "i",        // 0x96 і (Ukrainian)
    "yi",       // 0x97 ї (Ukrainian)
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  };

  int j = 0;
  for (int i = 0; src[i] != 0 && j < (int)buf_sz - 1; ) {
    unsigned char c = (unsigned char)src[i];

    if (c < 0x80) {
      // ASCII character - pass through
      dest[j++] = c;
      i++;
    } else if (c == 0xD0 || c == 0xD1) {
      // Cyrillic UTF-8 two-byte sequence
      unsigned char c2 = (unsigned char)src[i + 1];
      if (c2 == 0) break; // incomplete sequence at end of string

      const char** table = (c == 0xD0) ? d0_table : d1_table;
      int idx = c2 - 0x80;

      if (idx >= 0 && idx < 64 && table[idx] != NULL) {
        const char* repl = table[idx];
        while (*repl && j < (int)buf_sz - 1) {
          dest[j++] = *repl++;
        }
      }
      i += 2;
    } else if (c == 0xD2) {
      // Ukrainian Ґ/ґ
      unsigned char c2 = (unsigned char)src[i + 1];
      if (c2 == 0) break;
      if (c2 == 0x90 && j < (int)buf_sz - 1) dest[j++] = 'G';   // Ґ
      else if (c2 == 0x91 && j < (int)buf_sz - 1) dest[j++] = 'g'; // ґ
      i += 2;
    } else if (c >= 0x80) {
      // Other multi-byte UTF-8 - skip continuation bytes
      i++;
      while (src[i] && ((unsigned char)src[i] & 0xC0) == 0x80) i++;
    } else {
      i++;
    }
  }
  dest[j] = 0;
  return j;
}
