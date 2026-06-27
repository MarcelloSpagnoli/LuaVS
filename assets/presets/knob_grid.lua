local preset = {}
local rotationAngle = 0.0

function preset.init()
    print("[LUA] Preset 'knob_grid' inizializzato.")
end

-- Grid of circles (one column per frequency band), made to try the 5 pots: each
-- knob controls ONE isolated visual effect, so its job is obvious at a touch.
--   knobs[1] -> sensitivity (how much the bands push the circle radius)
--   knobs[2] -> color (hue, rotates the whole spectrum)
--   knobs[3] -> base circle size
--   knobs[4] -> pattern rotation speed
--   knobs[5] -> background brightness
function preset.render(rms, centroid, onset, bands, dt, knobs, W, H)
    local bg = (knobs[5] or 0) * 40
    clearBackground(bg, bg, bg + 8)

    local sensitivity = 0.5 + (knobs[1] or 0) * 2.5   -- 0.5x - 3x
    local baseHue = (knobs[2] or 0) * 255
    local baseSize = 14 + (knobs[3] or 0) * 36        -- 14 - 50 px
    local rotSpeed = (knobs[4] or 0) * 2.0            -- 0 - 2 rad/s
    -- Accumulate the angle a bit per frame (rotSpeed*dt), NOT "time*rotSpeed":
    -- multiplying by total elapsed time would amplify any pot jitter more and
    -- more as the minutes pass.
    rotationAngle = rotationAngle + rotSpeed * dt

    local cx, cy = W * 0.5, H * 0.5
    local numBands = #bands
    local cols, rows = numBands, 5
    local spacingX = (W * 0.6) / cols
    local spacingY = (H * 0.6) / rows

    -- pushMatrix/translate/rotate move the origin to the center and rotate the
    -- whole pattern: each circle below is drawn relative to the center, and
    -- popMatrix restores the matrix at the end of the block.
    pushMatrix()
    translate(cx, cy)
    rotate(rotationAngle)

    for col = 1, cols do
        local value = bands[col] * sensitivity
        for row = 1, rows do
            local x = (col - (cols + 1) / 2) * spacingX
            local y = (row - (rows + 1) / 2) * spacingY
            local r = baseSize * (0.3 + value * 0.7)

            local hue = (baseHue + col * 20) % 255
            fillCircle(x, y, r, hue, 200 - hue * 0.3, 255 - hue, 180 + value * 75)
        end
    end

    popMatrix()

    -- Center circle: pulses with overall volume, always visible (also handy to
    -- check at a glance whether rms is coming through).
    fillCircle(cx, cy, 20 + rms * sensitivity * 60, 255, 255, 255, 200)
end

return preset
