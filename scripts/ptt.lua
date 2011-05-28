function OnEvent(event, arg)
    gkey = arg
    mkey = GetMKeyState()
    if event == "G_PRESSED" and gkey == 1 and mkey == 1 then
        OutputDebugMessage("TS3_PTT_ACTIVATE")
    end
    if event == "G_RELEASED" and gkey == 1 and mkey == 1 then
        OutputDebugMessage("TS3_PTT_DEACTIVATE")
    end
end