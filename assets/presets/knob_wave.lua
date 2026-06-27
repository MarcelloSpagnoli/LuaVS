local preset = {}
local time = 0.0

function preset.init()
    print("[LUA] Preset 'knob_wave' inizializzato.")
end

-- A wave reacting to volume, with the 5 pots shaping the curve, one per effect:
--   knobs[1] -> amplitude (wave height)
--   knobs[2] -> frequency (crests across the screen)
--   knobs[3] -> scroll speed over time
--   knobs[4] -> color (hue)
--   knobs[5] -> line thickness
function preset.render(rms, centroid, onset, bands, dt, knobs, W, H)
    local speed = 0.5 + (knobs[3] or 0) * 3.0   -- 0.5x - 3.5x
    time = time + dt * speed

    clearBackground(5, 5, 10)

    local ampMul = 0.3 + (knobs[1] or 0) * 2.0  -- 0.3x - 2.3x
    local amplitude = (20 + rms * 350) * ampMul
    local cycles = 1 + (knobs[2] or 0) * 7      -- 1 - 8 crests
    local hue = (knobs[4] or 0) * 255
    local thickness = 2 + (knobs[5] or 0) * 10  -- 2 - 12 px

    local cy = H * 0.5
    local steps = 200

    -- Main wave: build the point sequence and draw it as a polyline.
    local pts = {}
    for i = 0, steps do
        local t = i / steps
        pts[i + 1] = {
            x = t * W,
            y = cy + math.sin(t * math.pi * 2 * cycles + time * 2) * amplitude
        }
    end
    polyline(pts, thickness, hue, 200, 255 - hue, 255)

    -- Second, thinner wave, phase-shifted by the spectral centroid (brighter =
    -- more shift): adds depth and shows a second audio feature without a sixth
    -- control. centroid is already 0-1 full-scale (auto-gain), scaled to 0..pi.
    local pts2 = {}
    for i = 0, steps do
        local t = i / steps
        pts2[i + 1] = {
            x = t * W,
            y = cy + math.sin(t * math.pi * 2 * cycles + time * 2 + centroid * math.pi) * amplitude * 0.6
        }
    end
    polyline(pts2, thickness * 0.5, 255 - hue, 200, hue, 160)
end

return preset
