package org.example.mqtt;

import org.example.settings.MqttSettings;

/**
 * Manual verification harness for {@link MqttBrokerService}.
 * Not included in production builds—kept for developers to smoke-test
 * embedded broker startup from the command line if needed.
 */
public final class MqttBrokerLauncher {
    private MqttBrokerLauncher() {
    }

    public static void main(String[] args) {
        MqttSettings settings = new MqttSettings();
        settings.setBrokerEnabled(true);
        settings.setHost("127.0.0.1");
        settings.setPort(1885);
        settings.setWebsocketEnabled(false);
        MqttBrokerService service = new MqttBrokerService();
        service.applySettings(settings);
        System.out.println("Broker started.");
    }
}
