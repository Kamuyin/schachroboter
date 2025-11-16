package org.example.settings;

import lombok.Data;

@Data
public class AppSettings {
    private DisplaySettings display = new DisplaySettings();
    private GameSettings game = new GameSettings();
    private EngineSettings engine = new EngineSettings();
    private AppearanceSettings appearance = new AppearanceSettings();
    private MqttSettings mqtt = new MqttSettings();
}
