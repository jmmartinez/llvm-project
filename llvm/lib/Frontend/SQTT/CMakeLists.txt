add_llvm_component_library(LLVMFrontendSQTT
  Event.cpp

  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/Frontend/SQTT

  LINK_COMPONENTS
  Core
  Support
  )
