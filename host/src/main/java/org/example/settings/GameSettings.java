package org.example.settings;

import lombok.Data;
import org.example.settings.annotations.*;

@Data
@SettingCategory(name = "Game", description = "Game play settings", order = 2)
public class GameSettings {
    
    @Setting(name = "Enable Sound", type = SettingType.CHECKBOX, order = 1)
    private boolean enableSound = false;

    @Setting(name = "Auto-Queen Promotion", type = SettingType.CHECKBOX, order = 2)
    private boolean autoQueen = false;

    @Setting(name = "Confirm Moves", type = SettingType.CHECKBOX, order = 3)
    private boolean confirmMoves = false;

    @Setting(name = "Time Control Minutes", type = SettingType.NUMBER_SPINNER, order = 4)
    @SettingRange(min = 1, max = 180, step = 1)
    private int timeControlMinutes = 10;

    @Setting(name = "Time Control Increment (sec)", type = SettingType.NUMBER_SPINNER, order = 5)
    @SettingRange(min = 0, max = 60, step = 1)
    private int timeControlIncrement = 0;

    @Setting(name = "Enable Time Control", type = SettingType.CHECKBOX, order = 6)
    private boolean enableTimeControl = false;
}
