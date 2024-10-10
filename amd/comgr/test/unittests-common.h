#ifndef UNITTEST_COMMON_H
#define UNITTEST_COMMON_H

#include <amd_comgr.h>

#include <gtest/gtest.h>

#include <string>

#define EXPECT_SUCCESS(Expr) EXPECT_EQ(Expr, AMD_COMGR_STATUS_SUCCESS);

std::string ReadFile(const char *Path);

#endif
