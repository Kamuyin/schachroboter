package org.example.settings;

import lombok.Data;
import org.example.settings.annotations.*;

@Data
@SettingCategory(name = "Appearance", description = "UI appearance settings", order = 4)
public class AppearanceSettings {
    
    @Setting(name = "Window Theme", type = SettingType.COMBO_BOX, order = 1)
    @SettingOptions(values = {"Light", "Dark", "System"})
    private String theme = "System";

    @Setting(name = "Font Size", type = SettingType.NUMBER_SPINNER, order = 2)
    @SettingRange(min = 10, max = 24, step = 2)
    private int fontSize = 14;

    @Setting(name = "Show Toolbar", type = SettingType.CHECKBOX, order = 3)
    private boolean showToolbar = true;

    @Setting(name = "Show Status Bar", type = SettingType.CHECKBOX, order = 4)
    private boolean showStatusBar = true;
}
