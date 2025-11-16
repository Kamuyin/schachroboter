package org.example.mqtt;

import io.moquette.broker.Server;
import io.moquette.broker.config.IConfig;
import io.moquette.broker.config.MemoryConfig;
import io.moquette.broker.security.IAuthenticator;
import io.moquette.broker.security.PermitAllAuthorizatorPolicy;
import io.moquette.interception.InterceptHandler;
import org.example.settings.MqttSettings;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Collections;
import java.util.Objects;
import java.util.Properties;

public class MqttBrokerService {
    private static final Logger logger = LoggerFactory.getLogger(MqttBrokerService.class);


    private static final String PROP_PORT = "port";
    private static final String PROP_HOST = "host";
    private static final String PROP_ALLOW_ANONYMOUS = "allow_anonymous";
    private static final String PROP_NEED_CLIENT_AUTH = "need_client_auth";
    private static final String PROP_DATA_PATH = "data_path";
    private static final String PROP_PERSISTENCE_ENABLED = "persistence_enabled";
    private static final String PROP_WS_ENABLED = "websocket_enabled";
    private static final String PROP_WS_PORT = "websocket_port";
    private static final String PROP_WS_PATH = "websocket_path";

    public enum BrokerChange {
        STARTED,
        RESTARTED,
        STOPPED,
        NO_CHANGE
    }

    public record RuntimeInfo(String host, int port, boolean websocketEnabled, int websocketPort) { }

    public record BrokerStatus(BrokerChange change, RuntimeInfo runtimeInfo) { }

    private Server server;
    private boolean running;
    private RuntimeInfo runtimeInfo;
    private MqttSettings cachedSettings;

    public synchronized BrokerStatus applySettings(MqttSettings settings) {
        logger.debug("Applying MQTT broker settings");
        if (settings == null || !settings.isBrokerEnabled()) {
            if (running) {
                logger.info("Stopping MQTT broker (disabled by settings)");
                stopInternal();
                return new BrokerStatus(BrokerChange.STOPPED, null);
            }
            return new BrokerStatus(BrokerChange.NO_CHANGE, null);
        }

        MqttSettings snapshot = settings.copy();
        BrokerChange change = running ? BrokerChange.RESTARTED : BrokerChange.STARTED;

        if (running && Objects.equals(snapshot, cachedSettings)) {
            logger.debug("MQTT broker settings unchanged");
            return new BrokerStatus(BrokerChange.NO_CHANGE, runtimeInfo);
        }

        if (running) {
            logger.info("Restarting MQTT broker with new settings");
            stopInternal();
        }

        startInternal(snapshot);
        cachedSettings = snapshot;
        return new BrokerStatus(change, runtimeInfo);
    }

    public synchronized boolean isRunning() {
        return running;
    }

    public synchronized RuntimeInfo getRuntimeInfo() {
        return runtimeInfo;
    }

    public synchronized void shutdown() {
        logger.info("Shutting down MQTT broker service");
        stopInternal();
    }

    private void startInternal(MqttSettings settings) {
        logger.info("Starting MQTT broker on {}:{}", settings.getHost(), settings.getPort());
        try {
            Properties props = buildProperties(settings);
            IConfig config = new MemoryConfig(props);
            server = new Server();
            IAuthenticator authenticator = settings.isAllowAnonymous() ? null : buildAuthenticator(settings);
            server.startServer(config, Collections.<InterceptHandler>emptyList(), null, authenticator, new PermitAllAuthorizatorPolicy());
            running = true;
            runtimeInfo = new RuntimeInfo(settings.getHost(), settings.getPort(), settings.isWebsocketEnabled(), settings.getWebsocketPort());
            logger.info("MQTT broker started successfully");
        } catch (Exception ex) {
            logger.error("Failed to start MQTT broker", ex);
            stopInternal();
            throw new IllegalStateException("Failed to start MQTT broker: " + ex.getMessage(), ex);
        }
    }

    private void stopInternal() {
        if (server != null) {
            logger.debug("Stopping MQTT broker");
            try {
                server.stopServer();
                logger.info("MQTT broker stopped");
            } catch (Exception ignored) {
                logger.warn("Exception while stopping MQTT broker", ignored);
            }
        }
        server = null;
        running = false;
        runtimeInfo = null;
        cachedSettings = null;
    }

    private Properties buildProperties(MqttSettings settings) throws IOException {
        Properties props = new Properties();
    props.setProperty(PROP_PORT, String.valueOf(settings.getPort()));
    props.setProperty(PROP_HOST, settings.getHost());
    props.setProperty(PROP_ALLOW_ANONYMOUS, String.valueOf(settings.isAllowAnonymous()));
    props.setProperty(PROP_NEED_CLIENT_AUTH, String.valueOf(!settings.isAllowAnonymous()));

        Path brokerDir = Paths.get(System.getProperty("user.home"), ".schachroboter", "mqtt");
        Files.createDirectories(brokerDir);
    String normalizedDataPath = brokerDir.toString().replace("\\", "/");
    props.setProperty(PROP_DATA_PATH, normalizedDataPath);
    props.setProperty(PROP_PERSISTENCE_ENABLED, Boolean.TRUE.toString());

        if (settings.isWebsocketEnabled()) {
            props.setProperty(PROP_WS_ENABLED, Boolean.TRUE.toString());
            props.setProperty(PROP_WS_PORT, String.valueOf(settings.getWebsocketPort()));
            props.setProperty(PROP_WS_PATH, "/mqtt");
        } else {
            props.setProperty(PROP_WS_ENABLED, Boolean.FALSE.toString());
        }
        return props;
    }

    private IAuthenticator buildAuthenticator(MqttSettings settings) {
        String expectedUser = settings.getUsername();
        String expectedPassword = settings.getPassword();
        if (expectedUser == null || expectedUser.isBlank()) {
            throw new IllegalArgumentException("Username must be provided when anonymous access is disabled.");
        }
        return (clientId, username, password) -> {
            if (username == null) {
                return false;
            }
            if (!expectedUser.equals(username)) {
                return false;
            }
            String providedPassword = password == null ? "" : new String(password, StandardCharsets.UTF_8);
            return Objects.equals(expectedPassword, providedPassword);
        };
    }
}
