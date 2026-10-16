#ifndef LLVM_OBJECTIVECINTEROP_H
#define LLVM_OBJECTIVECINTEROP_H

struct ObjCTypeAdapter;

namespace llvm::kaldo::objc {

struct ObjCAdapterTable {
  const ObjCTypeAdapter **Entries{nullptr};
  int Count{0};
};

struct ObjCUniquePrefixOutput {
  const char* UniquePrefixAddress;
};

struct ObjCExportAdaptersOutput {
  const ObjCTypeAdapter **ClassAdapters;
  int ClassAdaptersNum;

  const ObjCTypeAdapter **ProtocolAdapters;
  int ProtocolAdaptersNum;
};

}

#endif // LLVM_OBJECTIVECINTEROP_H
