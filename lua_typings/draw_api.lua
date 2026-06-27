---@diagnostic disable: lowercase-global

--[[
Type stubs for LuaLS (Lua Language Server) — this file is NEVER executed by the
program, it only feeds the editor's autocomplete/diagnostics. It describes the
drawing functions that LuaEngine::init() registers AT RUNTIME (see
src/core/LuaEngine.cpp) as wrappers around raylib: they do not exist as "real"
Lua anywhere, so without this file the editor cannot know they exist nor what
parameters they take.

raylib is immediate-mode: each function draws a finished, colored shape right
away. Colors are always 4 numbers r,g,b,a in 0-255 (non-integers are fine: the
engine rounds and clamps).

Preset contract (see the header comment in src/core/LuaEngine.h):
  preset.init()
  preset.render(rms, centroid, onset, bands, dt, knobs, w, h)
]]

-- ===========================================================================
-- Background
-- ===========================================================================

--- Fills the WHOLE window with an opaque color (alpha implicitly 255).
---@param r number 0-255
---@param g number 0-255
---@param b number 0-255
function clearBackground(r, g, b) end

-- ===========================================================================
-- Filled 2D shapes (fill...)
-- ===========================================================================

---@param x number
---@param y number
---@param w number
---@param h number
---@param r number 0-255
---@param g number 0-255
---@param b number 0-255
---@param a number 0-255
function fillRect(x, y, w, h, r, g, b, a) end

--- roundness is 0.0-1.0 (0 = sharp corners, 1 = rounded sides), NOT a pixel
--- radius (raylib convention).
---@param x number
---@param y number
---@param w number
---@param h number
---@param roundness number 0-1
---@param r number 0-255
---@param g number 0-255
---@param b number 0-255
---@param a number 0-255
function fillRoundedRect(x, y, w, h, roundness, r, g, b, a) end

---@param cx number
---@param cy number
---@param radius number
---@param r number 0-255
---@param g number 0-255
---@param b number 0-255
---@param a number 0-255
function fillCircle(cx, cy, radius, r, g, b, a) end

---@param cx number
---@param cy number
---@param rx number
---@param ry number
---@param r number 0-255
---@param g number 0-255
---@param b number 0-255
---@param a number 0-255
function fillEllipse(cx, cy, rx, ry, r, g, b, a) end

--- Regular polygon with "sides" sides, inscribed in a circle of "radius",
--- rotated by "rotation" DEGREES.
---@param cx number
---@param cy number
---@param sides integer
---@param radius number
---@param rotation number degrees
---@param r number 0-255
---@param g number 0-255
---@param b number 0-255
---@param a number 0-255
function fillPoly(cx, cy, sides, radius, rotation, r, g, b, a) end

-- ===========================================================================
-- Outlined 2D shapes (stroke...)
-- ===========================================================================

---@param x number
---@param y number
---@param w number
---@param h number
---@param thick number pixel thickness
---@param r number 0-255
---@param g number 0-255
---@param b number 0-255
---@param a number 0-255
function strokeRect(x, y, w, h, thick, r, g, b, a) end

---@param cx number
---@param cy number
---@param radius number
---@param r number 0-255
---@param g number 0-255
---@param b number 0-255
---@param a number 0-255
function strokeCircle(cx, cy, radius, r, g, b, a) end

-- ===========================================================================
-- Lines, polylines and splines
-- ===========================================================================

---@param x1 number
---@param y1 number
---@param x2 number
---@param y2 number
---@param thick number pixel thickness
---@param r number 0-255
---@param g number 0-255
---@param b number 0-255
---@param a number 0-255
function line(x1, y1, x2, y2, thick, r, g, b, a) end

--- Joins a 1-indexed table of {x=,y=} points with STRAIGHT segments (corners).
--- The raw counterpart to spline().
---@param points table[] list of {x=number, y=number}
---@param thick number pixel thickness
---@param r number 0-255
---@param g number 0-255
---@param b number 0-255
---@param a number 0-255
function polyline(points, thick, r, g, b, a) end

--- Interpolating spline (Catmull-Rom): passes EXACTLY through every given
--- point. Same signature as polyline() so they can be compared on the same points.
---@param points table[] list of {x=number, y=number}
---@param thick number pixel thickness
---@param r number 0-255
---@param g number 0-255
---@param b number 0-255
---@param a number 0-255
function spline(points, thick, r, g, b, a) end

-- ===========================================================================
-- 2D transforms (matrix stack)
-- ===========================================================================

--- Saves the current matrix: translate/rotate/scale until popMatrix() only
--- affect what is drawn in between.
function pushMatrix() end

--- Restores the last matrix saved with pushMatrix().
function popMatrix() end

---@param x number
---@param y number
function translate(x, y) end

---@param angle number RADIANS (like Lua math)
function rotate(angle) end

---@param x number
---@param y number
function scale(x, y) end

-- ===========================================================================
-- Clipping (scissor): restricts drawing to the given rectangle
-- ===========================================================================

---@param x number
---@param y number
---@param w number
---@param h number
function beginClip(x, y, w, h) end

--- Closes the clipping area opened by beginClip().
function endClip() end

-- ===========================================================================
-- 3D: camera and mode
-- ===========================================================================

--- Sets the perspective camera used by beginMode3D(). Up vector is fixed to +Y.
---@param px number observer position X
---@param py number observer position Y
---@param pz number observer position Z
---@param tx number look-at point X
---@param ty number look-at point Y
---@param tz number look-at point Z
---@param fovy number vertical field of view in degrees
function setCamera(px, py, pz, tx, ty, tz, fovy) end

--- Opens 3D mode: everything drawn until endMode3D() is in 3D coordinates,
--- seen through the current camera.
function beginMode3D() end

--- Closes the 3D mode opened by beginMode3D().
function endMode3D() end

-- ===========================================================================
-- 3D: simple primitives (use ONLY between beginMode3D() and endMode3D())
-- ===========================================================================

---@param x number
---@param y number
---@param z number
---@param w number width
---@param h number height
---@param l number depth
---@param r number 0-255
---@param g number 0-255
---@param b number 0-255
---@param a number 0-255
function drawCube(x, y, z, w, h, l, r, g, b, a) end

--- Like drawCube but only the edges (wireframe).
---@param x number
---@param y number
---@param z number
---@param w number
---@param h number
---@param l number
---@param r number 0-255
---@param g number 0-255
---@param b number 0-255
---@param a number 0-255
function drawCubeWires(x, y, z, w, h, l, r, g, b, a) end

---@param x number
---@param y number
---@param z number
---@param radius number
---@param r number 0-255
---@param g number 0-255
---@param b number 0-255
---@param a number 0-255
function drawSphere(x, y, z, radius, r, g, b, a) end

--- Like drawSphere but wireframe only.
---@param x number
---@param y number
---@param z number
---@param radius number
---@param r number 0-255
---@param g number 0-255
---@param b number 0-255
---@param a number 0-255
function drawSphereWires(x, y, z, radius, r, g, b, a) end

---@param x number
---@param y number
---@param z number
---@param radiusTop number
---@param radiusBottom number
---@param height number
---@param r number 0-255
---@param g number 0-255
---@param b number 0-255
---@param a number 0-255
function drawCylinder(x, y, z, radiusTop, radiusBottom, height, r, g, b, a) end

---@param x number
---@param y number
---@param z number
---@param sizeX number
---@param sizeZ number
---@param r number 0-255
---@param g number 0-255
---@param b number 0-255
---@param a number 0-255
function drawPlane(x, y, z, sizeX, sizeZ, r, g, b, a) end

---@param x1 number
---@param y1 number
---@param z1 number
---@param x2 number
---@param y2 number
---@param z2 number
---@param r number 0-255
---@param g number 0-255
---@param b number 0-255
---@param a number 0-255
function drawLine3D(x1, y1, z1, x2, y2, z2, r, g, b, a) end

--- Reference grid on the XZ plane, centered at the origin.
---@param slices integer cells per side
---@param spacing number size of each cell
function drawGrid(slices, spacing) end

---@class Preset
---@field init fun()
--- bands: 6 values 0.0-1.0 (1.0 = the band is at its recent maximum, see
--- FeatureExtractor::getBands), from lowest (1) to highest (6).
--- knobs: 5 values 0.0-1.0, the potentiometers (see InputManager::getValues).
--- dt: real seconds since the previous frame (NOT a fixed step).
--- w, h: REAL window size in pixels (see Renderer::m_width/m_height), not
--- hardcoded constants.
---@field render fun(rms: number, centroid: number, onset: number, bands: number[], dt: number, knobs: number[], w: number, h: number)
