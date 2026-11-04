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

amd_comgr_status_t
SQTTTable::getRecordInfo(uint32_t RecordId,
                         const llvm::sqtt::BinarySQTTRecord **Value) const {
  if (RecordId >= Records.size()) {
    return AMD_COMGR_STATUS_ERROR_INVALID_ARGUMENT;
  }
  *Value = &Records[RecordId];
  return AMD_COMGR_STATUS_SUCCESS;
}

amd_comgr_status_t
SQTTTable::getRecordInfo(uint32_t RecordId,
                         amd_comgr_sqtt_record_info_t Attribute,
                         void *Value) const {
  if (!isRecordInfoValid(Attribute)) {
    return AMD_COMGR_STATUS_ERROR_INVALID_ARGUMENT;
  }

  const sqtt::BinarySQTTRecord *Record;
  amd_comgr_status_t Status = getRecordInfo(RecordId, &Record);
  if (Status != AMD_COMGR_STATUS_SUCCESS) {
    return Status;
  }

  switch (Attribute) {
  case AMD_COMGR_SQTT_RECORD_INFO_PC:
    *static_cast<uint64_t *>(Value) = Record->getTTraceDataAddress();
    return AMD_COMGR_STATUS_SUCCESS;
  case AMD_COMGR_SQTT_RECORD_INFO_EVENT_COUNT:
    *static_cast<uint64_t *>(Value) = Record->size();
    return AMD_COMGR_STATUS_SUCCESS;
  }
  return AMD_COMGR_STATUS_ERROR_INVALID_ARGUMENT;
}

static amd_comgr_sqtt_event_type_t convertEventType(sqtt::EventType Type) {
  switch (Type) {
  case sqtt::EventType::FunctionEntry:
    return AMD_COMGR_SQTT_EVENT_TYPE_FUNCTION_ENTRY;
  case sqtt::EventType::FunctionExit:
    return AMD_COMGR_SQTT_EVENT_TYPE_FUNCTION_EXIT;
  case sqtt::EventType::UserEntry:
    return AMD_COMGR_SQTT_EVENT_TYPE_USER_ENTRY;
  case sqtt::EventType::UserExit:
    return AMD_COMGR_SQTT_EVENT_TYPE_USER_EXIT;
  case sqtt::EventType::SetEventTableBase:
    return AMD_COMGR_SQTT_EVENT_TYPE_SET_EVENT_TABLE_BASE;
  default:
    llvm_unreachable("invalid event type");
  }
}

amd_comgr_status_t
SQTTTable::getEventInfo(uint32_t RecordId, size_t EventId,
                        amd_comgr_sqtt_event_info_t Attribute,
                        void *Value) const {
  if (!isEventInfoValid(Attribute)) {
    return AMD_COMGR_STATUS_ERROR_INVALID_ARGUMENT;
  }

  const sqtt::BinarySQTTRecord *Record;
  amd_comgr_status_t Status = getRecordInfo(RecordId, &Record);
  if (Status != AMD_COMGR_STATUS_SUCCESS) {
    return Status;
  }

  if (EventId >= Record->size()) {
    return AMD_COMGR_STATUS_ERROR_INVALID_ARGUMENT;
  }

  const sqtt::Event &Event = Record->events()[EventId];
  bool HasString = sqtt::Event::hasStringPayload(Event.getType());
  switch (Attribute) {
  case AMD_COMGR_SQTT_EVENT_INFO_TYPE:
    *static_cast<amd_comgr_sqtt_event_type_t *>(Value) =
        convertEventType(Event.getType());
    return AMD_COMGR_STATUS_SUCCESS;
  case AMD_COMGR_SQTT_EVENT_INFO_PAYLOAD_STRING:
    if (!HasString) {
      return AMD_COMGR_STATUS_ERROR;
    }
    // The reader checked that a null byte follows the payload.
    *static_cast<const char **>(Value) = Event.getPayload().data();
    return AMD_COMGR_STATUS_SUCCESS;
  case AMD_COMGR_SQTT_EVENT_INFO_PAYLOAD_TABLE_BASE_INDEX:
    if (HasString) {
      return AMD_COMGR_STATUS_ERROR;
    }
    *static_cast<uint64_t *>(Value) = Event.getIndex();
    return AMD_COMGR_STATUS_SUCCESS;
  }
  return AMD_COMGR_STATUS_ERROR_INVALID_ARGUMENT;
}

bool SQTTTable::isRecordInfoValid(amd_comgr_sqtt_record_info_t RecordInfo) {
  switch (RecordInfo) {
  case AMD_COMGR_SQTT_RECORD_INFO_PC:
  case AMD_COMGR_SQTT_RECORD_INFO_EVENT_COUNT:
    return true;
  }
  return false;
}

bool SQTTTable::isEventInfoValid(amd_comgr_sqtt_event_info_t EventInfo) {
  switch (EventInfo) {
  case AMD_COMGR_SQTT_EVENT_INFO_TYPE:
  case AMD_COMGR_SQTT_EVENT_INFO_PAYLOAD_STRING:
  case AMD_COMGR_SQTT_EVENT_INFO_PAYLOAD_TABLE_BASE_INDEX:
    return true;
  }
  return false;
}
