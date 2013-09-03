#ifndef SCANNER_H_
#define SCANNER_H_

#include <inttypes.h>

class Binary;

struct CU {
  uint32_t length;
  uint16_t version;
  uint32_t abbrev_offset;
  uint8_t ptrsize;
} __attribute__((packed));

class Scanner {
public:
  explicit Scanner(Binary* binary);

  void run();

protected:
  virtual void onCU(CU* cu, uint64_t offset) = 0;
  virtual bool onAbbrev(uint16_t tag, uint64_t number, uint64_t offset) = 0;
  virtual void onAbbrevDone() = 0;
  virtual void onAttr(uint16_t name, uint8_t form,
                      uint64_t value, uint64_t offset) = 0;

  Binary* binary_;
};

#endif  // SCANNER_H_
