package org.example.settings;

import lombok.Data;
import org.example.settings.annotations.*;

@Data
@SettingCategory(name = "Display", description = "Display and visual settings", order = 1)
public class DisplaySettings {
    
    @Setting(name = "Show Legal Moves", type = SettingType.CHECKBOX, order = 1)
    private boolean showLegalMoves = true;

    @Setting(name = "Show Coordinates", type = SettingType.CHECKBOX, order = 2)
    private boolean showCoordinates = true;

    @Setting(name = "Highlight Last Move", type = SettingType.CHECKBOX, order = 3)
    private boolean highlightLastMove = true;

    @Setting(name = "Show Move Hints", type = SettingType.CHECKBOX, order = 4)
    private boolean showMoveHints = false;

    @Setting(name = "Board Theme", type = SettingType.COMBO_BOX, order = 5)
    @SettingOptions(values = {"Classic", "Wood", "Blue", "Green"})
    private String boardTheme = "Classic";

    @Setting(name = "Square Size", type = SettingType.NUMBER_SPINNER, order = 6)
    @SettingRange(min = 40, max = 100, step = 5)
    private int squareSize = 65;
}
