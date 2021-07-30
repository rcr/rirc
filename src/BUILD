package(default_visibility = ["//visibility:public"])

load("//:rirc.bzl", "COPTS")

cc_binary(
    name = "bin",
    srcs = [
        "rirc.c",
        "rirc.h",
    ],
    copts = COPTS,
    linkopts = ["-pthread"],
    deps = [
        ":rirc",
        "//:config",
    ],
)

cc_library(
    name = "rirc",
    srcs = [
        "rirc.c",
        "rirc.h",
    ],
    copts = COPTS,
    deps = [
        ":draw",
        ":io",
        ":state",
        "//:config",
    ],
)

cc_library(
    name = "rirc_h",
    hdrs = ["rirc.h"],
)

cc_library(
    name = "io",
    srcs = [
        "io.c",
        "io.h",
    ],
    deps = [
        ":rirc_h",
        "//:config",
        "//lib:mbedtls",
        "//src/utils",
    ],
)

cc_library(
    name = "state",
    srcs = [
        "state.c",
    ],
    copts = [
        '-DVERSION=\\"0.1.5\\"',
    ],
    deps = [
        ":draw",
        ":state_h",
        "//src/components",
        "//src/handlers",
    ],
)

cc_library(
    name = "state_h",
    hdrs = ["state.h"],
)

cc_library(
    name = "draw",
    srcs = [
        "draw.c",
        "draw.h",
    ],
    deps = [
        ":io",
        ":state_h",
        "//:config",
        "//src/components",
    ],
)
