#include "core/LuaEngine.h"
#include <raylib.h> // also defines RAD2DEG
#include <rlgl.h>   // rlPushMatrix/rlTranslatef/... for the 2D transforms
#include <iostream>
#include <vector>

LuaEngine::LuaEngine() : m_ready(false) {
    // Default 3D camera: observer at (0, 4, 10) looking at the origin, up = +Y,
    // 60 deg FOV. A preset can override it with setCamera(); this just avoids an
    // empty scene if beginMode3D() is called before configuring it.
    m_camera.position = Vector3{0.0f, 4.0f, 10.0f};
    m_camera.target = Vector3{0.0f, 0.0f, 0.0f};
    m_camera.up = Vector3{0.0f, 1.0f, 0.0f};
    m_camera.fovy = 60.0f;
    m_camera.projection = CAMERA_PERSPECTIVE;
}

LuaEngine::~LuaEngine() {}

bool LuaEngine::init() {
    try {
        m_lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::math, sol::lib::table, sol::lib::string);

        // Color channels arrive as float, not unsigned char: presets compute
        // colors with expressions like "centroid * 255", which rarely yield an
        // exact integer in floating point (e.g. 50.9999 instead of 51). A direct
        // unsigned char binding would make sol reject those "almost-integer"
        // values. makeColor clamps to 0-255 and rounds, so a preset can pass any
        // number without flooring by hand.
        auto toByte = [](float v) -> unsigned char {
            if (v < 0.0f) v = 0.0f;
            if (v > 255.0f) v = 255.0f;
            return static_cast<unsigned char>(v + 0.5f);
        };
        auto makeColor = [toByte](float r, float g, float b, float a) -> Color {
            return Color{toByte(r), toByte(g), toByte(b), toByte(a)};
        };

        // --- Background ---
        // Fills the whole window with an opaque color.
        m_lua.set_function("clearBackground", [makeColor](float r, float g, float b) {
            ClearBackground(makeColor(r, g, b, 255));
        });

        // --- Filled 2D shapes (fill...) ---
        // Immediate-mode: one call carries position and color (r,g,b,a 0-255).
        m_lua.set_function("fillRect", [makeColor](float x, float y, float w, float h,
                                                   float r, float g, float b, float a) {
            DrawRectangleRec(Rectangle{x, y, w, h}, makeColor(r, g, b, a));
        });

        // roundness is 0.0-1.0 (0 = sharp corners, 1 = fully rounded sides), not
        // a pixel radius: raylib's convention.
        m_lua.set_function("fillRoundedRect", [makeColor](float x, float y, float w, float h,
                                                          float roundness, float r, float g, float b, float a) {
            DrawRectangleRounded(Rectangle{x, y, w, h}, roundness, 16, makeColor(r, g, b, a));
        });

        m_lua.set_function("fillCircle", [makeColor](float cx, float cy, float radius,
                                                     float r, float g, float b, float a) {
            DrawCircleV(Vector2{cx, cy}, radius, makeColor(r, g, b, a));
        });

        m_lua.set_function("fillEllipse", [makeColor](float cx, float cy, float rx, float ry,
                                                      float r, float g, float b, float a) {
            DrawEllipse((int)cx, (int)cy, rx, ry, makeColor(r, g, b, a));
        });

        // Regular polygon with "sides" sides, centered at (cx,cy), inscribed in a
        // circle of "radius", rotated by "rotation" DEGREES.
        m_lua.set_function("fillPoly", [makeColor](float cx, float cy, int sides, float radius,
                                                   float rotation, float r, float g, float b, float a) {
            DrawPoly(Vector2{cx, cy}, sides, radius, rotation, makeColor(r, g, b, a));
        });

        // --- Outlined 2D shapes (stroke...) ---
        m_lua.set_function("strokeRect", [makeColor](float x, float y, float w, float h,
                                                     float thick, float r, float g, float b, float a) {
            DrawRectangleLinesEx(Rectangle{x, y, w, h}, thick, makeColor(r, g, b, a));
        });

        m_lua.set_function("strokeCircle", [makeColor](float cx, float cy, float radius,
                                                       float r, float g, float b, float a) {
            DrawCircleLines((int)cx, (int)cy, radius, makeColor(r, g, b, a));
        });

        // --- Lines, polylines and splines ---
        // Single segment with pixel thickness.
        m_lua.set_function("line", [makeColor](float x1, float y1, float x2, float y2,
                                               float thick, float r, float g, float b, float a) {
            DrawLineEx(Vector2{x1, y1}, Vector2{x2, y2}, thick, makeColor(r, g, b, a));
        });

        // Polyline: joins a 1-indexed Lua table of {x=, y=} points with straight
        // segments. Drawn segment by segment with DrawLineEx so thickness applies
        // (raylib's DrawLineStrip is always 1px). The raw counterpart to spline().
        m_lua.set_function("polyline", [makeColor](sol::table pts, float thick,
                                                   float r, float g, float b, float a) {
            const size_t n = pts.size();
            if (n < 2) return;
            const Color c = makeColor(r, g, b, a);
            for (size_t i = 1; i < n; ++i) {
                sol::table p0 = pts[i];
                sol::table p1 = pts[i + 1];
                Vector2 v0{p0["x"].get<float>(), p0["y"].get<float>()};
                Vector2 v1{p1["x"].get<float>(), p1["y"].get<float>()};
                DrawLineEx(v0, v1, thick, c);
            }
        });

        // Interpolating spline (Catmull-Rom): passes EXACTLY through every point
        // (unlike a B-spline, which only uses them as guides). Same signature as
        // polyline() so they can be compared on the same points. The endpoints
        // are duplicated so the curve also reaches the first and last point
        // (Catmull-Rom needs a neighbor on each side).
        m_lua.set_function("spline", [makeColor](sol::table pts, float thick,
                                                 float r, float g, float b, float a) {
            const size_t n = pts.size();
            if (n < 2) return;
            std::vector<Vector2> v;
            v.reserve(n + 2);
            {
                sol::table first = pts[1];
                v.push_back(Vector2{first["x"].get<float>(), first["y"].get<float>()});
            }
            for (size_t i = 1; i <= n; ++i) {
                sol::table p = pts[i];
                v.push_back(Vector2{p["x"].get<float>(), p["y"].get<float>()});
            }
            {
                sol::table last = pts[n];
                v.push_back(Vector2{last["x"].get<float>(), last["y"].get<float>()});
            }
            DrawSplineCatmullRom(v.data(), (int)v.size(), thick, makeColor(r, g, b, a));
        });

        // --- 2D transforms (matrix stack via rlgl) ---
        // pushMatrix/popMatrix save and restore the current matrix; translate/
        // rotate/scale in between affect only what is drawn there. rotate's angle
        // is in RADIANS (like Lua math); conversion to degrees happens here.
        m_lua.set_function("pushMatrix", []() { rlPushMatrix(); });
        m_lua.set_function("popMatrix", []() { rlPopMatrix(); });
        m_lua.set_function("translate", [](float x, float y) { rlTranslatef(x, y, 0.0f); });
        m_lua.set_function("rotate", [](float angle) { rlRotatef(angle * RAD2DEG, 0.0f, 0.0f, 1.0f); });
        m_lua.set_function("scale", [](float x, float y) { rlScalef(x, y, 1.0f); });

        // --- Clipping (scissor) ---
        // Restricts drawing to the given rectangle. Always close with endClip().
        m_lua.set_function("beginClip", [](float x, float y, float w, float h) {
            BeginScissorMode((int)x, (int)y, (int)w, (int)h);
        });
        m_lua.set_function("endClip", []() { EndScissorMode(); });

        // --- 3D: camera and mode ---
        // Sets the perspective camera used by beginMode3D(): observer position
        // (px,py,pz), look-at point (tx,ty,tz), vertical FOV in degrees. Up = +Y.
        m_lua.set_function("setCamera", [this](float px, float py, float pz,
                                               float tx, float ty, float tz, float fovy) {
            m_camera.position = Vector3{px, py, pz};
            m_camera.target = Vector3{tx, ty, tz};
            m_camera.up = Vector3{0.0f, 1.0f, 0.0f};
            m_camera.fovy = fovy;
            m_camera.projection = CAMERA_PERSPECTIVE;
        });

        // Everything drawn between beginMode3D() and endMode3D() is in 3D, seen
        // through the current camera. The 3D primitives below go only inside it.
        m_lua.set_function("beginMode3D", [this]() { BeginMode3D(m_camera); });
        m_lua.set_function("endMode3D", []() { EndMode3D(); });

        // --- 3D: simple primitives ---
        m_lua.set_function("drawCube", [makeColor](float x, float y, float z,
                                                   float w, float h, float l,
                                                   float r, float g, float b, float a) {
            DrawCube(Vector3{x, y, z}, w, h, l, makeColor(r, g, b, a));
        });

        m_lua.set_function("drawCubeWires", [makeColor](float x, float y, float z,
                                                        float w, float h, float l,
                                                        float r, float g, float b, float a) {
            DrawCubeWires(Vector3{x, y, z}, w, h, l, makeColor(r, g, b, a));
        });

        m_lua.set_function("drawSphere", [makeColor](float x, float y, float z, float radius,
                                                     float r, float g, float b, float a) {
            DrawSphere(Vector3{x, y, z}, radius, makeColor(r, g, b, a));
        });

        m_lua.set_function("drawSphereWires", [makeColor](float x, float y, float z, float radius,
                                                          float r, float g, float b, float a) {
            // Fixed rings/slices at 12: enough to read the shape without cost.
            DrawSphereWires(Vector3{x, y, z}, radius, 12, 12, makeColor(r, g, b, a));
        });

        m_lua.set_function("drawCylinder", [makeColor](float x, float y, float z,
                                                       float radiusTop, float radiusBottom, float height,
                                                       float r, float g, float b, float a) {
            DrawCylinder(Vector3{x, y, z}, radiusTop, radiusBottom, height, 16, makeColor(r, g, b, a));
        });

        m_lua.set_function("drawPlane", [makeColor](float x, float y, float z,
                                                    float sizeX, float sizeZ,
                                                    float r, float g, float b, float a) {
            DrawPlane(Vector3{x, y, z}, Vector2{sizeX, sizeZ}, makeColor(r, g, b, a));
        });

        m_lua.set_function("drawLine3D", [makeColor](float x1, float y1, float z1,
                                                     float x2, float y2, float z2,
                                                     float r, float g, float b, float a) {
            DrawLine3D(Vector3{x1, y1, z1}, Vector3{x2, y2, z2}, makeColor(r, g, b, a));
        });

        // Reference grid on the XZ plane (centered at origin): "slices" cells per
        // side, "spacing" the size of each cell. Gives a sense of 3D space.
        m_lua.set_function("drawGrid", [](int slices, float spacing) {
            DrawGrid(slices, spacing);
        });

        std::cout << "[OK] LuaJIT Engine (sol2) pronto (API raylib 2D + 3D)." << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "[LUA ERRORE INIZIALIZZAZIONE] " << e.what() << std::endl;
        return false;
    }
}

bool LuaEngine::loadPreset(const std::string& filename) {
    try {
        sol::protected_function_result result = m_lua.script_file(filename);

        if (!result.valid()) {
            sol::error err = result;
            std::cerr << "[LUA ERRORE CARICAMENTO] " << err.what() << std::endl;
            return false;
        }

        if (result.return_count() > 0 && result.get_type() == sol::type::table) {
            m_currentPreset = result;
            m_ready = true;

            sol::protected_function initFunc = m_currentPreset["init"];
            if (initFunc.valid()) initFunc();

            return true;
        }

        std::cerr << "[LUA ERRORE] Il preset non ha ritornato una table valida." << std::endl;
        return false;
    }
    catch (const std::exception& e) {
        std::cerr << "[LUA ERRORE CRASH] " << e.what() << std::endl;
        return false;
    }
}

void LuaEngine::updateAndRender(float rms, float centroid, float onset,
                                const std::vector<float>& bands, float dt, const std::vector<float>& knobs,
                                float width, float height) {
    if (!m_ready) return;

    sol::protected_function renderFunc = m_currentPreset["render"];
    if (!renderFunc.valid()) return;

    auto result = renderFunc(rms, centroid, onset, sol::as_table(bands), dt,
                              sol::as_table(knobs), width, height);

    if (!result.valid()) {
        sol::error err = result;
        std::cerr << "[LUA RUNTIME ERRORE] " << err.what() << std::endl;
    }
}
