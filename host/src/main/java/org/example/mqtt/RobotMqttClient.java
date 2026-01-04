package org.example.mqtt;

import com.google.gson.Gson;
import org.eclipse.paho.client.mqttv3.*;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.concurrent.ConcurrentHashMap;
import java.util.function.Consumer;

public class RobotMqttClient {
    private static final Logger logger = LoggerFactory.getLogger(RobotMqttClient.class);
    
    private MqttClient mqttClient;
    private final ConcurrentHashMap<String, Consumer<String>> messageHandlers = new ConcurrentHashMap<>();
    private final Gson gson = new Gson();
    private boolean connected = false;
    
    public RobotMqttClient() {
    }
    
    public void connect(String brokerUrl, String clientId) throws MqttException {
        logger.info("Connecting to MQTT broker at: {}", brokerUrl);
        mqttClient = new MqttClient(brokerUrl, clientId, null);
        
        MqttConnectOptions options = new MqttConnectOptions();
        options.setCleanSession(true);
        options.setAutomaticReconnect(true);
        options.setConnectionTimeout(10);
        options.setKeepAliveInterval(60);
        
        mqttClient.setCallback(new MqttCallback() {
            @Override
            public void connectionLost(Throwable cause) {
                logger.warn("MQTT connection lost", cause);
                connected = false;
            }

            @Override
            public void messageArrived(String topic, MqttMessage message) {
                String payload = new String(message.getPayload());
                logger.debug("Message received on topic {}: {}", topic, payload);
                
                Consumer<String> handler = messageHandlers.get(topic);
                if (handler != null) {
                    handler.accept(payload);
                }
            }

            @Override
            public void deliveryComplete(IMqttDeliveryToken token) {
                // Not used for our purposes
            }
        });
        
        mqttClient.connect(options);
        connected = true;
        logger.info("Connected to MQTT broker successfully");
    }
    
    public void disconnect() {
        if (mqttClient != null && mqttClient.isConnected()) {
            try {
                logger.info("Disconnecting from MQTT broker");
                mqttClient.disconnect();
                mqttClient.close();
                connected = false;
            } catch (MqttException e) {
                logger.error("Error disconnecting from MQTT broker", e);
            }
        }
    }
    
    public boolean isConnected() {
        return connected && mqttClient != null && mqttClient.isConnected();
    }
    
    public void publish(String topic, Object payload) throws MqttException {
        if (!isConnected()) {
            throw new MqttException(MqttException.REASON_CODE_CLIENT_NOT_CONNECTED);
        }
        
        String jsonPayload = gson.toJson(payload);
        MqttMessage message = new MqttMessage(jsonPayload.getBytes());
        message.setQos(1);
        message.setRetained(false);
        
        logger.debug("Publishing to {}: {}", topic, jsonPayload);
        mqttClient.publish(topic, message);
    }
    
    public void subscribe(String topic, Consumer<String> handler) throws MqttException {
        if (!isConnected()) {
            throw new MqttException(MqttException.REASON_CODE_CLIENT_NOT_CONNECTED);
        }
        
        logger.debug("Subscribing to topic: {}", topic);
        messageHandlers.put(topic, handler);
        mqttClient.subscribe(topic, 1);
    }
    
    public void unsubscribe(String topic) throws MqttException {
        if (mqttClient != null && mqttClient.isConnected()) {
            logger.debug("Unsubscribing from topic: {}", topic);
            mqttClient.unsubscribe(topic);
            messageHandlers.remove(topic);
        }
    }
}
