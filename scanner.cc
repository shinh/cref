#include "scanner.h"

#include <assert.h>
#include <dwarf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "binary.h"

using namespace std;

struct Attr {
  uint16_t name;
  uint8_t form;
};

struct Abbrev {
  uint16_t tag;
  bool has_children;
  vector<Attr> attrs;
};

static uint64_t uleb128(const uint8_t*& p) {
  uint64_t r = 0;
  int s = 0;
  do {
    r |= (uint64_t)(*p & 0x7f) << s;
    s += 7;
  } while (*p++ >= 0x80);
  return r;
}

static int64_t sleb128(const uint8_t*& p) {
  int64_t r = 0;
  int s = 0;
  for (;;) {
    uint8_t b = *p++;
    if (b < 0x80) {
      if (b & 0x40) {
        r -= (0x80 - b) << s;
      }
      else {
        r |= (b & 0x3f) << s;
      }
      break;
    }
    r |= (b & 0x7f) << s;
    s += 7;
  }
  return r;
}

template <class T>
static void bug(const char* fmt, T v) {
  fprintf(stderr, fmt, v);
  abort();
}

Scanner::Scanner(Binary* binary)
  : binary_(binary) {
}

static void parseAbbrev(const uint8_t* p, vector<Abbrev>* abbrevs) {
  while (true) {
    uint64_t number = uleb128(p);
    if (!number)
      break;

    abbrevs->resize(number + 1);
    Abbrev* abbrev = &(*abbrevs)[number];
    abbrev->tag = uleb128(p);
    abbrev->has_children = *p++;
    while (true) {
      Attr attr;
      attr.name = uleb128(p);
      attr.form = *p++;
      //printf("abbrev attr parsed: %x %x\n", attr.name, attr.form);
      if (!attr.name)
        break;
      abbrev->attrs.push_back(attr);
    }
    //printf("abbrev parsed: %d %d %d\n",
    //       abbrev->tag, abbrev->has_children, (int)abbrev->attrs.size());
  }
}

void Scanner::run() {
  const uint8_t* dinfo_start = (const uint8_t*)binary_->debug_info;
  const uint8_t* dinfo = (const uint8_t*)binary_->debug_info;
  const uint8_t* dabbrev = (const uint8_t*)binary_->debug_abbrev;
  // const char* dstr = binary_->debug_str;
  const uint8_t* dinfo_end = dinfo + binary_->debug_info_len;

  vector<Abbrev> abbrevs;
  const uint8_t* p = dinfo;

  while (p + sizeof(CU) < dinfo_end) {
    CU* cu = (CU*)p;
    if (cu->length == 0 || cu->length == 0xffffffff) {
      bug("unimplemented cu length: %x\n", cu->length);
    }

    const uint8_t* cu_end = p + cu->length + 4;

    onCU(cu, p - dinfo_start);
    p += sizeof(CU);

    abbrevs.clear();
    parseAbbrev(dabbrev + cu->abbrev_offset, &abbrevs);
    //printf("COME abbrevs=%d abbrev_offset=%d\n",
    //       (int)abbrevs.size(), (int)cu->abbrev_offset);

    int depth = 0;

    while (p < cu_end) {
      const uint8_t* abb_p = p;
      uint64_t abbrev_number = uleb128(p);
      //printf("abbrev_number: %d\n", (int)abbrev_number);
      assert(abbrev_number < abbrevs.size());

      if (abbrev_number == 0) {
        depth--;
        if (depth == 0)
          break;
        continue;
      }

      assert(p < cu_end);

      const Abbrev& abbrev = abbrevs[abbrev_number];
      if (abbrev.has_children)
        depth++;

      bool will_care = onAbbrev(abbrev.tag, abbrev_number,
                                abb_p - dinfo_start);

      for (size_t i = 0; i < abbrev.attrs.size(); i++) {
        const uint8_t* attr_p = p;
        const Attr attr = abbrev.attrs[i];
        uint64_t value = 0xffffffffffffffff;
        //printf("name=%x form=%x\n", attr.name, attr.form);

        switch (attr.form) {
        case DW_FORM_addr:
        case DW_FORM_ref_addr:
          if (binary_->is_zipped && cu->ptrsize == 8) {
            value = sleb128(p);
          } else {
            value = (cu->ptrsize == 8 ? *(uint64_t*)p :
                     cu->ptrsize == 4 ? *(uint32_t*)p :
                     cu->ptrsize == 2 ? *(uint16_t*)p :
                     (bug("Unknown ptrsize: %d\n", cu->ptrsize), 0));
            p += cu->ptrsize;
          }
          break;

        case DW_FORM_block1: {
          value = (uint64_t)p;
          uint8_t size = *p++;
          p += size;
          break;
        }

        case DW_FORM_block2: {
          value = (uint64_t)p;
          uint16_t size = *(uint16_t*)p;
          p += 2;
          p += size;
          break;
        }

        case DW_FORM_block4: {
          value = (uint64_t)p;
          uint32_t size = *(uint32_t*)p;
          p += 4;
          p += size;
          break;
        }

        case DW_FORM_block:
        case DW_FORM_exprloc: {
          value = (uint64_t)p;
          uint64_t size = uleb128(p);
          p += size;
          break;
        }

        case DW_FORM_data1:
        case DW_FORM_ref1:
        case DW_FORM_flag:
          value = *p++;
          break;

        case DW_FORM_data2:
        case DW_FORM_ref2:
          value = *(uint16_t*)p;
          p += 2;
          break;

        case DW_FORM_strp:
        case DW_FORM_data4:
        case DW_FORM_ref4:
        case DW_FORM_sec_offset:
          // TODO: Consider offset_size for DW_FORM_strp
          if (binary_->is_zipped) {
            value = sleb128(p);
          } else {
            value = *(uint32_t*)p;
            p += 4;
          }
          break;

        case DW_FORM_data8:
        case DW_FORM_ref8:
          value = *(uint64_t*)p;
          p += 8;
          break;

        case DW_FORM_string:
          value = (uint64_t)p;
          p += strlen((char*)p) + 1;
          break;

        case DW_FORM_sdata:
          value = (uint64_t)sleb128(p);
          break;

        case DW_FORM_udata:
          value = (uint64_t)uleb128(p);
          break;

        case DW_FORM_flag_present:
          break;

        case DW_FORM_ref_udata:
        case DW_FORM_indirect:
        case DW_FORM_ref_sig8:

        default:
          bug("Unknown DW_FORM: %x\n", attr.form);
        }

        if (will_care)
          onAttr(attr.name, attr.form, value, attr_p - dinfo_start);
      }
      if (will_care)
        onAbbrevDone();
    }

    if (!binary_->is_zipped)
      assert(p == cu_end);
  }

  assert(p == dinfo_end);
}
