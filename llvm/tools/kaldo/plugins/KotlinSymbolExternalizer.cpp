#include "KotlinSymbolExternalizer.hpp"

#include "llvm/ExecutionEngine/Orc/Shared/MachOObjectFormat.h"
#include "llvm/Support/raw_ostream.h"

#include "../KaldoInternal.hpp"

namespace llvm::orc {
static llvm::StringRef DWARFSectionName = "__DWARF";
}

namespace {
constexpr auto KCallableExportedFlags =
    llvm::JITSymbolFlags::Callable | llvm::JITSymbolFlags::Exported;
} // namespace

struct SymbolRename {
  llvm::jitlink::Symbol *Symbol;
  llvm::orc::SymbolStringPtr OriginalSymName;
  llvm::orc::SymbolStringPtr ImplSymName;
};

static std::vector<SymbolRename>
findSymbolsToExternalize(llvm::jitlink::LinkGraph &G) {
  std::vector<SymbolRename> ToExternalize{};

  // Collect _kfun: defined symbols that need externalizing
  for (auto *Symbol : G.defined_symbols()) {
    if (!Symbol->hasName())
      continue;
    if (Symbol->getScope() == llvm::jitlink::Scope::Local)
      continue;

    llvm::StringRef Name = *Symbol->getName();
    if (!Name.starts_with(llvm::kaldo::KKotlinFunPrefix))
      continue;

    auto OriginalName = G.getSymbolStringPool()->intern(Name);
    auto ImplementationName = G.getSymbolStringPool()->intern(
        (Name + llvm::kaldo::KImplSymbolSuffix).str());

    ToExternalize.push_back(
        {Symbol, std::move(OriginalName), std::move(ImplementationName)});
  }

  return ToExternalize;
}

/// Rename definitions and retarget edges.
static void retargetEdges(llvm::jitlink::LinkGraph &Graph,
                          const std::vector<SymbolRename> &ToExternalize) {
  for (const auto &[symbol, originalSymName, implSymName] : ToExternalize) {

    // Create external symbol for the original name
    auto &ExternalSymbol = Graph.addExternalSymbol(*originalSymName, 0, false);

    // Unwind and debug metadata must point to the implementation, not the
    // redirectable stub.

    for (auto &Section : Graph.sections()) {
      const auto SectionName = Section.getName();
      if (SectionName == llvm::orc::MachOCompactUnwindSectionName ||
          SectionName == llvm::orc::MachOEHFrameSectionName ||
          SectionName.starts_with(llvm::orc::DWARFSectionName))
        continue;

      for (auto *Block : Section.blocks()) {
        for (auto &Edge : Block->edges()) {
          if (&Edge.getTarget() != symbol)
            continue;
          // Non-zero addends are internal references into the implementation.
          if (Edge.getAddend() != 0)
            continue;
          Edge.setTarget(ExternalSymbol);
        }
      }
    }

    // Rename the definition to $knhr, keep it visible so MR can emit it
    symbol->setName(implSymName);
    symbol->setScope(llvm::jitlink::Scope::Default);
  }
}

void llvm::kaldo::orc::plugin::KotlinSymbolExternalizerPlugin::modifyPassConfig(
    llvm::orc::MaterializationResponsibility &MR, llvm::jitlink::LinkGraph &G,
    llvm::jitlink::PassConfiguration &Config) {

  if (&MR.getTargetJITDylib() == &StubsJd)
    return;
  // Ignore symbols coming from cache artifacts (i.e., non user-defined code)
  if (G.getName().find("-cache.a") != std::string::npos)
    return;

  // Note from the documentation: graph nodes still have their original vmaddrs.
  Config.PrePrunePasses.emplace_back(
      [&MR, this](llvm::jitlink::LinkGraph &Graph) -> llvm::Error {
        const auto ToExternalize = findSymbolsToExternalize(Graph);
        if (ToExternalize.empty())
          return llvm::Error::success();

        // Register $knhr names with MR and hand back originals as reexports
        // from StubsJD
        llvm::orc::SymbolFlagsMap ImplFlags;
        llvm::orc::SymbolAliasMap Aliases;

        for (auto &Entry : ToExternalize) {
          ImplFlags[Entry.ImplSymName] = KCallableExportedFlags;
          Aliases[Entry.OriginalSymName] = llvm::orc::SymbolAliasMapEntry(
              Entry.OriginalSymName, KCallableExportedFlags);
        }

        // Tell MR we're defining the new $knhr impl names
        if (auto Err = MR.defineMaterializing(std::move(ImplFlags))) {
          return Err;
        }

        // Hand back original names, they'll resolve from StubsJD via reexports
        if (auto Err =
                MR.replace(llvm::orc::reexports(StubsJd, std::move(Aliases)))) {
          return Err;
        }

        retargetEdges(Graph, ToExternalize);

        dbgs() << "[kaldo] :: KotlinSymbolExternalizer has externalized '"
               << ToExternalize.size() << "' symbols.\n";
        return llvm::Error::success();
      });
}
