/*******************************************************************************
 *
 * University of Illinois/NCSA
 * Open Source License
 *
 * Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * with the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 *     * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimers.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimers in the
 *       documentation and/or other materials provided with the distribution.
 *
 *     * Neither the names of Advanced Micro Devices, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this Software without specific prior written permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS WITH
 * THE SOFTWARE.
 *
 ******************************************************************************/

#include "unittests-common.h"

TEST(ComgrTest, TestCompile) {
  amd_comgr_data_t DataSource1, DataSource2, DataInclude;
  amd_comgr_data_set_t DataSetIn, DataSetPreproc, DataSetBc, DataSetLinked,
      DataSetAsm, DataSetReloc, DataSetExec;
  amd_comgr_action_info_t DataAction;
  size_t Count;
  const char *CodeGenOptions[] = {"-mllvm", "--color"};
  size_t CodeGenOptionsCount = std::size(CodeGenOptions);

  auto BufSource1 = ReadFile(TEST_OBJ_DIR "/source1.cl");
  auto BufSource2 = ReadFile(TEST_OBJ_DIR "/source2.cl");
  auto BufInclude = ReadFile(TEST_OBJ_DIR "/include-macro.h");

  EXPECT_SUCCESS(amd_comgr_create_data_set(&DataSetIn));

  EXPECT_SUCCESS(
      amd_comgr_create_data(AMD_COMGR_DATA_KIND_SOURCE, &DataSource1));
  EXPECT_SUCCESS(
      amd_comgr_set_data(DataSource1, BufSource1.size(), BufSource1.c_str()));
  EXPECT_SUCCESS(amd_comgr_set_data_name(DataSource1, "source1.cl"));
  EXPECT_SUCCESS(amd_comgr_data_set_add(DataSetIn, DataSource1));

  EXPECT_SUCCESS(
      amd_comgr_create_data(AMD_COMGR_DATA_KIND_SOURCE, &DataSource2));
  EXPECT_SUCCESS(
      amd_comgr_set_data(DataSource2, BufSource2.size(), BufSource2.c_str()));
  EXPECT_SUCCESS(amd_comgr_set_data_name(DataSource2, "source2.cl"));
  EXPECT_SUCCESS(amd_comgr_data_set_add(DataSetIn, DataSource2));

  EXPECT_SUCCESS(
      amd_comgr_create_data(AMD_COMGR_DATA_KIND_INCLUDE, &DataInclude));
  EXPECT_SUCCESS(
      amd_comgr_set_data(DataInclude, BufInclude.size(), BufInclude.c_str()));
  EXPECT_SUCCESS(amd_comgr_set_data_name(DataInclude, "include-macro.h"));
  EXPECT_SUCCESS(amd_comgr_data_set_add(DataSetIn, DataInclude));

  EXPECT_SUCCESS(amd_comgr_create_action_info(&DataAction));
  EXPECT_SUCCESS(amd_comgr_action_info_set_language(
      DataAction, AMD_COMGR_LANGUAGE_OPENCL_1_2));
  EXPECT_SUCCESS(amd_comgr_action_info_set_isa_name(
      DataAction, "amdgcn-amd-amdhsa--gfx900"));
  EXPECT_SUCCESS(amd_comgr_action_info_set_option_list(
      DataAction, CodeGenOptions, CodeGenOptionsCount));

  EXPECT_SUCCESS(amd_comgr_create_data_set(&DataSetPreproc));

  EXPECT_SUCCESS(amd_comgr_do_action(AMD_COMGR_ACTION_SOURCE_TO_PREPROCESSOR,
                                     DataAction, DataSetIn, DataSetPreproc));

  EXPECT_SUCCESS(amd_comgr_action_data_count(
      DataSetPreproc, AMD_COMGR_DATA_KIND_SOURCE, &Count));

  EXPECT_EQ(Count, 2);

  EXPECT_SUCCESS(amd_comgr_create_data_set(&DataSetBc));

  EXPECT_SUCCESS(amd_comgr_do_action(AMD_COMGR_ACTION_COMPILE_SOURCE_TO_BC,
                                     DataAction, DataSetPreproc, DataSetBc));

  EXPECT_SUCCESS(
      amd_comgr_action_data_count(DataSetBc, AMD_COMGR_DATA_KIND_BC, &Count));

  EXPECT_EQ(Count, 2);

  EXPECT_SUCCESS(amd_comgr_create_data_set(&DataSetLinked));

  EXPECT_SUCCESS(amd_comgr_do_action(AMD_COMGR_ACTION_LINK_BC_TO_BC, DataAction,
                                     DataSetBc, DataSetLinked));

  EXPECT_SUCCESS(amd_comgr_action_data_count(DataSetLinked,
                                             AMD_COMGR_DATA_KIND_BC, &Count));

  EXPECT_EQ(Count, 1);

  EXPECT_SUCCESS(amd_comgr_create_data_set(&DataSetAsm));

  EXPECT_SUCCESS(amd_comgr_do_action(AMD_COMGR_ACTION_CODEGEN_BC_TO_ASSEMBLY,
                                     DataAction, DataSetLinked, DataSetAsm));

  EXPECT_SUCCESS(amd_comgr_action_data_count(
      DataSetAsm, AMD_COMGR_DATA_KIND_SOURCE, &Count));

  EXPECT_EQ(Count, 1);

  EXPECT_SUCCESS(amd_comgr_create_data_set(&DataSetReloc));

  EXPECT_SUCCESS(
      amd_comgr_do_action(AMD_COMGR_ACTION_ASSEMBLE_SOURCE_TO_RELOCATABLE,
                          DataAction, DataSetAsm, DataSetReloc));

  EXPECT_SUCCESS(amd_comgr_action_data_count(
      DataSetReloc, AMD_COMGR_DATA_KIND_RELOCATABLE, &Count));

  EXPECT_EQ(Count, 1);

  EXPECT_SUCCESS(amd_comgr_create_data_set(&DataSetExec));

  EXPECT_SUCCESS(amd_comgr_action_info_set_option_list(DataAction, NULL, 0));

  EXPECT_SUCCESS(
      amd_comgr_do_action(AMD_COMGR_ACTION_LINK_RELOCATABLE_TO_EXECUTABLE,
                          DataAction, DataSetReloc, DataSetExec));

  EXPECT_SUCCESS(amd_comgr_action_data_count(
      DataSetExec, AMD_COMGR_DATA_KIND_EXECUTABLE, &Count));

  EXPECT_EQ(Count, 1);

  EXPECT_SUCCESS(amd_comgr_release_data(DataSource1));
  EXPECT_SUCCESS(amd_comgr_release_data(DataSource2));
  EXPECT_SUCCESS(amd_comgr_release_data(DataInclude));
  EXPECT_SUCCESS(amd_comgr_destroy_data_set(DataSetIn));
  EXPECT_SUCCESS(amd_comgr_destroy_data_set(DataSetPreproc));
  EXPECT_SUCCESS(amd_comgr_destroy_data_set(DataSetBc));
  EXPECT_SUCCESS(amd_comgr_destroy_data_set(DataSetLinked));
  EXPECT_SUCCESS(amd_comgr_destroy_data_set(DataSetAsm));
  EXPECT_SUCCESS(amd_comgr_destroy_data_set(DataSetReloc));
  EXPECT_SUCCESS(amd_comgr_destroy_data_set(DataSetExec));
  EXPECT_SUCCESS(amd_comgr_destroy_action_info(DataAction));
}
