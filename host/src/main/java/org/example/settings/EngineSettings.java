package org.example.settings;

import lombok.Data;
import org.example.settings.annotations.*;

@Data
@SettingCategory(name = "Engine", description = "Chess engine configuration", order = 3)
public class EngineSettings {
    
    @Setting(name = "Enable Engine", type = SettingType.CHECKBOX, order = 0)
    private boolean engineEnabled = false;

    @Setting(name = "Stockfish Path", type = SettingType.FILE_CHOOSER, order = 1)
    private String stockfishPath = "";

    @Setting(name = "Engine Plays As", type = SettingType.COMBO_BOX, order = 2)
    @SettingOptions(values = {"Disabled", "White", "Black"})
    private String enginePlayAs = "Disabled";

    @Setting(name = "Thinking Time (ms)", type = SettingType.NUMBER_SPINNER, order = 3)
    @SettingRange(min = 100, max = 10000, step = 100)
    private int thinkingTimeMs = 1000;

    @Setting(name = "Skill Level", type = SettingType.SLIDER, order = 4)
    @SettingRange(min = 0, max = 20, step = 1)
    private int skillLevel = 20;

    @Setting(name = "Threads", type = SettingType.NUMBER_SPINNER, order = 5)
    @SettingRange(min = 1, max = 16, step = 1)
    private int threads = 1;

    @Setting(name = "Hash Size (MB)", type = SettingType.NUMBER_SPINNER, order = 6)
    @SettingRange(min = 16, max = 4096, step = 16)
    private int hashSizeMB = 128;
}
