#include "comgr-cache-command.h"

namespace COMGR {
using namespace llvm;
using namespace clang;

CachedCommand::CachedCommand(driver::Command &Command,
                             DiagnosticOptions &DiagOpts,
                             ExecuteFnTy &&ExecuteImpl)
    : Command(Command), DiagOpts(DiagOpts),
      ExecuteImpl(std::move(ExecuteImpl)) {}

amd_comgr_status_t CachedCommand::execute(llvm::raw_ostream &LogS) {
  return ExecuteImpl(Command, LogS, DiagOpts);
}
} // namespace COMGR
