#include "ObjCSelectorFixupPlugin.hpp"

#if defined(__APPLE__)
#include <objc/runtime.h>
#endif

namespace {
auto constexpr KDataObjcSelrefsSection = "__DATA,__objc_selrefs";
auto constexpr KObjcSelrefsSection = "__objc_selrefs";
auto constexpr KTextObjcMethnameSection = "__TEXT,__objc_methname";
auto constexpr KObjcMethnameSection = "__objc_methname";
} // namespace

using MethnameMap = llvm::DenseMap<uint64_t, std::string>;

static llvm::jitlink::Section *
findObjCSelectorSection(llvm::jitlink::LinkGraph &G) {
  // Find __objc_selrefs section, this contains pointers to selector strings.
  // JITLink uses segment,section naming convention: "__DATA,__objc_selrefs"
  llvm::jitlink::Section *SelrefsSection {nullptr};

  for (auto &Section : G.sections()) {
    if (auto SectionName = Section.getName();
        SectionName == KDataObjcSelrefsSection ||
        SectionName == KObjcSelrefsSection ||
        SectionName.ends_with(KObjcSelrefsSection)) {
      SelrefsSection = &Section;
      break;
    }
  }

  return SelrefsSection;
}

static MethnameMap buildMethnameStringMap(llvm::jitlink::LinkGraph &G) {
  // First, build a map from methname block addresses to their string content.
  // __objc_methname section contains the actual selector strings.
  // Use uint64_t as key since ExecutorAddr doesn't have std::hash.
  llvm::DenseMap<uint64_t, std::string> MethnameStrings{};

  for (auto &Section : G.sections()) {
    auto SectionName = Section.getName();
    if (SectionName == KTextObjcMethnameSection ||
        SectionName == KObjcMethnameSection ||
        SectionName.ends_with(KObjcMethnameSection)) {
      for (const auto *Block : Section.blocks()) {
        auto Content = Block->getContent();
        if (!Content.empty()) {
          // The content is the null-terminated string
          const std::string MethName(Content.data(),
                                     strnlen(Content.data(), Content.size()));
          MethnameStrings[Block->getAddress().getValue()] = MethName;
        }
      }
    }
  }

  return MethnameStrings;
}

static std::string
tryToFindSelectoViaGraphEdges(const MethnameMap &MethnameStrings,
                              llvm::jitlink::Block *Block) {
  for (auto &Edge : Block->edges()) {
    if (auto &TargetSym = Edge.getTarget(); TargetSym.isDefined()) {
      auto TargetAddrVal = TargetSym.getAddress().getValue();
      if (auto It = MethnameStrings.find(TargetAddrVal);
          It != MethnameStrings.end()) {
        return It->second;
      }
    }
  }
  return "";
}

static std::string
tryToFindSelectoViaFixedupContent(const MethnameMap &MethnameStrings,
                                  const llvm::ArrayRef<char> &BlockContent) {

  void *SelectorStringAddr = nullptr;
  memcpy(&SelectorStringAddr, BlockContent.data(), sizeof(void *));

  if (SelectorStringAddr != nullptr) {
    // Look up in our methname map
    const auto TargetAddrVal = reinterpret_cast<uint64_t>(SelectorStringAddr);
    if (const auto It = MethnameStrings.find(TargetAddrVal);
        It != MethnameStrings.end()) {
      return It->second;
    }

    // Last resort, try to read directly from the pointer
    const auto *SelectorCStr = static_cast<const char *>(SelectorStringAddr);
    const auto Len = strnlen(SelectorCStr, 256);
    if (Len > 0 && Len < 256) {
      return {SelectorCStr, Len};
    }
  }

  return "";
}

void llvm::kaldo::orc::plugin::ObjCSelectorFixupPlugin::modifyPassConfig(
    llvm::orc::MaterializationResponsibility &MR, llvm::jitlink::LinkGraph &G,
    llvm::jitlink::PassConfiguration &Config) {

  // Add a post-fixup pass that runs after all relocations are applied.
  // At this point, we can find and fix up ObjC selector references.
  Config.PostFixupPasses.emplace_back([](llvm::jitlink::LinkGraph &Graph) {
    auto *SelrefsSection = findObjCSelectorSection(Graph);

    // No selector references, nothing to do
    if (!SelrefsSection) {
      return llvm::Error::success();
    }

    int FixedCount = 0;
    const auto MethnameStrings = buildMethnameStringMap(Graph);

    for (auto *Block : SelrefsSection->blocks()) {
      auto BlockContent = Block->getContent();

      if (BlockContent.size() < sizeof(void *)) {
        continue;
      }

      // Approach 1: Try to find selector via graph edges (more reliable)
      std::string SelectorName =
          tryToFindSelectoViaGraphEdges(MethnameStrings, Block);
      // Approach 2: Fall back to reading fixedup content
      if (SelectorName.empty()) {
        SelectorName =
            tryToFindSelectoViaFixedupContent(MethnameStrings, BlockContent);
      }

      if (SelectorName.empty()) {
        dbgs() << "[kaldo] :: ObjCSelectorFixupPlugin: Could not determine "
                  "selector for "
                  "block @ 0x"
               << Block->getAddress().getValue() << "\n";
        continue;
      }

      // Register patched Objective-C selector to its runtime by
      // updating the selector reference to use the registered selector.
      SEL RegisteredSel = sel_registerName(SelectorName.c_str());
      auto MutableContent = Block->getMutableContent(Graph);
      memcpy(MutableContent.data(), &RegisteredSel, sizeof(void *));

      FixedCount++;
    }

    dbgs() << "[kaldo] :: ObjCSelectorFixupPlugin fixed " << FixedCount
           << " selector references\n";

    return llvm::Error::success();
  });
}
