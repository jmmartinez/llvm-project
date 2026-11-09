//===- comgr-sqtt.h - SQTT event table ------------------------------------===//
//
// Part of Comgr, under the Apache License v2.0 with LLVM Exceptions. See
// amd/comgr/LICENSE.TXT in this repository for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef COMGR_SQTT_H
#define COMGR_SQTT_H

#include "amd_comgr.h"
#include "llvm/Target/AMDGPU/SQTTEvent.h"
#include <cstdint>
#include <vector>

namespace COMGR {

struct DataObject;

class SQTTTable {
  // Indexed by record ID. The string payloads point into the data object's
  // buffer, so the data object must outlive the table.
  std::vector<llvm::sqtt::BinarySQTTRecord> Records;

  amd_comgr_status_t
  getRecordInfo(uint32_t RecordId,
                const llvm::sqtt::BinarySQTTRecord **Value) const;

  static bool isRecordInfoValid(amd_comgr_sqtt_record_info_t RecordInfo);
  static bool isEventInfoValid(amd_comgr_sqtt_event_info_t EventInfo);

public:
  // Parses the SQTT sections of DataP into a new table.
  static amd_comgr_status_t create(DataObject *DataP,
                                   amd_comgr_sqtt_table_t *Table);

  size_t recordSize() const { return Records.size(); }

  amd_comgr_status_t getRecordInfo(uint32_t RecordId,
                                   amd_comgr_sqtt_record_info_t Attribute,
                                   void *Value) const;

  amd_comgr_status_t getEventInfo(uint32_t RecordId, size_t EventId,
                                  amd_comgr_sqtt_event_info_t Attribute,
                                  void *Value) const;

  static amd_comgr_sqtt_table_t convert(SQTTTable *Table) {
    amd_comgr_sqtt_table_t Handle = {
        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(Table))};
    return Handle;
  }

  static SQTTTable *convert(amd_comgr_sqtt_table_t Table) {
    return reinterpret_cast<SQTTTable *>(Table.handle);
  }
};

} // namespace COMGR

#endif // COMGR_SQTT_H
