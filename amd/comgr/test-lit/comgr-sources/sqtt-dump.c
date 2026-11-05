//===- sqtt-dump.c --------------------------------------------------------===//
//
// Part of Comgr, under the Apache License v2.0 with LLVM Exceptions. See
// amd/comgr/LICENSE.TXT in this repository for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "amd_comgr.h"
#include "common.h"

static const char *eventTypeName(amd_comgr_sqtt_event_type_t Type) {
  switch (Type) {
  case AMD_COMGR_SQTT_EVENT_TYPE_FUNCTION_ENTRY:
    return "FunctionEntry";
  case AMD_COMGR_SQTT_EVENT_TYPE_FUNCTION_EXIT:
    return "FunctionExit";
  case AMD_COMGR_SQTT_EVENT_TYPE_USER_ENTRY:
    return "UserEntry";
  case AMD_COMGR_SQTT_EVENT_TYPE_USER_EXIT:
    return "UserExit";
  case AMD_COMGR_SQTT_EVENT_TYPE_SET_EVENT_TABLE_BASE:
    return "SetEventTableBase";
  }
  return "Unknown";
}

// amd_comgr_symbolize requires a non-null user data, which is unused here.
static void printLocation(const char *Symbol, void *UserData) {
  (void)UserData;
  printf("  location: %s", Symbol);
}

static void printEvent(amd_comgr_sqtt_table_t Table, uint32_t RecordId,
                       size_t EventId) {
  amd_comgr_sqtt_event_type_t Type;
  amd_comgr_(sqtt_table_get_event_info(Table, RecordId, EventId,
                                       AMD_COMGR_SQTT_EVENT_INFO_TYPE, &Type));
  if (Type == AMD_COMGR_SQTT_EVENT_TYPE_SET_EVENT_TABLE_BASE) {
    uint64_t Index;
    amd_comgr_(sqtt_table_get_event_info(
        Table, RecordId, EventId,
        AMD_COMGR_SQTT_EVENT_INFO_PAYLOAD_TABLE_BASE_INDEX, &Index));
    printf("  %s %llu\n", eventTypeName(Type), (unsigned long long)Index);
  } else {
    const char *Payload;
    amd_comgr_(sqtt_table_get_event_info(
        Table, RecordId, EventId, AMD_COMGR_SQTT_EVENT_INFO_PAYLOAD_STRING,
        &Payload));
    printf("  %s %s\n", eventTypeName(Type), Payload);
  }
}

static void printAll(amd_comgr_data_t Data, amd_comgr_sqtt_table_t Table) {
  amd_comgr_symbolizer_info_t Symbolizer;
  amd_comgr_(create_symbolizer_info(Data, printLocation, &Symbolizer));

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

    for (size_t EventId = 0; EventId != EventCount; ++EventId)
      printEvent(Table, RecordId, EventId);

    amd_comgr_(symbolize(Symbolizer, PC, /*is_code=*/true, &RecordId));
  }

  amd_comgr_(destroy_symbolizer_info(Symbolizer));
}

// Parses a decimal ID no larger than Max.
static bool parseId(const char *Str, unsigned long long Max,
                    unsigned long long *Id) {
  if (*Str < '0' || *Str > '9')
    return false;
  char *End;
  errno = 0;
  *Id = strtoull(Str, &End, 10);
  return errno == 0 && *End == '\0' && *Id <= Max;
}

int main(int argc, char *argv[]) {
  bool All = argc == 3 && strcmp(argv[2], "--all") == 0;
  unsigned long long RecordId;
  unsigned long long EventId;
  bool Single = argc == 6 && strcmp(argv[2], "--record-id") == 0 &&
                parseId(argv[3], UINT32_MAX, &RecordId) &&
                strcmp(argv[4], "--event-id") == 0 &&
                parseId(argv[5], SIZE_MAX, &EventId);
  if ((!All && !Single) || strncmp(argv[1], "--", 2) == 0) {
    fprintf(stderr, "usage: sqtt-dump <executable> --all\n"
                    "       sqtt-dump <executable> --record-id <id> "
                    "--event-id <id>\n");
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

  if (All)
    printAll(Data, Table);
  else
    printEvent(Table, (uint32_t)RecordId, (size_t)EventId);

  amd_comgr_(destroy_sqtt_table(Table));
  amd_comgr_(release_data(Data));
  return 0;
}
