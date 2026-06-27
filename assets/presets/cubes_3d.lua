local preset = {}
local angle = 0.0  -- camera angle, accumulated over time

function preset.init()
    print("[LUA] Preset 'cubes_3d' inizializzato.")
end

-- First 3D preset: a row of cubes (one per frequency band) that grow taller with
-- their band, on a reference grid. The camera slowly orbits the scene to convey
-- depth. The 3D equivalent of 'bars_eq', showing that the same audio data can be
-- rendered in 2D or 3D with equally simple preset code.
--   knobs[1] -> sensitivity (how much the bands push the cube heights)
--   knobs[2] -> camera rotation speed
function preset.render(rms, centroid, onset, bands, dt, knobs, W, H)
    local sensitivity = 1.0 + (knobs[1] or 0) * 4.0   -- 1x - 5x
    local orbitSpeed = 0.2 + (knobs[2] or 0) * 1.5    -- 0.2 - 1.7 rad/s
    -- Accumulate the angle: changing the speed only changes how fast it spins,
    -- not the current position -> no jumps when turning the pot.
    angle = angle + orbitSpeed * dt

    -- Camera orbiting the origin at a fixed radius, looking at the center of the
    -- cube row (slightly above the plane).
    local camRadius = 12.0
    setCamera(math.cos(angle) * camRadius, 7.0, math.sin(angle) * camRadius,
              0.0, 1.5, 0.0, 60.0)

    clearBackground(8, 8, 16)

    beginMode3D()

    -- Floor grid for a sense of space.
    drawGrid(20, 1.0)

    local numBands = #bands
    local spacing = 2.0
    for i = 1, numBands do
        local height = 0.3 + bands[i] * sensitivity * 4.0
        -- Cubes centered on the origin along the X axis.
        local x = (i - (numBands + 1) / 2) * spacing
        local y = height * 0.5 -- base stays on the plane (y=0)

        -- Color from blue (low) to red (high), as in bars_eq.
        local hue = (i - 1) / (numBands - 1)
        local r = hue * 255
        local b = (1 - hue) * 255

        drawCube(x, y, 0, 1.2, height, 1.2, r, 100, b, 255)
        drawCubeWires(x, y, 0, 1.2, height, 1.2, 255, 255, 255, 60)
    end

    -- Center sphere pulsing with overall volume, above the row.
    drawSphere(0, 4.0, 0, 0.4 + rms * 1.5, 255, 255, 255, 230)

    endMode3D()
end

return preset
