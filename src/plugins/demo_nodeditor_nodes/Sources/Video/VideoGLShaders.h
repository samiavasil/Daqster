#pragma once

// SPDX-License-Identifier: MIT
//
// GLSL source builders shared by the video GL paths (VideoGLBlitWidget and
// VideoEffectGLProcessor). Extracted from VideoGLBlitWidget.cpp so the effect
// processor can reuse the exact same vertex/YUV->RGB shader sources.
//
// All builders are `inline` free functions in a header-only unit — no state,
// no QObject, safe to include from multiple translation units.

#include <QString>

/// Vertex shader: fullscreen quad passthrough. Core profile (>= 3.0) uses
/// "#version 150" + `in`/`out`; compatibility profile uses "#version 120" +
/// `attribute`/`varying`.
inline QString buildVertexSource(bool core)
{
    if (core) {
        return QStringLiteral(
            "#version 150 core\n"
            "in vec2 a_position;\n"
            "in vec2 a_texcoord;\n"
            "out vec2 v_texcoord;\n"
            "void main() {\n"
            "  gl_Position = vec4(a_position, 0.0, 1.0);\n"
            "  v_texcoord = a_texcoord;\n"
            "}\n");
    }
    return QStringLiteral(
        "#version 120\n"
        "attribute vec2 a_position;\n"
        "attribute vec2 a_texcoord;\n"
        "varying vec2 v_texcoord;\n"
        "void main() {\n"
        "  gl_Position = vec4(a_position, 0.0, 1.0);\n"
        "  v_texcoord = a_texcoord;\n"
        "}\n");
}

/// YUV->RGB fragment shader. UV channel swizzle is baked per upload layout:
/// core GL_RG => ".rg", compat GL_LUMINANCE_ALPHA => ".ra" (R=U, A=V).
inline QString buildYuvFragmentSource(bool core, bool nv12)
{
    QString src;
    if (core) {
        src += QStringLiteral(
            "#version 150 core\n"
            "uniform sampler2D u_texY;\n");
        if (nv12)
            src += QStringLiteral("uniform sampler2D u_texUV;\n");
        else
            src += QStringLiteral("uniform sampler2D u_texU;\nuniform sampler2D u_texV;\n");
        src += QStringLiteral(
            "uniform int u_matrix;\n"
            "uniform int u_range;\n"
            "in vec2 v_texcoord;\n"
            "out vec4 fragColor;\n");
    } else {
        src += QStringLiteral(
            "#version 120\n"
            "uniform sampler2D u_texY;\n");
        if (nv12)
            src += QStringLiteral("uniform sampler2D u_texUV;\n");
        else
            src += QStringLiteral("uniform sampler2D u_texU;\nuniform sampler2D u_texV;\n");
        src += QStringLiteral(
            "uniform int u_matrix;\n"
            "uniform int u_range;\n"
            "varying vec2 v_texcoord;\n");
    }

    src += QStringLiteral("void main() {\n");
    if (core) {
        if (nv12) {
            src += QStringLiteral(
                "  float y = texture(u_texY, v_texcoord).r;\n"
                "  vec2 uv = texture(u_texUV, v_texcoord).rg;\n");
        } else {
            src += QStringLiteral(
                "  float y = texture(u_texY, v_texcoord).r;\n"
                "  vec2 uv = vec2(texture(u_texU, v_texcoord).r,\n"
                "                 texture(u_texV, v_texcoord).r);\n");
        }
    } else {
        if (nv12) {
            src += QStringLiteral(
                "  float y = texture2D(u_texY, v_texcoord).r;\n"
                "  vec2 uv = texture2D(u_texUV, v_texcoord).ra;\n");
        } else {
            src += QStringLiteral(
                "  float y = texture2D(u_texY, v_texcoord).r;\n"
                "  vec2 uv = vec2(texture2D(u_texU, v_texcoord).r,\n"
                "                 texture2D(u_texV, v_texcoord).r);\n");
        }
    }

    src += QStringLiteral(
        "  if (u_range == 0) {\n"
        "    y = (y - 0.0627451) * 1.1643836;\n"
        "    uv = (uv - vec2(0.5019608)) * 1.1383929;\n"
        "  }\n"
        "  vec3 rgb;\n"
        "  if (u_matrix == 1) {\n"
        "    rgb = vec3(y + 1.5748 * (uv.y - 0.5),\n"
        "               y - 0.187324 * (uv.x - 0.5) - 0.468124 * (uv.y - 0.5),\n"
        "               y + 1.8556 * (uv.x - 0.5));\n"
        "  } else {\n"
        "    rgb = vec3(y + 1.402 * (uv.y - 0.5),\n"
        "               y - 0.344136 * (uv.x - 0.5) - 0.714136 * (uv.y - 0.5),\n"
        "               y + 1.772 * (uv.x - 0.5));\n"
        "  }\n");
    src += core ? QStringLiteral("  fragColor = vec4(rgb, 1.0);\n}\n")
                : QStringLiteral("  gl_FragColor = vec4(rgb, 1.0);\n}\n");
    return src;
}

/// RGBA/BGRA fragment shader (QImage path / RGB-format frames).
inline QString buildRgbaFragmentSource(bool core)
{
    if (core) {
        return QStringLiteral(
            "#version 150 core\n"
            "uniform sampler2D u_tex;\n"
            "in vec2 v_texcoord;\n"
            "out vec4 fragColor;\n"
            "void main() { fragColor = texture(u_tex, v_texcoord); }\n");
    }
    return QStringLiteral(
        "#version 120\n"
        "uniform sampler2D u_tex;\n"
        "varying vec2 v_texcoord;\n"
        "void main() { gl_FragColor = texture2D(u_tex, v_texcoord); }\n");
}

/// YUV->RGB fragment shader with an injected effect body (VideoEffectNode GPU
/// backend). Same base as buildYuvFragmentSource() plus:
///   - `uniform int u_flipY` — when non-zero the texture coordinate is flipped
///     vertically (`tc.y = 1.0 - tc.y`) before sampling. The flip effect uses
///     this; other effects leave it 0.
///   - `uniform float u_brightness` / `uniform float u_contrast` — declared
///     unconditionally so effect bodies may reference them; the processor sets
///     them only when the effect uses them (harmless when unused).
///   - `effectBody` — GLSL statements injected after the YUV->RGB conversion,
///     operating on the local `vec3 rgb` variable, right before the final
///     color write. Effect-specific uniforms are declared in the base above.
inline QString buildEffectFragmentSource(bool core, bool nv12, const QString &effectBody)
{
    QString src;
    if (core) {
        src += QStringLiteral(
            "#version 150 core\n"
            "uniform sampler2D u_texY;\n");
        if (nv12)
            src += QStringLiteral("uniform sampler2D u_texUV;\n");
        else
            src += QStringLiteral("uniform sampler2D u_texU;\nuniform sampler2D u_texV;\n");
        src += QStringLiteral(
            "uniform int u_matrix;\n"
            "uniform int u_range;\n"
            "uniform int u_flipY;\n"
            "uniform float u_brightness;\n"
            "uniform float u_contrast;\n"
            "in vec2 v_texcoord;\n"
            "out vec4 fragColor;\n");
    } else {
        src += QStringLiteral(
            "#version 120\n"
            "uniform sampler2D u_texY;\n");
        if (nv12)
            src += QStringLiteral("uniform sampler2D u_texUV;\n");
        else
            src += QStringLiteral("uniform sampler2D u_texU;\nuniform sampler2D u_texV;\n");
        src += QStringLiteral(
            "uniform int u_matrix;\n"
            "uniform int u_range;\n"
            "uniform int u_flipY;\n"
            "uniform float u_brightness;\n"
            "uniform float u_contrast;\n"
            "varying vec2 v_texcoord;\n");
    }

    src += QStringLiteral("void main() {\n");
    src += QStringLiteral(
        "  vec2 tc = v_texcoord;\n"
        "  if (u_flipY != 0) tc.y = 1.0 - tc.y;\n");
    if (core) {
        if (nv12) {
            src += QStringLiteral(
                "  float y = texture(u_texY, tc).r;\n"
                "  vec2 uv = texture(u_texUV, tc).rg;\n");
        } else {
            src += QStringLiteral(
                "  float y = texture(u_texY, tc).r;\n"
                "  vec2 uv = vec2(texture(u_texU, tc).r,\n"
                "                 texture(u_texV, tc).r);\n");
        }
    } else {
        if (nv12) {
            src += QStringLiteral(
                "  float y = texture2D(u_texY, tc).r;\n"
                "  vec2 uv = texture2D(u_texUV, tc).ra;\n");
        } else {
            src += QStringLiteral(
                "  float y = texture2D(u_texY, tc).r;\n"
                "  vec2 uv = vec2(texture2D(u_texU, tc).r,\n"
                "                 texture2D(u_texV, tc).r);\n");
        }
    }

    src += QStringLiteral(
        "  if (u_range == 0) {\n"
        "    y = (y - 0.0627451) * 1.1643836;\n"
        "    uv = (uv - vec2(0.5019608)) * 1.1383929;\n"
        "  }\n"
        "  vec3 rgb;\n"
        "  if (u_matrix == 1) {\n"
        "    rgb = vec3(y + 1.5748 * (uv.y - 0.5),\n"
        "               y - 0.187324 * (uv.x - 0.5) - 0.468124 * (uv.y - 0.5),\n"
        "               y + 1.8556 * (uv.x - 0.5));\n"
        "  } else {\n"
        "    rgb = vec3(y + 1.402 * (uv.y - 0.5),\n"
        "               y - 0.344136 * (uv.x - 0.5) - 0.714136 * (uv.y - 0.5),\n"
        "               y + 1.772 * (uv.x - 0.5));\n"
        "  }\n");
    if (!effectBody.isEmpty())
        src += effectBody;
    src += core ? QStringLiteral("  fragColor = vec4(rgb, 1.0);\n}\n")
                : QStringLiteral("  gl_FragColor = vec4(rgb, 1.0);\n}\n");
    return src;
}