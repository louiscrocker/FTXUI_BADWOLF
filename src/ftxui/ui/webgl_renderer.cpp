// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file webgl_renderer.cpp
///
/// Two compile-time paths:
///   - __EMSCRIPTEN__  : actual WebGL2/GLES2 implementation.
///   - otherwise       : complete no-op stubs so native builds compile cleanly.

#include "ftxui/ui/webgl_renderer.hpp"

#ifdef __EMSCRIPTEN__

#include <GLES2/gl2.h>
#include <emscripten.h>
#include <emscripten/html5.h>

#include <chrono>
#include <cstring>
#include <string>
#include <vector>

namespace {

// ── WebGL state
// ───────────────────────────────────────────────────────────────

EMSCRIPTEN_WEBGL_CONTEXT_HANDLE g_ctx = 0;
bool g_available = false;

GLuint g_program = 0;
GLuint g_vbo = 0;

GLint g_loc_pos = -1;
GLint g_loc_color = -1;

ftxui::ui::WebGLRenderer::Stats g_stats;
double g_frame_start = 0.0;

// Each quad = 2 triangles = 6 vertices; each vertex: x,y,r,g,b,a (6 floats).
static constexpr int kFloatsPerVertex = 6;
static constexpr int kVerticesPerQuad = 6;
static constexpr int kFloatsPerQuad = kFloatsPerVertex * kVerticesPerQuad;

// ── Shader sources
// ────────────────────────────────────────────────────────────

static const char* kVertexShader = R"(
  attribute vec2 a_pos;
  attribute vec4 a_color;
  varying vec4 v_color;
  void main() {
    gl_Position = vec4(a_pos, 0.0, 1.0);
    v_color = a_color;
  }
)";

static const char* kFragShader = R"(
  precision mediump float;
  varying vec4 v_color;
  void main() {
    gl_FragColor = v_color;
  }
)";

// ── Helpers
// ───────────────────────────────────────────────────────────────────

static GLuint CompileShader(GLenum type, const char* src) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &src, nullptr);
  glCompileShader(shader);
  return shader;
}

static GLuint LinkProgram(GLuint vert, GLuint frag) {
  GLuint prog = glCreateProgram();
  glAttachShader(prog, vert);
  glAttachShader(prog, frag);
  glLinkProgram(prog);
  glDeleteShader(vert);
  glDeleteShader(frag);
  return prog;
}

// Push one quad (two triangles, six vertices) into @p buf.
// NDC coords: x in [-1,1], y in [-1,1].
static void PushQuad(std::vector<float>& buf,
                     float x0,
                     float y0,
                     float x1,
                     float y1,
                     float r,
                     float g,
                     float b,
                     float a) {
  // Triangle 1: (x0,y0) (x1,y0) (x1,y1)
  // Triangle 2: (x0,y0) (x1,y1) (x0,y1)
  const float verts[kFloatsPerQuad] = {
      x0, y0, r, g, b, a, x1, y0, r, g, b, a, x1, y1, r, g, b, a,
      x0, y0, r, g, b, a, x1, y1, r, g, b, a, x0, y1, r, g, b, a,
  };
  buf.insert(buf.end(), verts, verts + kFloatsPerQuad);
}

static void DrawQuadBatch(const std::vector<float>& buf) {
  if (buf.empty()) {
    return;
  }
  glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(buf.size() * sizeof(float)), buf.data(),
               GL_STREAM_DRAW);

  glEnableVertexAttribArray(g_loc_pos);
  glEnableVertexAttribArray(g_loc_color);

  glVertexAttribPointer(g_loc_pos, 2, GL_FLOAT, GL_FALSE,
                        kFloatsPerVertex * sizeof(float),
                        reinterpret_cast<void*>(0));
  glVertexAttribPointer(g_loc_color, 4, GL_FLOAT, GL_FALSE,
                        kFloatsPerVertex * sizeof(float),
                        reinterpret_cast<void*>(2 * sizeof(float)));

  const int numVerts = static_cast<int>(buf.size()) / kFloatsPerVertex;
  glDrawArrays(GL_TRIANGLES, 0, numVerts);

  g_stats.quads_rendered += numVerts / kVerticesPerQuad;
  g_stats.draw_calls++;
}

}  // namespace

namespace ftxui::ui {

bool WebGLRenderer::Init(const std::string& canvas_id) {
  EmscriptenWebGLContextAttributes attrs;
  emscripten_webgl_init_context_attributes(&attrs);
  attrs.majorVersion = 2;  // request WebGL2
  attrs.minorVersion = 0;
  attrs.alpha = EM_FALSE;
  attrs.depth = EM_FALSE;
  attrs.stencil = EM_FALSE;
  attrs.antialias = EM_FALSE;
  attrs.powerPreference = EM_WEBGL_POWER_PREFERENCE_DEFAULT;

  const std::string selector = "#" + canvas_id;
  g_ctx = emscripten_webgl_create_context(selector.c_str(), &attrs);
  if (g_ctx <= 0) {
    // Fallback to WebGL1
    attrs.majorVersion = 1;
    g_ctx = emscripten_webgl_create_context(selector.c_str(), &attrs);
  }
  if (g_ctx <= 0) {
    return false;
  }

  emscripten_webgl_make_context_current(g_ctx);

  GLuint vert = CompileShader(GL_VERTEX_SHADER, kVertexShader);
  GLuint frag = CompileShader(GL_FRAGMENT_SHADER, kFragShader);
  g_program = LinkProgram(vert, frag);

  g_loc_pos = glGetAttribLocation(g_program, "a_pos");
  g_loc_color = glGetAttribLocation(g_program, "a_color");

  glGenBuffers(1, &g_vbo);

  glUseProgram(g_program);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  g_available = true;
  return true;
}

bool WebGLRenderer::IsAvailable() {
  return g_available;
}

void WebGLRenderer::RenderBrailleCanvas(
    const std::vector<std::vector<bool>>& dots,
    int width,
    int height,
    uint8_t r,
    uint8_t g,
    uint8_t b) {
  if (!g_available) {
    return;
  }

  g_frame_start = emscripten_get_now();
  g_stats = {};

  const float fr = r / 255.0f;
  const float fg = g / 255.0f;
  const float fb = b / 255.0f;

  // Dot size in NDC: fill the canvas evenly.
  const float dw = 2.0f / static_cast<float>(width);
  const float dh = 2.0f / static_cast<float>(height);

  std::vector<float> buf;
  buf.reserve(static_cast<size_t>(width * height) * kFloatsPerQuad / 4);

  for (int row = 0; row < static_cast<int>(dots.size()) && row < height;
       ++row) {
    for (int col = 0; col < static_cast<int>(dots[row].size()) && col < width;
         ++col) {
      if (!dots[row][col]) {
        continue;
      }
      // Map (col,row) → NDC, flip Y so row 0 is top.
      float x0 = -1.0f + col * dw;
      float y1 = 1.0f - row * dh;
      PushQuad(buf, x0, y1 - dh, x0 + dw, y1, fr, fg, fb, 1.0f);
    }
  }
  DrawQuadBatch(buf);

  g_stats.frame_ms = emscripten_get_now() - g_frame_start;
}

void WebGLRenderer::RenderScreen(
    int cols,
    int rows,
    const std::vector<std::string>& /*lines*/,
    const std::vector<std::vector<uint32_t>>& fg_colors) {
  if (!g_available) {
    return;
  }

  g_frame_start = emscripten_get_now();
  g_stats = {};

  const float cw = 2.0f / static_cast<float>(cols);
  const float ch = 2.0f / static_cast<float>(rows);

  std::vector<float> buf;
  buf.reserve(static_cast<size_t>(cols * rows) * kFloatsPerQuad);

  for (int row = 0; row < rows && row < static_cast<int>(fg_colors.size());
       ++row) {
    for (int col = 0;
         col < cols && col < static_cast<int>(fg_colors[row].size()); ++col) {
      uint32_t packed = fg_colors[row][col];
      float fr = ((packed >> 24) & 0xFF) / 255.0f;
      float fg = ((packed >> 16) & 0xFF) / 255.0f;
      float fb = ((packed >> 8) & 0xFF) / 255.0f;
      float fa = (packed & 0xFF) / 255.0f;
      float x0 = -1.0f + col * cw;
      float y1 = 1.0f - row * ch;
      PushQuad(buf, x0, y1 - ch, x0 + cw, y1, fr, fg, fb, fa);
    }
  }
  DrawQuadBatch(buf);

  g_stats.frame_ms = emscripten_get_now() - g_frame_start;
}

void WebGLRenderer::Clear(uint8_t r, uint8_t g, uint8_t b) {
  if (!g_available) {
    return;
  }
  glClearColor(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
}

void WebGLRenderer::Present() {
  if (!g_available) {
    return;
  }
  emscripten_webgl_commit_frame();
}

void WebGLRenderer::Shutdown() {
  if (!g_available) {
    return;
  }
  glDeleteBuffers(1, &g_vbo);
  glDeleteProgram(g_program);
  emscripten_webgl_destroy_context(g_ctx);
  g_available = false;
  g_ctx = 0;
}

WebGLRenderer::Stats WebGLRenderer::GetStats() {
  return g_stats;
}

// ── WebGLScope
// ────────────────────────────────────────────────────────────────

WebGLScope::WebGLScope(const std::string& canvas_id)
    : ok(WebGLRenderer::Init(canvas_id)) {}

WebGLScope::~WebGLScope() {
  WebGLRenderer::Shutdown();
}

}  // namespace ftxui::ui

#else  // !__EMSCRIPTEN__ — native stub path

namespace ftxui::ui {

bool WebGLRenderer::Init(const std::string&) {
  return false;
}
bool WebGLRenderer::IsAvailable() {
  return false;
}
void WebGLRenderer::RenderBrailleCanvas(const std::vector<std::vector<bool>>&,
                                        int,
                                        int,
                                        uint8_t,
                                        uint8_t,
                                        uint8_t) {}
void WebGLRenderer::RenderScreen(int,
                                 int,
                                 const std::vector<std::string>&,
                                 const std::vector<std::vector<uint32_t>>&) {}
void WebGLRenderer::Clear(uint8_t, uint8_t, uint8_t) {}
void WebGLRenderer::Present() {}
void WebGLRenderer::Shutdown() {}
WebGLRenderer::Stats WebGLRenderer::GetStats() {
  return {};
}

WebGLScope::WebGLScope(const std::string& canvas_id)
    : ok(WebGLRenderer::Init(canvas_id)) {}
WebGLScope::~WebGLScope() {
  WebGLRenderer::Shutdown();
}

}  // namespace ftxui::ui

#endif  // __EMSCRIPTEN__
