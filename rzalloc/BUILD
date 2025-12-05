load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "rzalloc",
    srcs = ["rzalloc.c"],
    hdrs = ["rzalloc.h"],
)

cc_test(
    name = "rzalloc_test",
    timeout = "short",
    srcs = ["rzalloc_test.cc"],
    deps = [
        ":rzalloc",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
