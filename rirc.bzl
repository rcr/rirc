def _config_impl(ctx):
    out = ctx.actions.declare_file("config.h")
    ctx.actions.expand_template(
        template = ctx.file.hdr,
        output = out,
        substitutions = {},
    )
    return [CcInfo(
        compilation_context = cc_common.create_compilation_context(
            includes = depset([out.dirname]),
            headers = depset([out]),
        ),
    )]

config = rule(
    implementation = _config_impl,
    attrs = {
        "hdr": attr.label(allow_single_file = [".h"]),
    },
)

COPTS = ['-DVERSION=\\"0.1.5\\"']
