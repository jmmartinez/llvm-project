//===- comgr-sqtt.cpp - SQTT event table ----------------------------------===//
//
// Part of Comgr, under the Apache License v2.0 with LLVM Exceptions. See
// amd/comgr/LICENSE.TXT in this repository for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file implements the SQTT event table behind the
/// amd_comgr_create_sqtt_table() API.
///
//===----------------------------------------------------------------------===//

#include "comgr-sqtt.h"
#include "comgr-logger.h"
#include "comgr-symbol.h"
#include "comgr.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/Error.h"
#include <memory>
#include <new>

using namespace llvm;
using namespace COMGR;

amd_comgr_status_t SQTTTable::create(DataObject *DataP,
                                     amd_comgr_sqtt_table_t *Table) {
  if (!DataP || DataP->DataKind != AMD_COMGR_DATA_KIND_EXECUTABLE) {
    return AMD_COMGR_STATUS_ERROR_INVALID_ARGUMENT;
  }

  SymbolHelper Helper;
  object::ObjectFile *Obj = Helper.getCachedObjectFile(DataP);
  if (!Obj) {
    getLogger().emit(LogLevel::Error, "SQTT: the data is not an object file");
    return AMD_COMGR_STATUS_ERROR;
  }

  Expected<std::vector<sqtt::BinarySQTTRecord>> RecordsOrErr =
      sqtt::parseBinarySQTTRecords(*Obj);
  if (!RecordsOrErr) {
    getLogger().emit(LogLevel::Error, toString(RecordsOrErr.takeError()));
    return AMD_COMGR_STATUS_ERROR;
  }

  std::unique_ptr<SQTTTable> NewTable(new (std::nothrow) SQTTTable());
  if (!NewTable) {
    return AMD_COMGR_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  NewTable->Records = std::move(*RecordsOrErr);
  *Table = convert(NewTable.release());
  return AMD_COMGR_STATUS_SUCCESS;
}
