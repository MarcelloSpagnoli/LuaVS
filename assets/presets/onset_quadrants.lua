local preset = {}

local active = 0          -- currently lit quadrant (1-4), 0 = none yet
local prevOnset = false   -- previous frame's onset, for rising-edge detection

function preset.init()
    print("[LUA] Preset 'onset_quadrants' inizializzato.")
end

-- Four quadrants; each onset advances the lit one (1 -> 2 -> 3 -> 4 -> 1 ...).
-- Reacts only to onset, ignores rms/bands/centroid/knobs. Rising edge so a hit
-- counts once even if onset stays true for several frames.
function preset.render(rms, centroid, onset, bands, dt, knobs, W, H)
    clearBackground(5, 5, 8)

    local hit = onset > 0.5
    if hit and not prevOnset then active = active % 4 + 1 end
    prevOnset = hit

    local hw, hh = W * 0.5, H * 0.5
    for i = 1, 4 do
        local x = (i == 2 or i == 4) and hw or 0
        local y = (i >= 3) and hh or 0
        if i == active then
            fillRect(x, y, hw, hh, 255, 255, 255, 255) -- lit
        else
            fillRect(x, y, hw, hh, 25, 25, 35, 255)    -- dim
        end
    end
end

return preset
