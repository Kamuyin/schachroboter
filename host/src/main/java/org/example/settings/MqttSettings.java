package org.example.settings;

import lombok.Data;
import org.example.settings.annotations.*;

@Data
@SettingCategory(name = "MQTT Broker", description = "Embedded broker configuration", order = 5)
public class MqttSettings {

    @Setting(name = "Enable Broker", type = SettingType.CHECKBOX, order = 0)
    private boolean brokerEnabled = false;

    @Setting(name = "Bind Address", type = SettingType.TEXT_FIELD, order = 1)
    private String host = "0.0.0.0";

    @Setting(name = "Port", type = SettingType.NUMBER_SPINNER, order = 2)
    @SettingRange(min = 1024, max = 65535, step = 1)
    private int port = 1883;

    @Setting(name = "Enable WebSocket", type = SettingType.CHECKBOX, order = 3)
    private boolean websocketEnabled = false;

    @Setting(name = "WebSocket Port", type = SettingType.NUMBER_SPINNER, order = 4)
    @SettingRange(min = 1024, max = 65535, step = 1)
    private int websocketPort = 8083;

    @Setting(name = "Allow Anonymous", type = SettingType.CHECKBOX, order = 5)
    private boolean allowAnonymous = true;

    @Setting(name = "Username", type = SettingType.TEXT_FIELD, order = 6)
    private String username = "";

    @Setting(name = "Password", type = SettingType.TEXT_FIELD, order = 7)
    private String password = "";

    public MqttSettings copy() {
        MqttSettings copy = new MqttSettings();
        copy.setBrokerEnabled(brokerEnabled);
        copy.setHost(host);
        copy.setPort(port);
        copy.setWebsocketEnabled(websocketEnabled);
        copy.setWebsocketPort(websocketPort);
        copy.setAllowAnonymous(allowAnonymous);
        copy.setUsername(username);
        copy.setPassword(password);
        return copy;
    }
}
