//===-ThinLTOCodeGenerator.cpp - LLVM Link Time Optimizer -----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the Thin Link Time Optimization library. This library is
// intended to be used by linker to optimize code at link time.
//
//===----------------------------------------------------------------------===//

#include "llvm/LTO/ThinLTOCodeGenerator.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Bitcode/BitcodeWriterPass.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/ExecutionEngine/ObjectMemoryBuffer.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Mangler.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Linker/Linker.h"
#include "llvm/MC/SubtargetFeature.h"
#include "llvm/Object/ModuleSummaryIndexObjectFile.h"
#include "llvm/Support/CachePruning.h"
#include "llvm/Support/raw_sha1_ostream.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/ThreadPool.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/IPO/FunctionImport.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/ObjCARC.h"
#include "llvm/Transforms/Utils/FunctionImportUtils.h"

using namespace llvm;

namespace {

static cl::opt<int> ThreadCount("threads",
                                cl::init(std::thread::hardware_concurrency()));

// APPLE SPECIFIC
#if defined(__APPLE__)
#include <sys/param.h>
#include <sys/sysctl.h>

// Gets the number of *physical cores* on the machine.
static int getNumCores() {
  if (ThreadCount.getNumOccurrences())
    return ThreadCount;

  uint32_t count;
  size_t len = sizeof(count);

  sysctlbyname("hw.physicalcpu", &count, &len, NULL, 0);
  if (count < 1) {
    int nm[2];
    nm[0] = CTL_HW;
    nm[1] = HW_AVAILCPU;
    sysctl(nm, 2, &count, &len, NULL, 0);
    if (count < 1) {
      count = std::thread::hardware_concurrency();
    }
  }
  return count;
}
#else
static int getNumCores() { return ThreadCount; }
#endif
// END APPLE SPECIFIC

static void diagnosticHandler(const DiagnosticInfo &DI) {
  DiagnosticPrinterRawOStream DP(errs());
  DI.print(DP);
  errs() << '\n';
}

// Simple helper to load a module from bitcode
static std::unique_ptr<Module>
loadModuleFromBuffer(const MemoryBufferRef &Buffer, LLVMContext &Context,
                     bool Lazy) {
  SMDiagnostic Err;
  ErrorOr<std::unique_ptr<Module>> ModuleOrErr(nullptr);
  if (Lazy) {
    ModuleOrErr =
        getLazyBitcodeModule(MemoryBuffer::getMemBuffer(Buffer, false), Context,
                             /* ShouldLazyLoadMetadata */ Lazy);
  } else {
    ModuleOrErr = parseBitcodeFile(Buffer, Context);
  }
  if (std::error_code EC = ModuleOrErr.getError()) {
    Err = SMDiagnostic(Buffer.getBufferIdentifier(), SourceMgr::DK_Error,
                       EC.message());
    Err.print("ThinLTO", errs());
    report_fatal_error("Can't load module, abort.");
  }
  return std::move(ModuleOrErr.get());
}

// Simple helper to save temporary files for debug.
static void saveTempBitcode(const Module &TheModule, StringRef TempDir,
                            unsigned count, StringRef Suffix) {
  if (TempDir.empty())
    return;
  // User asked to save temps, let dump the bitcode file after import.
  auto SaveTempPath = TempDir + llvm::utostr(count) + Suffix;
  std::error_code EC;
  raw_fd_ostream OS(SaveTempPath.str(), EC, sys::fs::F_None);
  if (EC)
    report_fatal_error(Twine("Failed to open ") + SaveTempPath +
                       " to save optimized bitcode\n");
  WriteBitcodeToFile(&TheModule, OS, true, false);
}

static void generateModuleMap(const std::vector<MemoryBufferRef> &Modules,
                              StringMap<MemoryBufferRef> &ModuleMap) {
  for (auto &ModuleBuffer : Modules) {
    assert(ModuleMap.find(ModuleBuffer.getBufferIdentifier()) ==
               ModuleMap.end() &&
           "Expect unique Buffer Identifier");
    ModuleMap[ModuleBuffer.getBufferIdentifier()] = ModuleBuffer;
  }
}

/// Provide a "loader" for the FunctionImporter to access function from other
/// modules.
class ModuleLoader {
  /// The context that will be used for importing.
  LLVMContext &Context;

  /// Map from Module identifier to MemoryBuffer. Used by clients like the
  /// FunctionImported to request loading a Module.
  StringMap<MemoryBufferRef> &ModuleMap;

public:
  ModuleLoader(LLVMContext &Context, StringMap<MemoryBufferRef> &ModuleMap)
      : Context(Context), ModuleMap(ModuleMap) {}

  /// Load a module on demand.
  std::unique_ptr<Module> operator()(StringRef Identifier) {
    return loadModuleFromBuffer(ModuleMap[Identifier], Context, /*Lazy*/ true);
  }
};

static void promoteModule(Module &TheModule, const ModuleSummaryIndex &Index) {
  if (renameModuleForThinLTO(TheModule, Index))
    report_fatal_error("renameModuleForThinLTO failed");
}

static void crossImportIntoModule(Module &TheModule,
                                  const ModuleSummaryIndex &Index,
                                  StringMap<MemoryBufferRef> &ModuleMap) {
  ModuleLoader Loader(TheModule.getContext(), ModuleMap);
  FunctionImporter Importer(Index, Loader);
  Importer.importFunctions(TheModule);
}

static std::string toHex(StringRef Input) {
  static const char *const LUT = "0123456789ABCDEF";
  size_t Length = Input.size();

  std::string Output;
  Output.reserve(2 * Length);
  for (size_t i = 0; i < Length; ++i) {
    const unsigned char c = Input[i];
    Output.push_back(LUT[c >> 4]);
    Output.push_back(LUT[c & 15]);
  }
  return Output;
}

static void optimizeModule(Module &TheModule, TargetMachine &TM) {
  // Populate the PassManager
  PassManagerBuilder PMB;
  PMB.LibraryInfo = new TargetLibraryInfoImpl(TM.getTargetTriple());
  PMB.Inliner = createFunctionInliningPass();
  // FIXME: should get it from the bitcode?
  PMB.OptLevel = 3;
  PMB.LoopVectorize = true;
  PMB.SLPVectorize = true;
  PMB.VerifyInput = true;
  PMB.VerifyOutput = false;

  legacy::PassManager PM;

  // Add the TTI (required to inform the vectorizer about register size for
  // instance)
  PM.add(createTargetTransformInfoWrapperPass(TM.getTargetIRAnalysis()));

  // Add optimizations
  PMB.populateThinLTOPassManager(PM);
  PM.add(createObjCARCContractPass());

  PM.run(TheModule);
}

std::unique_ptr<MemoryBuffer> codegenModule(Module &TheModule,
                                            TargetMachine &TM) {
  SmallVector<char, 128> OutputBuffer;

  // CodeGen
  {
    raw_svector_ostream OS(OutputBuffer);
    legacy::PassManager PM;
    if (TM.addPassesToEmitFile(PM, OS, TargetMachine::CGFT_ObjectFile,
                               /* DisableVerify */ true))
      report_fatal_error("Failed to setup codegen");

    // Run codegen now. resulting binary is in OutputBuffer.
    PM.run(TheModule);
  }
  return make_unique<ObjectMemoryBuffer>(std::move(OutputBuffer));
}

static std::unique_ptr<MemoryBuffer>
ProcessThinLTOModule(Module &TheModule, const ModuleSummaryIndex &Index,
                     StringMap<MemoryBufferRef> &ModuleMap, TargetMachine &TM,
                     ThinLTOCodeGenerator::CachingOptions CacheOptions,
                     StringRef SaveTempsDir, unsigned count) {

  // Save temps: after IPO.
  saveTempBitcode(TheModule, SaveTempsDir, count, ".1.IPO.bc");

  // "Benchmark"-like optimization: single-source case
  bool SingleModule = (ModuleMap.size() == 1);

  if (!SingleModule) {
    promoteModule(TheModule, Index);

    // Save temps: after promotion.
    saveTempBitcode(TheModule, SaveTempsDir, count, ".2.promoted.bc");

    crossImportIntoModule(TheModule, Index, ModuleMap);

    // Save temps: after cross-module import.
    saveTempBitcode(TheModule, SaveTempsDir, count, ".3.imported.bc");
  }

  std::string CachedFilename;
  if (!CacheOptions.Path.empty()) {
    // Compute the hash of the IR
    raw_sha1_ostream HashStream;
    WriteBitcodeToFile(&TheModule, HashStream);
    auto Hash = toHex(HashStream.sha1());

    // Check if this IR has already an object file in the cache
    sys::fs::file_status Status;
    CachedFilename = (Twine(CacheOptions.Path) + "/" + Hash + ".o").str();
    sys::fs::status(CachedFilename, Status);
    if (sys::fs::exists(Status)) {
      // Cache Hit!
      auto FileLoaded =
      MemoryBuffer::getFile(CachedFilename, Status.getSize(), false);
      if (!FileLoaded) {
        errs() << "ThinLTO: error opening the file '" << CachedFilename
        << "': " << FileLoaded.getError().message() << "\n";
        report_fatal_error("FAILURE");
      }
      return std::move(*FileLoaded);
    }
    // Cache miss, move on
  }


  optimizeModule(TheModule, TM);

  saveTempBitcode(TheModule, SaveTempsDir, count, ".3.opt.bc");


  auto OutputBuffer = codegenModule(TheModule, TM);

  if (!CachedFilename.empty()) {
    // Cache the Produced object file

    // Write to a temporary to avoid race condition
    SmallString<128> TempFilename;
    int TempFD;
    std::error_code EC =
    sys::fs::createTemporaryFile("Thin", "tmp.o", TempFD, TempFilename);
    if (EC) {
      errs() << "Error: " << EC.message() << "\n";
      report_fatal_error("ThinLTO: Can't get a temporary file");
    }
    {
      raw_fd_ostream OS(TempFD, /* ShouldClose */ true);
      OS << OutputBuffer->getBuffer();
    }
    // Rename to final destination (hopefully race condition won't matter here)
    sys::fs::rename(TempFilename, CachedFilename);
  }
  return OutputBuffer;
}

// Initialize the TargetMachine builder for a given Triple
static void initTMBuilder(TargetMachineBuilder &TMBuilder,
                          const Triple &TheTriple) {
  // Set a default CPU for Darwin triples (copied from LTOCodeGenerator).
  // FIXME this looks pretty terrible...
  if (TMBuilder.MCpu.empty() && TheTriple.isOSDarwin()) {
    if (TheTriple.getArch() == llvm::Triple::x86_64)
      TMBuilder.MCpu = "core2";
    else if (TheTriple.getArch() == llvm::Triple::x86)
      TMBuilder.MCpu = "yonah";
    else if (TheTriple.getArch() == llvm::Triple::aarch64)
      TMBuilder.MCpu = "cyclone";
  }
  TMBuilder.TheTriple = std::move(TheTriple);
}

} // end anonymous namespace

void ThinLTOCodeGenerator::addModule(StringRef Identifier, StringRef Data) {
  MemoryBufferRef Buffer(Data, Identifier);
  if (Modules.empty()) {
    // First module added, so initialize the triple and some options
    LLVMContext Context;
    Triple TheTriple(getBitcodeTargetTriple(Buffer, Context));
    initTMBuilder(TMBuilder, Triple(TheTriple));
  }
#ifndef NDEBUG
  else {
    LLVMContext Context;
    assert(TMBuilder.TheTriple.str() ==
               getBitcodeTargetTriple(Buffer, Context) &&
           "ThinLTO modules with different triple not supported");
  }
#endif
  Modules.push_back(Buffer);
}

void ThinLTOCodeGenerator::preserveSymbol(StringRef Name) {
  PreservedSymbols.insert(Name);
}

void ThinLTOCodeGenerator::crossReferenceSymbol(StringRef Name) {
  CrossReferencedSymbols.insert(Name);
}

// TargetMachine factory
std::unique_ptr<TargetMachine> TargetMachineBuilder::create() const {
  std::string ErrMsg;
  const Target *TheTarget =
      TargetRegistry::lookupTarget(TheTriple.str(), ErrMsg);
  if (!TheTarget) {
    report_fatal_error("Can't load target for this Triple: " + ErrMsg);
  }

  // Use MAttr as the default set of features.
  SubtargetFeatures Features(MAttr);
  Features.getDefaultSubtargetFeatures(TheTriple);
  std::string FeatureStr = Features.getString();
  return std::unique_ptr<TargetMachine>(TheTarget->createTargetMachine(
      TheTriple.str(), MCpu, FeatureStr, Options, RelocModel,
      CodeModel::Default, CGOptLevel));
}

/**
 * Produce the combined summary index from all the bitcode files:
 * "thin-link".
 */
std::unique_ptr<ModuleSummaryIndex> ThinLTOCodeGenerator::linkCombinedIndex() {
  std::unique_ptr<ModuleSummaryIndex> CombinedIndex;
  uint64_t NextModuleId = 0;
  for (auto &ModuleBuffer : Modules) {
    ErrorOr<std::unique_ptr<object::ModuleSummaryIndexObjectFile>> ObjOrErr =
        object::ModuleSummaryIndexObjectFile::create(ModuleBuffer,
                                                     diagnosticHandler, false);
    if (std::error_code EC = ObjOrErr.getError()) {
      // FIXME diagnose
      errs() << "error: can't create ModuleSummaryIndexObjectFile for buffer: "
             << EC.message() << "\n";
      return nullptr;
    }
    auto Index = (*ObjOrErr)->takeIndex();
    if (CombinedIndex) {
      CombinedIndex->mergeFrom(std::move(Index), ++NextModuleId);
    } else {
      CombinedIndex = std::move(Index);
    }
  }
  return CombinedIndex;
}

/**
 * Perform promotion and renaming of exported internal functions.
 */
void ThinLTOCodeGenerator::promote(Module &TheModule,
                                   ModuleSummaryIndex &Index) {
  promoteModule(TheModule, Index);
}

/**
 * Perform cross-module importing for the module identified by ModuleIdentifier.
 */
void ThinLTOCodeGenerator::crossModuleImport(Module &TheModule,
                                             ModuleSummaryIndex &Index) {
  StringMap<MemoryBufferRef> ModuleMap;
  generateModuleMap(Modules, ModuleMap);
  crossImportIntoModule(TheModule, Index, ModuleMap);
}

/**
 * Perform post-importing ThinLTO optimizations.
 */
void ThinLTOCodeGenerator::optimize(Module &TheModule) {
  initTMBuilder(TMBuilder, Triple(TheModule.getTargetTriple()));
  optimizeModule(TheModule, *TMBuilder.create());
}

/**
 * Perform ThinLTO CodeGen.
 */
std::unique_ptr<MemoryBuffer> ThinLTOCodeGenerator::codegen(Module &TheModule) {
  initTMBuilder(TMBuilder, Triple(TheModule.getTargetTriple()));
  return codegenModule(TheModule, *TMBuilder.create());
}

// Main entry point for the ThinLTO processing
void ThinLTOCodeGenerator::run() {
  std::unique_ptr<ModuleSummaryIndex> Index;
  StringMap<MemoryBufferRef> ModuleMap;

  {
    ThreadPool Pool(getNumCores());
    // Launch Cache Pruning in parallel with the Thin-Link phase
    Pool.async([&] {
      CachePruning(CacheOptions.Path)
          .setPruningInterval(CacheOptions.PruningInterval)
          .setEntryExpiration(CacheOptions.Expiration)
          .setMaxSize(CacheOptions.MaxPercentageOfAvailableSpace)
          .prune();
    });

    Pool.async([&] {
      // Sequential linking phase
      Index = linkCombinedIndex();

      // Save temps: index.
      if (!SaveTempsDir.empty()) {
        auto SaveTempPath = SaveTempsDir + "index.bc";
        std::error_code EC;
        raw_fd_ostream OS(SaveTempPath, EC, sys::fs::F_None);
        if (EC)
          report_fatal_error(Twine("Failed to open ") + SaveTempPath +
                             " to save optimized bitcode\n");
        WriteIndexToFile(*Index, OS);
      }

      // Prepare the resulting object vector
      assert(ProducedBinaries.empty() && "The generator should not be reused");
      ProducedBinaries.resize(Modules.size());

      // Prepare the module map.
      generateModuleMap(Modules, ModuleMap);
    });

    // Wait for the previous tasks to complete before starting the process
    Pool.wait();

    // Parallel optimizer + codegen
    int count = 0;
    for (auto &ModuleBuffer : Modules) {
      Pool.async([&](int count) {
        LLVMContext Context;

        // Parse module now
        auto TheModule = loadModuleFromBuffer(ModuleBuffer, Context, false);

        // Save temps: original file.
        if (!SaveTempsDir.empty()) {
          saveTempBitcode(*TheModule, SaveTempsDir, count, ".0.original.bc");
        }

        ProducedBinaries[count] = ProcessThinLTOModule(
            *TheModule, *Index, ModuleMap, *TMBuilder.create(), CacheOptions,
            SaveTempsDir, count);
      }, count);
      count++;
    }
  }

  // If statistics were requested, print them out now.
  if (llvm::AreStatisticsEnabled())
    llvm::PrintStatistics();
}
