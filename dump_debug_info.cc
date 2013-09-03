#define __STDC_FORMAT_MACROS
#include <assert.h>
#include <dwarf.h>
#include <err.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <map>
#include <memory>
#include <set>
#include <stack>
#include <string>
#include <vector>

#include "binary.h"
#include "scanner.h"

using namespace std;

static uint64_t offset_;

#define CHECK(c, ...) if (!(c)) error(__VA_ARGS__)

void reportImpl(const char* fmt, va_list ap) {
  char* str;
  vasprintf(&str, fmt, ap);
  fprintf(stderr, "%"PRIx64": %s\n", offset_, str);
  free(str);
}

void report(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  reportImpl(fmt, ap);
  va_end(ap);
}

void error(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  reportImpl(fmt, ap);
  va_end(ap);
  abort();
}

string stringPrintf(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char* str;
  vasprintf(&str, fmt, ap);
  va_end(ap);
  string ret(str);
  free(str);
  return ret;
}

static const uint64_t VAARG_OFFSET = (uint64_t)-1;

bool isSpecialTypeOffset(uint64_t offset) {
  return offset == 0 || offset == VAARG_OFFSET;
}

struct Type {
  enum {
    TYPE_ERROR, TYPE_BASE, TYPE_TYPEDEF, TYPE_STRUCT,
    TYPE_POINTER, TYPE_ARRAY, TYPE_CONST, TYPE_VOLATILE, TYPE_FUNC
  };
  int type;
  int size;
  const char* name;
  uint64_t ref;
  int cu_id;
  uint64_t offset;
  Type* ref_type;

  Type(int t)
    : type(t), ref(0), ref_type(NULL) {}
  Type(int t, int s, const char* n)
    : type(t), size(s), name(n), ref(0), ref_type(NULL) {}
  Type(int t, const char* n, uint64_t r)
    : type(t), name(n), ref(r), ref_type(NULL) {}

  int getSize() const {
    if (size)
      return size;
    if (type == TYPE_TYPEDEF || type == TYPE_CONST || type == TYPE_VOLATILE)
      return ref_type->getSize();
    return 0;
  }

  string getJson() const {
    offset_ = offset;
    switch (type) {
    case TYPE_BASE:
      CHECK(size, "Uknkown size for base");
      return stringPrintf("[\"base\", %d]", size);
    case TYPE_TYPEDEF:
      return stringPrintf("[\"typedef\", \"%s\"]", getName().c_str());
    case TYPE_STRUCT:
      return stringPrintf("[\"struct\", %d]", size);
    default:
      CHECK(false, "Unsupported type: %d", type);
    }
    return "???";
  }

  string getName() const {
    offset_ = offset;
    switch (type) {
    case TYPE_BASE:
      return name;
    case TYPE_TYPEDEF:
      // CHECK(ref_type, "Unresolved ref");
      // TODO(hamaji): OK?
      return ref_type ? ref_type->getName() : "<anonymous>";
    case TYPE_STRUCT:
      if (!name)
        return "<anonymous>";
      return name;
    case TYPE_POINTER:
      if (!ref_type)
        return "void*";
      return ref_type->getName() + '*';
    case TYPE_ARRAY:
      CHECK(ref_type, "Unresolved ref");
      return ref_type->getName() + "[]";
    case TYPE_CONST:
    case TYPE_VOLATILE:
      if (!ref_type)
        return "void";
      return ref_type->getName();
    case TYPE_FUNC:
      return "<func>";
    }
    CHECK(false, "Unknown type: %d", type);
    return "???";
  }

};

struct Func {
  uint64_t ret;
  const char* name;
  bool external;
  vector<uint64_t> args;
  int cu_id;
  uint64_t offset;
};

struct DumpCU {
  vector<Func*> funcs;
  set<uint64_t> types;
};

class DumpDebugScanner : public Scanner {
public:
  DumpDebugScanner(Binary* binary)
    : Scanner(binary),
      debug_str_(binary->debug_str),
      debug_str_len_(binary->debug_str_len),
      cu_cnt_(0),
      last_func_(NULL) {
  }

  void dump() {
    vector<DumpCU*> cus;
    DumpCU* cu = NULL;
    int prev_cu_id = 0;
    for (size_t i = 0; i < funcs_.size(); i++) {
      Func* func = funcs_[i];
      if (!func->name)
        continue;
      if (!func->external)
        continue;
      offset_ = func->offset;

      if (prev_cu_id != func->cu_id) {
        prev_cu_id = func->cu_id;
        cu = new DumpCU;
        cus.push_back(cu);

#if 1
        stack<uint64_t> types;
        for (map<uint64_t, Type*>::const_iterator iter = types_.begin();
             iter != types_.end();
             ++iter) {
          uint64_t type_offset = iter->first;
          if (isSpecialTypeOffset(type_offset))
            continue;
          Type* type = iter->second;
          if (type->cu_id != func->cu_id)
            continue;
          types.push(type_offset);
        }

        while (!types.empty()) {
          uint64_t type_offset = types.top();
          types.pop();
          if (isSpecialTypeOffset(type_offset))
            continue;
          if (cu->types.insert(type_offset).second) {
            Type* type = getTypeFromOffset(type_offset);
            if (type->ref) {
              Type* ref_type = getTypeFromOffset(type->ref);
              type->ref_type = ref_type;
              types.push(type->ref);
            }
          }
        }
#endif
      }

      cu->funcs.push_back(func);

#if 0
      stack<uint64_t> types;
      types.push(func->ret);
      for (size_t j = 0; j < func->args.size(); j++)
        types.push(func->args[j]);

      while (!types.empty()) {
        uint64_t type_offset = types.top();
        types.pop();
        if (isSpecialTypeOffset(type_offset))
          continue;
        if (cu->types.insert(type_offset).second) {
          Type* type = getTypeFromOffset(type_offset);
          if (type->ref) {
            Type* ref_type = getTypeFromOffset(type->ref);
            type->ref_type = ref_type;
            types.push(type->ref);
          }
        }
      }
#endif
    }

    puts("[");
    for (size_t i = 0; i < cus.size(); i++) {
      if (i)
        puts(",");
      puts("{\"type\": {");

      DumpCU* cu = cus[i];

      bool is_first = true;
      for (set<uint64_t>::const_iterator iter = cu->types.begin();
           iter != cu->types.end();
           ++iter) {
        Type* type = getTypeFromOffset(*iter);
        if (type->name &&
            (type->type == Type::TYPE_BASE ||
             type->type == Type::TYPE_TYPEDEF ||
             type->type == Type::TYPE_STRUCT)) {
          if (type->type == Type::TYPE_TYPEDEF &&
              type->name == type->getName())
            continue;
          if (!is_first)
            puts(",");
          is_first = false;
          printf("  \"%s\": %s", type->name, type->getJson().c_str());
        }
      }

      puts("");
      puts(" },");

      puts(" \"func\": {");

      for (size_t i = 0; i < cu->funcs.size(); i++) {
        Func* func = cu->funcs[i];
        string args;
        for (size_t j = 0; j < func->args.size(); j++) {
          args += stringPrintf(", \"%s\"",
                               getTypeName(func->args[j]).c_str());
        }
        printf("  \"%s\": [\"%s\"%s]%s\n",
               func->name, getTypeName(func->ret).c_str(), args.c_str(),
               i + 1 == cu->funcs.size() ? "" : ",");
      }

      puts(" }");

      puts("}");

      delete cu;
    }
    puts("]");
  }

private:
  virtual void onCU(CU* cu, uint64_t offset) {
    offset_ = offset;
    last_func_ = NULL;
    report("CU: %d len=%x version=%x ptrsize=%x",
           cu_cnt_, cu->length, cu->version, cu->ptrsize);
    cu_cnt_++;
    cu_offset_ = offset;
  }

  virtual bool onAbbrev(uint16_t tag, uint64_t /*number*/, uint64_t offset) {
    if (tag != DW_TAG_formal_parameter &&
        tag != DW_TAG_unspecified_parameters) {
      last_func_ = NULL;
    }
    prev_tag_ = tag_;
    tag_ = tag;
    if (tag == DW_TAG_base_type ||
        tag == DW_TAG_typedef ||
        tag == DW_TAG_structure_type ||
        tag == DW_TAG_union_type ||
        tag == DW_TAG_enumeration_type ||
        tag == DW_TAG_pointer_type ||
        tag == DW_TAG_array_type ||
        tag == DW_TAG_const_type ||
        tag == DW_TAG_volatile_type ||
        tag == DW_TAG_subroutine_type ||
        tag == DW_TAG_subprogram ||
        tag == DW_TAG_formal_parameter ||
        tag == DW_TAG_unspecified_parameters) {
      offset_ = offset;
      values_.clear();
      return true;
    }
    return false;
  }

  virtual void onAbbrevDone() {
    switch (tag_) {
    case DW_TAG_base_type:
      handleBaseType();
      break;
    case DW_TAG_typedef:
      handleTypedef();
      break;
    case DW_TAG_structure_type:
    case DW_TAG_union_type:
    case DW_TAG_enumeration_type:
      handleStruct();
      break;
    case DW_TAG_pointer_type:
      handleQualifiler(Type::TYPE_POINTER);
      break;
    case DW_TAG_array_type:
      handleQualifiler(Type::TYPE_ARRAY);
      break;
    case DW_TAG_const_type:
      handleQualifiler(Type::TYPE_CONST);
      break;
    case DW_TAG_volatile_type:
      handleQualifiler(Type::TYPE_VOLATILE);
      break;
    case DW_TAG_subroutine_type:
      handleSubroutine();
      break;
    case DW_TAG_subprogram:
      handleFunction();
      break;
    case DW_TAG_formal_parameter:
      if (prev_tag_ == DW_TAG_subprogram ||
          prev_tag_ == DW_TAG_formal_parameter) {
        handleParameter();
      }
      break;
    case DW_TAG_unspecified_parameters:
      if (prev_tag_ == DW_TAG_subprogram ||
          prev_tag_ == DW_TAG_formal_parameter) {
        handleUnspecifiedParameters();
      }
      break;
    }
  }

  virtual void onAttr(uint16_t name, uint8_t /*form*/, uint64_t value,
                      uint64_t /*offset*/) {
    if (!values_.insert(make_pair(name, value)).second) {
      fprintf(stderr, "Duplicated name: %d\n", (int)name);
      exit(1);
    }
  }

  void addType(Type* type) {
    type->cu_id = cu_cnt_;
    type->offset = offset_;
    if (!types_.insert(make_pair(offset_, type)).second) {
      fprintf(stderr, "Duplicated offset: %"PRIx64"\n", offset_);
      exit(1);
    }
  }

  void handleBaseType() {
    uint64_t size = getValue(DW_AT_byte_size);
    //uint64_t encoding = getValue(DW_AT_encoding);
    const char* name = getStrOrNull(DW_AT_name);
    if (!name)
      name = "???";
    report("basetype: %s", name);
    addType(new Type(Type::TYPE_BASE, size, name));
  }

  void handleTypedef() {
    uint64_t type = getType();
    const char* name = getStr(DW_AT_name);
    report("typedef: %s", name);
    addType(new Type(Type::TYPE_TYPEDEF, name, type));
  }

  void handleStruct() {
    uint64_t size = getValueOrZero(DW_AT_byte_size);
    const char* name = getStrOrNull(DW_AT_name);
    report("struct: %s", name);
    addType(new Type(Type::TYPE_STRUCT, size, name));
  }

  void handleQualifiler(int qual) {
    uint64_t type = getType();
    const char* name = getStrOrNull(DW_AT_name);
    report("qualifier: %s type=%"PRIx64, name, type);
    addType(new Type(qual, name, type));
  }

  void handleSubroutine() {
    last_func_ = NULL;
    addType(new Type(Type::TYPE_FUNC));
  }

  void handleFunction() {
    uint64_t ret = getType();
    const char* name = getStrOrNull(DW_AT_name);
    uint64_t external = getValueOrZero(DW_AT_external);
    if (!name)
      return;
    report("function: %s ret=%"PRIx64" %"PRIx64, name, ret);
    last_func_ = new Func();
    last_func_->ret = ret;
    last_func_->name = name;
    last_func_->external = external;
    last_func_->cu_id = cu_cnt_;
    last_func_->offset = offset_;
    funcs_.push_back(last_func_);
  }

  void handleParameter() {
    if (!last_func_)
      return;
    uint64_t type = getType();
    last_func_->args.push_back(type);
  }

  void handleUnspecifiedParameters() {
    if (!last_func_)
      return;
    last_func_->args.push_back(VAARG_OFFSET);
  }

  const char* getStr(int name) const {
    uint64_t v = getValue(name);
    if (v >= debug_str_len_)
      return (const char*)v;
    else
      return debug_str_ + v;
  }

  const char* getStrOrNull(int name) const {
    uint64_t v = getValueOrZero(name);
    if (!v)
      return NULL;
    if (v >= debug_str_len_)
      return (const char*)v;
    else
      return debug_str_ + v;
  }

  uint64_t getType() const {
    uint64_t v = getValueOrZero(DW_AT_type);
    if (!v)
      return v;
    return v + cu_offset_;
  }

  Type* getTypeFromOffset(uint64_t offset) const {
    map<uint64_t, Type*>::const_iterator found = types_.find(offset);
    CHECK(found != types_.end(), "Type %"PRIx64" not found", offset);
    return found->second;
  }

  uint64_t getValue(int name) const {
    map<int, uint64_t>::const_iterator found = values_.find(name);
    CHECK(found != values_.end(), "Name not found: %d", name);
    return found->second;
  }

  uint64_t getValueOrZero(int name) const {
    map<int, uint64_t>::const_iterator found = values_.find(name);
    return found != values_.end() ? found->second : 0;
  }

  string getTypeName(uint64_t offset) const {
    if (!offset)
      return "void";
    if (offset == VAARG_OFFSET)
      return "...";
    return getTypeFromOffset(offset)->getName();
  }

  const char* debug_str_;
  size_t debug_str_len_;

  int cu_cnt_;
  uint64_t cu_offset_;

  map<uint64_t, Type*> types_;
  vector<Func*> funcs_;
  Func* last_func_;

  uint16_t tag_;
  uint16_t prev_tag_;
  map<int, uint64_t> values_;
};

static const int HEADER_SIZE = 8;

int main(int argc, char* argv[]) {
  const char* argv0 = argv[0];
  for (int i = 1; i < argc; i++) {
    if (argv[i][0] != '-') {
      continue;
    }
    fprintf(stderr, "Unknown option: %s\n", argv[i]);
    argc--;
    argv++;
  }

  if (argc < 2) {
    fprintf(stderr, "Usage: %s binary\n", argv0);
    exit(1);
  }

  auto_ptr<Binary> binary(readBinary(argv[1]));

  DumpDebugScanner dumper(binary.get());
  dumper.run();
  dumper.dump();
}
