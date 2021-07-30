package(default_visibility = ["//visibility:public"])

cc_library(
    name = "mbedtls",
    srcs = ["@mbedtls//:mbedtls_library"],
    hdrs = ["@mbedtls//:mbedtls_headers"],
    copts = [
        '-DMBEDTLS_CONFIG_FILE=\\"mbedtls.h\\"',
    ],
    includes = [""],
    strip_include_prefix = "//include/",
    deps = [
        ":mbedtls_config",
    ],
)

cc_library(
    name = "mbedtls_config",
    hdrs = ["mbedtls.h"],
)
