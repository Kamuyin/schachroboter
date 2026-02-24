package org.example.ui.view;

import com.google.gson.Gson;
import com.google.gson.JsonObject;
import javafx.application.Platform;
import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.control.*;
import javafx.scene.layout.*;
import org.example.mqtt.RobotMqttClient;
import org.eclipse.paho.client.mqttv3.MqttException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.HashMap;
import java.util.Map;

public class DiagnosticsView {
    private static final Logger logger = LoggerFactory.getLogger(DiagnosticsView.class);
    
    private final BorderPane rootPane;
    private final RobotMqttClient mqttClient;
    private final TextArea logArea;
    private final Gson gson = new Gson();
    
    // Connection controls
    private TextField brokerUrlField;
    private Button connectBtn;
    private Label connectionStatusLabel;
    
    // Stepper motor controls
    private ComboBox<String> motorSelector;
    private Spinner<Integer> stepsSpinner;
    private Spinner<Integer> speedSpinner;
    private Button moveBtn;
    private Button stopBtn;
    private Button enableBtn;
    private Button disableBtn;
    private Button homeBtn;
    private Button statusBtn;
    
    // Servo controls
    private Spinner<Integer> servoAngleSpinner;
    private Button servoSetBtn;
    private Button servoEnableBtn;
    private Button servoDisableBtn;
    
    // Chess move controls
    private ComboBox<String> chessMoveCommandSelector;
    private TextField chessMoveFromField;
    private TextField chessMoveToField;
    private Spinner<Integer> chessMoveSpeedSpinner;
    private Button chessMoveExecuteBtn;
    private Button chessMovePickupBtn;
    private Button chessMoveReleaseBtn;
    private Button chessMoveCalibrateBtn;
    private Label robotStatusLabel;
    
    // Status displays
    private Map<String, Label> motorStatusLabels;
    
    public DiagnosticsView() {
        this.mqttClient = new RobotMqttClient();
        this.logArea = new TextArea();
        this.motorStatusLabels = new HashMap<>();
        this.rootPane = new BorderPane();
        
        setupUI();
        updateConnectionState(false);
    }
    
    private void setupUI() {
        // Top: Connection panel
        VBox connectionPanel = createConnectionPanel();
        
        // Center: Split pane with controls and status
        SplitPane centerSplit = new SplitPane();
        centerSplit.setOrientation(javafx.geometry.Orientation.VERTICAL);
        
        ScrollPane controlsScroll = new ScrollPane(createControlsPanel());
        controlsScroll.setFitToWidth(true);
        
        VBox statusPanel = createStatusPanel();
        
        centerSplit.getItems().addAll(controlsScroll, statusPanel);
        centerSplit.setDividerPositions(0.6);
        
        // Bottom: Log area
        VBox logPanel = createLogPanel();
        logPanel.setPrefHeight(150);
        logPanel.setMinHeight(100);
        
        rootPane.setTop(connectionPanel);
        rootPane.setCenter(centerSplit);
        rootPane.setBottom(logPanel);
        
        rootPane.setPadding(new Insets(10));
    }
    
    private VBox createConnectionPanel() {
        VBox panel = new VBox(8);
        panel.setPadding(new Insets(10));
        panel.setStyle("-fx-background-color: #f0f0f0; -fx-border-color: #cccccc; -fx-border-width: 1;");
        
        Label titleLabel = new Label("Robot Connection");
        titleLabel.setStyle("-fx-font-weight: bold; -fx-font-size: 14px;");
        
        HBox connectionBox = new HBox(10);
        connectionBox.setAlignment(Pos.CENTER_LEFT);
        
        Label brokerLabel = new Label("Broker URL:");
        brokerUrlField = new TextField("tcp://localhost:1883");
        brokerUrlField.setPrefWidth(250);
        
        connectBtn = new Button("Connect");
        connectBtn.setOnAction(e -> handleConnect());
        
        Button disconnectBtn = new Button("Disconnect");
        disconnectBtn.setOnAction(e -> handleDisconnect());
        
        connectionStatusLabel = new Label("Disconnected");
        connectionStatusLabel.setStyle("-fx-padding: 0 0 0 20;");
        
        connectionBox.getChildren().addAll(brokerLabel, brokerUrlField, connectBtn, disconnectBtn, connectionStatusLabel);
        
        panel.getChildren().addAll(titleLabel, connectionBox);
        return panel;
    }
    
    private VBox createControlsPanel() {
        VBox panel = new VBox(15);
        panel.setPadding(new Insets(15));
        
        // Stepper Motor Controls
        panel.getChildren().add(createStepperControlsSection());
        
        panel.getChildren().add(new Separator());
        
        // Servo Motor Controls
        panel.getChildren().add(createServoControlsSection());
        
        panel.getChildren().add(new Separator());
        
        // Chess Move Controls
        panel.getChildren().add(createChessMoveControlsSection());
        
        return panel;
    }
    
    private VBox createStepperControlsSection() {
        VBox section = new VBox(10);
        
        Label titleLabel = new Label("Stepper Motor Control");
        titleLabel.setStyle("-fx-font-weight: bold; -fx-font-size: 13px;");
        
        // Motor selection
        HBox motorBox = new HBox(10);
        motorBox.setAlignment(Pos.CENTER_LEFT);
        Label motorLabel = new Label("Motor:");
        motorLabel.setPrefWidth(80);
        motorSelector = new ComboBox<>();
        motorSelector.getItems().addAll("x", "y", "y1", "y2", "z", "all");
        motorSelector.setValue("x");
        motorBox.getChildren().addAll(motorLabel, motorSelector);
        
        // Steps input
        HBox stepsBox = new HBox(10);
        stepsBox.setAlignment(Pos.CENTER_LEFT);
        Label stepsLabel = new Label("Steps:");
        stepsLabel.setPrefWidth(80);
        stepsSpinner = new Spinner<>(-10000, 10000, 200, 100);
        stepsSpinner.setEditable(true);
        stepsSpinner.setPrefWidth(120);
        stepsBox.getChildren().addAll(stepsLabel, stepsSpinner);
        
        // Speed input
        HBox speedBox = new HBox(10);
        speedBox.setAlignment(Pos.CENTER_LEFT);
        Label speedLabel = new Label("Speed (µs):");
        speedLabel.setPrefWidth(80);
        speedSpinner = new Spinner<>(100, 10000, 1000, 100);
        speedSpinner.setEditable(true);
        speedSpinner.setPrefWidth(120);
        speedBox.getChildren().addAll(speedLabel, speedSpinner);
        
        // Control buttons
        HBox buttonBox = new HBox(10);
        buttonBox.setAlignment(Pos.CENTER_LEFT);
        
        moveBtn = new Button("Move");
        moveBtn.setOnAction(e -> handleStepperMove());
        
        stopBtn = new Button("Stop");
        stopBtn.setOnAction(e -> handleStepperStop());
        stopBtn.setStyle("-fx-background-color: #ff6b6b;");
        
        enableBtn = new Button("Enable");
        enableBtn.setOnAction(e -> handleStepperEnable(true));
        
        disableBtn = new Button("Disable");
        disableBtn.setOnAction(e -> handleStepperEnable(false));
        
        homeBtn = new Button("Home");
        homeBtn.setOnAction(e -> handleStepperHome());
        
        statusBtn = new Button("Get Status");
        statusBtn.setOnAction(e -> handleStepperStatus());
        
        buttonBox.getChildren().addAll(moveBtn, stopBtn, enableBtn, disableBtn, homeBtn, statusBtn);
        
        section.getChildren().addAll(titleLabel, motorBox, stepsBox, speedBox, buttonBox);
        return section;
    }
    
    private VBox createServoControlsSection() {
        VBox section = new VBox(10);
        
        Label titleLabel = new Label("Gripper Servo Control");
        titleLabel.setStyle("-fx-font-weight: bold; -fx-font-size: 13px;");
        
        // Angle input
        HBox angleBox = new HBox(10);
        angleBox.setAlignment(Pos.CENTER_LEFT);
        Label angleLabel = new Label("Angle (°):");
        angleLabel.setPrefWidth(80);
        servoAngleSpinner = new Spinner<>(0, 180, 90, 10);
        servoAngleSpinner.setEditable(true);
        servoAngleSpinner.setPrefWidth(120);
        angleBox.getChildren().addAll(angleLabel, servoAngleSpinner);
        
        // Control buttons
        HBox buttonBox = new HBox(10);
        buttonBox.setAlignment(Pos.CENTER_LEFT);
        
        servoSetBtn = new Button("Set Angle");
        servoSetBtn.setOnAction(e -> handleServoSet());
        
        servoEnableBtn = new Button("Enable");
        servoEnableBtn.setOnAction(e -> handleServoEnable(true));
        
        servoDisableBtn = new Button("Disable");
        servoDisableBtn.setOnAction(e -> handleServoEnable(false));
        
        buttonBox.getChildren().addAll(servoSetBtn, servoEnableBtn, servoDisableBtn);
        
        section.getChildren().addAll(titleLabel, angleBox, buttonBox);
        return section;
    }
    
    private VBox createChessMoveControlsSection() {
        VBox section = new VBox(10);
        
        Label titleLabel = new Label("Chess Move Commands");
        titleLabel.setStyle("-fx-font-weight: bold; -fx-font-size: 13px;");
        
        // Command type selection
        HBox commandBox = new HBox(10);
        commandBox.setAlignment(Pos.CENTER_LEFT);
        Label commandLabel = new Label("Command:");
        commandLabel.setPrefWidth(80);
        chessMoveCommandSelector = new ComboBox<>();
        chessMoveCommandSelector.getItems().addAll("move", "pickup", "release", "calibrate");
        chessMoveCommandSelector.setValue("move");
        chessMoveCommandSelector.setPrefWidth(150);
        commandBox.getChildren().addAll(commandLabel, chessMoveCommandSelector);
        
        // From square input
        HBox fromBox = new HBox(10);
        fromBox.setAlignment(Pos.CENTER_LEFT);
        Label fromLabel = new Label("From:");
        fromLabel.setPrefWidth(80);
        chessMoveFromField = new TextField("E2");
        chessMoveFromField.setPrefWidth(60);
        chessMoveFromField.setPromptText("e.g., E2");
        chessMoveFromField.textProperty().addListener((obs, old, newVal) -> {
            if (newVal != null && newVal.length() > 2) {
                chessMoveFromField.setText(old);
            }
        });
        fromBox.getChildren().addAll(fromLabel, chessMoveFromField);
        
        // To square input
        HBox toBox = new HBox(10);
        toBox.setAlignment(Pos.CENTER_LEFT);
        Label toLabel = new Label("To:");
        toLabel.setPrefWidth(80);
        chessMoveToField = new TextField("E4");
        chessMoveToField.setPrefWidth(60);
        chessMoveToField.setPromptText("e.g., E4");
        chessMoveToField.textProperty().addListener((obs, old, newVal) -> {
            if (newVal != null && newVal.length() > 2) {
                chessMoveToField.setText(old);
            }
        });
        toBox.getChildren().addAll(toLabel, chessMoveToField);
        
        // Speed input
        HBox speedBox = new HBox(10);
        speedBox.setAlignment(Pos.CENTER_LEFT);
        Label speedLabel = new Label("Speed:");
        speedLabel.setPrefWidth(80);
        chessMoveSpeedSpinner = new Spinner<>(10, 1000, 100, 10);
        chessMoveSpeedSpinner.setEditable(true);
        chessMoveSpeedSpinner.setPrefWidth(120);
        Label speedUnitLabel = new Label("mm/min");
        speedBox.getChildren().addAll(speedLabel, chessMoveSpeedSpinner, speedUnitLabel);
        
        // Control buttons
        HBox buttonBox = new HBox(10);
        buttonBox.setAlignment(Pos.CENTER_LEFT);
        
        chessMoveExecuteBtn = new Button("Execute Move");
        chessMoveExecuteBtn.setOnAction(e -> handleChessMove());
        chessMoveExecuteBtn.setStyle("-fx-background-color: #4CAF50; -fx-text-fill: white;");
        
        chessMovePickupBtn = new Button("Pickup");
        chessMovePickupBtn.setOnAction(e -> handleChessCommand("pickup"));
        
        chessMoveReleaseBtn = new Button("Release");
        chessMoveReleaseBtn.setOnAction(e -> handleChessCommand("release"));
        
        chessMoveCalibrateBtn = new Button("Calibrate");
        chessMoveCalibrateBtn.setOnAction(e -> handleChessCommand("calibrate"));
        
        buttonBox.getChildren().addAll(chessMoveExecuteBtn, chessMovePickupBtn, chessMoveReleaseBtn, chessMoveCalibrateBtn);
        
        // Robot status display
        HBox statusBox = new HBox(10);
        statusBox.setAlignment(Pos.CENTER_LEFT);
        Label statusLabel = new Label("Robot Status:");
        statusLabel.setPrefWidth(80);
        statusLabel.setStyle("-fx-font-weight: bold;");
        robotStatusLabel = new Label("Unknown");
        robotStatusLabel.setStyle("-fx-padding: 0 0 0 10;");
        statusBox.getChildren().addAll(statusLabel, robotStatusLabel);
        
        section.getChildren().addAll(titleLabel, commandBox, fromBox, toBox, speedBox, buttonBox, statusBox);
        return section;
    }
    
    private VBox createStatusPanel() {
        VBox panel = new VBox(10);
        panel.setPadding(new Insets(15));
        
        Label titleLabel = new Label("Motor Status");
        titleLabel.setStyle("-fx-font-weight: bold; -fx-font-size: 13px;");
        
        GridPane statusGrid = new GridPane();
        statusGrid.setHgap(15);
        statusGrid.setVgap(8);
        statusGrid.setPadding(new Insets(10, 0, 0, 0));
        
        String[] motors = {"x", "y1", "y2", "z"};
        for (int i = 0; i < motors.length; i++) {
            String motor = motors[i];
            Label nameLabel = new Label(motor.toUpperCase() + ":");
            nameLabel.setStyle("-fx-font-weight: bold;");
            Label statusLabel = new Label("Unknown");
            statusLabel.setStyle("-fx-padding: 0 0 0 10;");
            motorStatusLabels.put(motor, statusLabel);
            
            statusGrid.add(nameLabel, 0, i);
            statusGrid.add(statusLabel, 1, i);
        }
        
        panel.getChildren().addAll(titleLabel, statusGrid);
        return panel;
    }
    
    private VBox createLogPanel() {
        VBox panel = new VBox(5);
        panel.setPadding(new Insets(10, 0, 0, 0));
        
        Label titleLabel = new Label("Diagnostic Log");
        titleLabel.setStyle("-fx-font-weight: bold;");
        
        logArea.setEditable(false);
        logArea.setWrapText(true);
        logArea.setPrefRowCount(6);
        logArea.setStyle("-fx-font-family: 'Consolas', monospace; -fx-font-size: 11px;");
        
        Button clearLogBtn = new Button("Clear Log");
        clearLogBtn.setOnAction(e -> logArea.clear());
        
        HBox logControls = new HBox(10);
        logControls.setAlignment(Pos.CENTER_LEFT);
        logControls.getChildren().addAll(titleLabel, clearLogBtn);
        
        panel.getChildren().addAll(logControls, logArea);
        VBox.setVgrow(logArea, Priority.ALWAYS);
        
        return panel;
    }
    
    // Connection handlers
    private void handleConnect() {
        try {
            String brokerUrl = brokerUrlField.getText();
            String clientId = "diagnostics-client-" + System.currentTimeMillis();
            
            logMessage("Connecting to " + brokerUrl + "...");
            mqttClient.connect(brokerUrl, clientId);
            
            // Subscribe to response topics
            mqttClient.subscribe("chess/diag/stepper/response", this::handleStepperResponse);
            mqttClient.subscribe("chess/diag/servo/response", this::handleServoResponse);
            mqttClient.subscribe("chess/robot/status", this::handleRobotStatusResponse);
            
            logMessage("Connected successfully!");
            updateConnectionState(true);
        } catch (MqttException e) {
            logger.error("Failed to connect to MQTT broker", e);
            logMessage("Connection failed: " + e.getMessage());
            updateConnectionState(false);
        }
    }
    
    private void handleDisconnect() {
        mqttClient.disconnect();
        logMessage("Disconnected from broker");
        updateConnectionState(false);
    }
    
    private void updateConnectionState(boolean connected) {
        Platform.runLater(() -> {
            connectionStatusLabel.setText(connected ? "Connected" : "Disconnected");
            connectionStatusLabel.setStyle(connected ? 
                "-fx-padding: 0 0 0 20; -fx-text-fill: green;" : 
                "-fx-padding: 0 0 0 20; -fx-text-fill: red;");
            
            connectBtn.setDisable(connected);
            brokerUrlField.setDisable(connected);
            
            // Enable/disable controls based on connection
            boolean enable = connected;
            moveBtn.setDisable(!enable);
            stopBtn.setDisable(!enable);
            enableBtn.setDisable(!enable);
            disableBtn.setDisable(!enable);
            homeBtn.setDisable(!enable);
            statusBtn.setDisable(!enable);
            servoSetBtn.setDisable(!enable);
            servoEnableBtn.setDisable(!enable);
            servoDisableBtn.setDisable(!enable);
            chessMoveExecuteBtn.setDisable(!enable);
            chessMovePickupBtn.setDisable(!enable);
            chessMoveReleaseBtn.setDisable(!enable);
            chessMoveCalibrateBtn.setDisable(!enable);
        });
    }
    
    // Stepper motor command handlers
    private void handleStepperMove() {
        try {
            Map<String, Object> payload = new HashMap<>();
            payload.put("motor", motorSelector.getValue());
            payload.put("steps", stepsSpinner.getValue());
            payload.put("speed", speedSpinner.getValue());
            
            mqttClient.publish("chess/diag/stepper/move", payload);
            logMessage("Sent move command: " + motorSelector.getValue() + " " + 
                      stepsSpinner.getValue() + " steps @ " + speedSpinner.getValue() + "µs");
        } catch (MqttException e) {
            logger.error("Failed to send move command", e);
            logMessage("Error: " + e.getMessage());
        }
    }
    
    private void handleStepperStop() {
        try {
            Map<String, Object> payload = new HashMap<>();
            payload.put("motor", motorSelector.getValue());
            
            mqttClient.publish("chess/diag/stepper/stop", payload);
            logMessage("Sent stop command: " + motorSelector.getValue());
        } catch (MqttException e) {
            logger.error("Failed to send stop command", e);
            logMessage("Error: " + e.getMessage());
        }
    }
    
    private void handleStepperEnable(boolean enable) {
        try {
            Map<String, Object> payload = new HashMap<>();
            payload.put("motor", motorSelector.getValue());
            payload.put("enable", enable);
            
            mqttClient.publish("chess/diag/stepper/enable", payload);
            logMessage("Sent " + (enable ? "enable" : "disable") + " command: " + motorSelector.getValue());
        } catch (MqttException e) {
            logger.error("Failed to send enable command", e);
            logMessage("Error: " + e.getMessage());
        }
    }
    
    private void handleStepperHome() {
        try {
            Map<String, Object> payload = new HashMap<>();
            payload.put("motor", motorSelector.getValue());
            
            mqttClient.publish("chess/diag/stepper/home", payload);
            logMessage("Sent home command: " + motorSelector.getValue());
        } catch (MqttException e) {
            logger.error("Failed to send home command", e);
            logMessage("Error: " + e.getMessage());
        }
    }
    
    private void handleStepperStatus() {
        try {
            Map<String, Object> payload = new HashMap<>();
            String motor = motorSelector.getValue();
            if (!motor.equals("all")) {
                payload.put("motor", motor);
            }
            
            mqttClient.publish("chess/diag/stepper/status", payload);
            logMessage("Requested status for: " + motor);
        } catch (MqttException e) {
            logger.error("Failed to request status", e);
            logMessage("Error: " + e.getMessage());
        }
    }
    
    // Servo motor command handlers
    private void handleServoSet() {
        try {
            Map<String, Object> payload = new HashMap<>();
            payload.put("angle", servoAngleSpinner.getValue());
            
            mqttClient.publish("chess/diag/servo/set", payload);
            logMessage("Sent gripper servo angle=" + servoAngleSpinner.getValue() + "°");
        } catch (MqttException e) {
            logger.error("Failed to send servo set command", e);
            logMessage("Error: " + e.getMessage());
        }
    }
    
    private void handleServoEnable(boolean enable) {
        try {
            Map<String, Object> payload = new HashMap<>();
            payload.put("enable", enable);
            
            mqttClient.publish("chess/diag/servo/enable", payload);
            logMessage("Sent gripper servo " + (enable ? "enable" : "disable"));
        } catch (MqttException e) {
            logger.error("Failed to send servo enable command", e);
            logMessage("Error: " + e.getMessage());
        }
    }
    
    // Response handlers
    private void handleStepperResponse(String payload) {
        Platform.runLater(() -> {
            logMessage("← " + payload);
            
            try {
                JsonObject json = gson.fromJson(payload, JsonObject.class);
                
                if (json.has("type") && json.get("type").getAsString().equals("stepper_status")) {
                    updateMotorStatus(json);
                }
            } catch (Exception e) {
                logger.debug("Failed to parse response JSON", e);
            }
        });
    }
    
    private void handleServoResponse(String payload) {
        Platform.runLater(() -> {
            logMessage("← " + payload);
        });
    }
    
    // Chess move command handlers
    private void handleChessMove() {
        try {
            String command = chessMoveCommandSelector.getValue();
            String from = chessMoveFromField.getText().toUpperCase().trim();
            String to = chessMoveToField.getText().toUpperCase().trim();
            
            // Validate input
            if (!from.matches("[A-H][1-8]")) {
                logMessage("Error: Invalid 'From' square. Use format A1-H8");
                return;
            }
            if (!to.matches("[A-H][1-8]")) {
                logMessage("Error: Invalid 'To' square. Use format A1-H8");
                return;
            }
            
            Map<String, Object> payload = new HashMap<>();
            payload.put("command", command);
            payload.put("from", from);
            payload.put("to", to);
            payload.put("speed", chessMoveSpeedSpinner.getValue());
            payload.put("timestamp", java.time.Instant.now().toString());
            
            mqttClient.publish("chess/robot/command", payload);
            logMessage("Sent chess command: " + command + " from " + from + " to " + to + " @ " + chessMoveSpeedSpinner.getValue() + "mm/min");
        } catch (MqttException e) {
            logger.error("Failed to send chess move command", e);
            logMessage("Error: " + e.getMessage());
        }
    }
    
    private void handleChessCommand(String command) {
        try {
            Map<String, Object> payload = new HashMap<>();
            payload.put("command", command);
            payload.put("timestamp", java.time.Instant.now().toString());
            
            // Add from/to if specified and relevant
            if (command.equals("pickup") || command.equals("release")) {
                String from = chessMoveFromField.getText().toUpperCase().trim();
                if (!from.isEmpty() && from.matches("[A-H][1-8]")) {
                    payload.put("from", from);
                }
            }
            
            mqttClient.publish("chess/robot/command", payload);
            logMessage("Sent chess command: " + command);
        } catch (MqttException e) {
            logger.error("Failed to send chess command", e);
            logMessage("Error: " + e.getMessage());
        }
    }
    
    private void handleRobotStatusResponse(String payload) {
        Platform.runLater(() -> {
            logMessage("← Robot Status: " + payload);
            
            try {
                JsonObject json = gson.fromJson(payload, JsonObject.class);
                
                if (json.has("state")) {
                    String state = json.get("state").getAsString();
                    robotStatusLabel.setText(state.toUpperCase());
                    
                    // Color code the status
                    String color = switch (state.toLowerCase()) {
                        case "idle" -> "green";
                        case "moving" -> "blue";
                        case "error" -> "red";
                        default -> "black";
                    };
                    robotStatusLabel.setStyle("-fx-padding: 0 0 0 10; -fx-text-fill: " + color + ";");
                }
                
                if (json.has("error_code") && !json.get("error_code").isJsonNull()) {
                    String errorCode = json.get("error_code").getAsString();
                    logMessage("⚠ Robot Error: " + errorCode);
                }
            } catch (Exception e) {
                logger.debug("Failed to parse robot status JSON", e);
            }
        });
    }
    
    private void updateMotorStatus(JsonObject json) {
        if (json.has("motors")) {
            JsonObject motors = json.getAsJsonObject("motors");
            
            for (String motorName : motors.keySet()) {
                JsonObject motorInfo = motors.getAsJsonObject(motorName);
                Label statusLabel = motorStatusLabels.get(motorName);
                
                if (statusLabel != null && motorInfo != null) {
                    int position = motorInfo.has("position") ? motorInfo.get("position").getAsInt() : 0;
                    boolean moving = motorInfo.has("moving") && motorInfo.get("moving").getAsBoolean();
                    
                    String statusText = String.format("Pos: %d, %s", position, moving ? "Moving" : "Idle");
                    statusLabel.setText(statusText);
                    statusLabel.setStyle(moving ? 
                        "-fx-padding: 0 0 0 10; -fx-text-fill: blue;" : 
                        "-fx-padding: 0 0 0 10;");
                }
            }
        }
    }
    
    private void logMessage(String message) {
        Platform.runLater(() -> {
            String timestamp = String.format("[%tT] ", System.currentTimeMillis());
            logArea.appendText(timestamp + message + "\n");
            logArea.setScrollTop(Double.MAX_VALUE);
        });
    }
    
    public BorderPane getView() {
        return rootPane;
    }
    
    public void shutdown() {
        mqttClient.disconnect();
    }
}
