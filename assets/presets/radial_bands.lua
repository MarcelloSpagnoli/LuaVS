local time = 0.0

local preset = {}

function preset.init()
    print("[LUA] Preset 'radial_bands' inizializzato.")
end

function preset.render(rms, centroid, onset, bands, dt, knobs, W, H)
    time = time + dt
    -- Computed each frame (not at file scope) because it depends on the real
    -- W/H passed by the engine: see the comment in LuaEngine.h
    local cx, cy = W * 0.5, H * 0.5
    local minDim = math.min(W, H)

    local numBands = #bands
    local baseRadius = minDim * 0.06 + rms * minDim * 0.06   -- max 0.12*minDim
    -- centroid already arrives normalized 0-1 full-scale (auto-gain in
    -- FeatureExtractor::calculateCentroid): mapped straight to hue 0-255.
    local baseHue = centroid * 255

    for i = 1, numBands do
        local angle = (i - 1) / numBands * math.pi * 2 + time * 0.2
        -- petalRadius max: 0.12 + 0.25 = 0.37*minDim
        local petalRadius = baseRadius + bands[i] * minDim * 0.25
        -- circle radius max: 0.05*minDim. Max sum with the petal:
        -- 0.37+0.05 = 0.42*minDim, below 0.5*minDim (the edge): safety margin so
        -- nothing is ever clipped off-screen.
        local circleRadius = minDim * 0.015 + bands[i] * minDim * 0.035

        local x = cx + math.cos(angle) * petalRadius
        local y = cy + math.sin(angle) * petalRadius

        fillCircle(x, y, circleRadius, baseHue, 180, 255 - baseHue, 200)
    end

    -- Center circle: pulses with overall volume (max 0.065*minDim)
    fillCircle(cx, cy, minDim * 0.02 + rms * minDim * 0.045, 255, 255, 255, 220)
end

return preset
