//===- sqtt-dump.c --------------------------------------------------------===//
//
// Part of Comgr, under the Apache License v2.0 with LLVM Exceptions. See
// amd/comgr/LICENSE.TXT in this repository for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "amd_comgr.h"
#include "common.h"

int main(int argc, char *argv[]) {
  if (argc != 2 || strncmp(argv[1], "--", 2) == 0) {
    fprintf(stderr, "usage: sqtt-dump <executable>\n");
    return 1;
  }

  char *Buf;
  size_t Size = setBuf(argv[1], &Buf);
  amd_comgr_data_t Data;
  amd_comgr_(create_data(AMD_COMGR_DATA_KIND_EXECUTABLE, &Data));
  amd_comgr_(set_data(Data, Size, Buf));
  free(Buf);

  amd_comgr_sqtt_table_t Table;
  amd_comgr_status_t Status = amd_comgr_create_sqtt_table(Data, &Table);
  if (Status != AMD_COMGR_STATUS_SUCCESS) {
    const char *StatusName;
    amd_comgr_status_string(Status, &StatusName);
    fprintf(stderr, "amd_comgr_create_sqtt_table: %s\n", StatusName);
    amd_comgr_(release_data(Data));
    return 1;
  }

  uint64_t Count;
  amd_comgr_(sqtt_table_get_record_count(Table, &Count));
  printf("records: %zu\n", (size_t)Count);

  for (uint32_t RecordId = 0; RecordId != Count; ++RecordId) {
    uint64_t PC;
    uint64_t EventCount;
    amd_comgr_(sqtt_table_get_record_info(Table, RecordId,
                                          AMD_COMGR_SQTT_RECORD_INFO_PC, &PC));
    amd_comgr_(sqtt_table_get_record_info(
        Table, RecordId, AMD_COMGR_SQTT_RECORD_INFO_EVENT_COUNT, &EventCount));
    printf("record %u: pc=%llu events=%zu\n", RecordId, (unsigned long long)PC,
           (size_t)EventCount);
  }

  // TODO: Print the events once the event getter is implemented.
  amd_comgr_(destroy_sqtt_table(Table));
  amd_comgr_(release_data(Data));
  return 0;
}
