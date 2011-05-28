function OnEvent(event, arg)
    gkey = arg
    mkey = GetMKeyState()
    if event == "G_PRESSED" and gkey == 1 and mkey == 1 then
        OutputDebugMessage("TS3_PTT_ACTIVATE")
    end
    if event == "G_RELEASED" and gkey == 1 and mkey == 1 then
        OutputDebugMessage("TS3_PTT_DEACTIVATE")
    end
    if event == "G_PRESSED" and gkey == 2 and mkey == 1 then
        OutputDebugMessage("TS3_INPUT_MUTE")
    end
    if event == "G_RELEASED" and gkey == 2 and mkey == 1 then
        OutputDebugMessage("TS3_INPUT_UNMUTE")
    end
    if event == "G_PRESSED" and gkey == 3 and mkey == 1 then
        OutputDebugMessage("TS3_INPUT_TOGGLE")
    end
    if event == "G_PRESSED" and gkey == 4 and mkey == 1 then
        OutputDebugMessage("TS3_OUTPUT_MUTE")
    end
    if event == "G_RELEASED" and gkey == 4 and mkey == 1 then
        OutputDebugMessage("TS3_OUTPUT_UNMUTE")
    end
    if event == "G_PRESSED" and gkey == 5 and mkey == 1 then
        OutputDebugMessage("TS3_OUTPUT_TOGGLE")
    end
    if event == "G_PRESSED" and gkey == 6 and mkey == 1 then
        OutputDebugMessage("TS3_AWAY_ZZZ")
    end
    if event == "G_RELEASED" and gkey == 6 and mkey == 1 then
        OutputDebugMessage("TS3_AWAY_NONE")
    end
    if event == "G_PRESSED" and gkey == 7 and mkey == 1 then
        OutputDebugMessage("TS3_AWAY_TOGGLE")
    end
end