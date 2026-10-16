#ifndef LLVM_OBJCSELECTORFIXUPPLUGIN_H
#define LLVM_OBJCSELECTORFIXUPPLUGIN_H

#include "llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h"

namespace llvm::kaldo::orc::plugin {

class ObjCSelectorFixupPlugin : public llvm::orc::ObjectLinkingLayer::Plugin {
public:

  void modifyPassConfig(llvm::orc::MaterializationResponsibility &MR,
                        llvm::jitlink::LinkGraph &G,
                        llvm::jitlink::PassConfiguration &Config) override;

  llvm::Error
  notifyEmitted(llvm::orc::MaterializationResponsibility &MR) override {
    return llvm::Error::success();
  }

  llvm::Error
  notifyFailed(llvm::orc::MaterializationResponsibility &MR) override {
    return llvm::Error::success();
  }

  llvm::Error notifyRemovingResources(llvm::orc::JITDylib &JD,
                                      llvm::orc::ResourceKey K) override {
    return llvm::Error::success();
  }

  void notifyTransferringResources(llvm::orc::JITDylib &JD,
                                   llvm::orc::ResourceKey DstKey,
                                   llvm::orc::ResourceKey SrcKey) override {}
};

} // namespace llvm::kaldo::orc::plugin

#endif // LLVM_OBJCSELECTORFIXUPPLUGIN_H
