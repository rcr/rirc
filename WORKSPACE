workspace(name = "rirc")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

http_archive(
    name = "mbedtls",
    build_file = "@//lib:mbedtls.BUILD",
    sha256 = "525bfde06e024c1218047dee1c8b4c89312df1a4b5658711009086cda5dfaa55",
    strip_prefix = "mbedtls-3.0.0",
    url = "https://github.com/ARMmbed/mbedtls/archive/v3.0.0.tar.gz",
)

git_repository(
    name = "dforsyth_bzl",
    commit = "c8a4f39e3b5a3286202baf9b9e8da7913d4e67af",
    remote = "https://github.com/dforsyth/bzl.git",
)
