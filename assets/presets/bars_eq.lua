local preset = {}

function preset.init()
    print("[LUA] Preset 'bars_eq' inizializzato.")
end

-- Shows the 6 frequency bands from FeatureExtractor::getBands() directly: one
-- bar per band, lowest (left) to highest (right). The simplest preset to show
-- that each band reacts only to its frequency range, not to overall volume.
function preset.render(rms, centroid, onset, bands, dt, knobs, W, H)
    clearBackground(10, 10, 18)

    local numBands = #bands
    local margin = 80
    local gap = 24
    local barWidth = (W - margin * 2 - gap * (numBands - 1)) / numBands
    local baseY = H - 100
    local maxBarHeight = H * 0.75

    for i = 1, numBands do
        local value = bands[i] -- 0.0-1.0, 1.0 = band's recent max (see calculateBands)
        local barHeight = value * maxBarHeight
        local x = margin + (i - 1) * (barWidth + gap)
        local y = baseY - barHeight

        -- Color from blue (low frequencies) to red (high)
        local hue = (i - 1) / (numBands - 1)
        local r = hue * 255
        local b = (1 - hue) * 255

        fillRect(x, y, barWidth, barHeight, r, 100, b, 255)
    end

    -- Baseline
    fillRect(margin, baseY, W - margin * 2, 4, 255, 255, 255, 180)
end

return preset
