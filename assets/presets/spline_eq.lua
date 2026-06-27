local preset = {}

function preset.init()
    print("[LUA] Preset 'spline_eq' inizializzato.")
end

-- Same idea as 'bars_eq' (one point per frequency band), but the points are
-- joined with a spline instead of bars. The spline is interpolating
-- (Catmull-Rom): it passes EXACTLY through each given point. The 5 pots let you
-- compare the spline and the raw polyline on the SAME points:
--   knobs[1] -> amplitude (how much the bands push the curve up)
--   knobs[2] -> spline thickness
--   knobs[3] -> visibility of the raw comparison polyline (0 = hidden)
--   knobs[4] -> spline color (hue)
--   knobs[5] -> radius of the dots on the original points (0 = hidden)
function preset.render(rms, centroid, onset, bands, dt, knobs, W, H)
    clearBackground(8, 8, 14)

    local numBands = #bands
    local margin = 80
    local baseY = H * 0.65
    local maxHeight = H * 0.45
    local ampMul = 0.3 + (knobs[1] or 0) * 2.0   -- 0.3x - 2.3x
    local thickness = 2 + (knobs[2] or 0) * 10    -- 2 - 12 px
    local polylineAlpha = (knobs[3] or 0) * 255   -- 0 (hidden) - 255 (full)
    local hue = (knobs[4] or 0) * 255
    local dotRadius = (knobs[5] or 0) * 8         -- 0 (hidden) - 8 px

    local pts = {}
    for i = 1, numBands do
        local t = (i - 1) / (numBands - 1)
        pts[i] = {
            x = margin + t * (W - margin * 2),
            y = baseY - bands[i] * maxHeight * ampMul
        }
    end

    -- 1) Raw polyline: the same points joined by straight segments (with
    -- corners). Only there as a comparison with the spline.
    if polylineAlpha > 1 then
        polyline(pts, 1.5, 255, 255, 255, polylineAlpha)
    end

    -- 2) Spline over the same points: same path, no corners.
    spline(pts, thickness, hue, 200, 255 - hue, 255)

    -- 3) Original points: show that the spline is interpolating, passing exactly
    -- through them (unlike a B-spline, which would only use them as guides).
    if dotRadius > 0.5 then
        for _, p in ipairs(pts) do
            fillCircle(p.x, p.y, dotRadius, 255, 255, 255, 220)
        end
    end
end

return preset
