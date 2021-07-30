package(default_visibility = ["//visibility:public"])

load("@dforsyth_bzl//gperf:gperf.bzl", "gperf")
load("//:rirc.bzl", "COPTS")

gperf(
    name = "gperf",
    srcs = glob(["*.gperf"]),
)

cc_library(
    name = "handlers",
    srcs = glob([
        "*.c",
        "*.h",
    ]),
    copts = COPTS,
    deps = [
        ":gperf",
        "//lib:mbedtls",
        "//src:draw",
        "//src:io",
        "//src:state_h",
        "//src/components",
    ],
)
