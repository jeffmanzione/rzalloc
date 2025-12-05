load("@rules_cc//cc:cc_binary.bzl", "cc_binary")

package(default_visibility = ["//visibility:private"])

cc_binary(
    name = "rzalloc_example",
    srcs = ["rzalloc_example.c"],
    deps = ["//rzalloc"],
)

cc_binary(
    name = "rzalloc_benchmark",
    srcs = ["rzalloc_benchmark.cc"],
    deps = [
        "//rzalloc",
        "@google_benchmark//:benchmark",
    ],
)
