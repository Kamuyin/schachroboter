package org.example.mqtt;

import org.example.settings.MqttSettings;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

import java.io.IOException;
import java.nio.file.Path;
import java.util.Properties;

import static org.junit.jupiter.api.Assertions.*;

class MqttBrokerServiceTest {

    @TempDir
    Path tempDir;

    @Test
    void buildPropertiesSetsPersistentClientExpirationAndDataPath() throws IOException {
        MqttSettings settings = new MqttSettings();
        settings.setBrokerEnabled(true);
        settings.setHost("127.0.0.1");
        settings.setPort(1999);
        settings.setWebsocketEnabled(false);

        MqttBrokerService service = new MqttBrokerService();
        Properties props = service.buildProperties(settings, tempDir);

        assertEquals("1999", props.getProperty("port"));
        assertEquals("127.0.0.1", props.getProperty("host"));
        assertEquals(Boolean.TRUE.toString(), props.getProperty("persistence_enabled"));
        assertEquals(tempDir.toString().replace("\\", "/"), props.getProperty("data_path"));
        assertEquals(Integer.MAX_VALUE + "s", props.getProperty("persistent_client_expiration"));
    }

    @Test
    void detectsMissingExpiryErrorsByMessageAndCause() {
        MqttBrokerService service = new MqttBrokerService();
        RuntimeException direct = new RuntimeException("Can't track for expiration an entity without expiry instant, client_id: controller");
        RuntimeException wrapped = new RuntimeException("wrapper", direct);

        assertTrue(service.isMissingExpiryException(direct));
        assertTrue(service.isMissingExpiryException(wrapped));
    }
}
