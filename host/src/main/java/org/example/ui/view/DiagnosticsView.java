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
    
    // Robot coordinate move controls
    private Spinner<Integer> xPositionSpinner;
    private Spinner<Integer> yPositionSpinner;
    private Spinner<Integer> zPositionSpinner;
    private Spinner<Integer> moveSpeedSpinner;
    private Button moveToBtn;
    private Button homeAllBtn;
    private Button gripperOpenBtn;
    private Button gripperCloseBtn;
    private Label currentPositionLabel;
    
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
        
        // Robot Coordinate Move Controls
        panel.getChildren().add(createRobotMoveControlsSection());
        
        panel.getChildren().add(new Separator());
        
        // Stepper Motor Controls
        panel.getChildren().add(createStepperControlsSection());
        
        panel.getChildren().add(new Separator());
        
        // Servo Motor Controls
        panel.getChildren().add(createServoControlsSection());
        
        return panel;
    }
    
    private VBox createRobotMoveControlsSection() {
        VBox section = new VBox(10);
        
        Label titleLabel = new Label("Robot Coordinate Move Control");
        titleLabel.setStyle("-fx-font-weight: bold; -fx-font-size: 13px;");
        
        // Current position display
        HBox currentPosBox = new HBox(10);
        currentPosBox.setAlignment(Pos.CENTER_LEFT);
        Label currentPosTitle = new Label("Current Position:");
        currentPosTitle.setStyle("-fx-font-weight: bold;");
        currentPositionLabel = new Label("X: ?, Y: ?, Z: ?");
        currentPositionLabel.setStyle("-fx-padding: 0 0 0 10; -fx-text-fill: #0066cc;");
        currentPosBox.getChildren().addAll(currentPosTitle, currentPositionLabel);
        
        // Position inputs
        GridPane positionGrid = new GridPane();
        positionGrid.setHgap(15);
        positionGrid.setVgap(8);
        
        // X position
        Label xLabel = new Label("X (mm):");
        xLabel.setPrefWidth(80);
        xPositionSpinner = new Spinner<>(-500, 500, 0, 10);
        xPositionSpinner.setEditable(true);
        xPositionSpinner.setPrefWidth(120);
        positionGrid.add(xLabel, 0, 0);
        positionGrid.add(xPositionSpinner, 1, 0);
        
        // Y position
        Label yLabel = new Label("Y (mm):");
        yLabel.setPrefWidth(80);
        yPositionSpinner = new Spinner<>(-500, 500, 0, 10);
        yPositionSpinner.setEditable(true);
        yPositionSpinner.setPrefWidth(120);
        positionGrid.add(yLabel, 0, 1);
        positionGrid.add(yPositionSpinner, 1, 1);
        
        // Z position
        Label zLabel = new Label("Z (mm):");
        zLabel.setPrefWidth(80);
        zPositionSpinner = new Spinner<>(-200, 200, 0, 10);
        zPositionSpinner.setEditable(true);
        zPositionSpinner.setPrefWidth(120);
        positionGrid.add(zLabel, 0, 2);
        positionGrid.add(zPositionSpinner, 1, 2);
        
        // Speed
        Label speedLabel = new Label("Speed (µs):");
        speedLabel.setPrefWidth(80);
        moveSpeedSpinner = new Spinner<>(100, 10000, 1000, 100);
        moveSpeedSpinner.setEditable(true);
        moveSpeedSpinner.setPrefWidth(120);
        positionGrid.add(speedLabel, 0, 3);
        positionGrid.add(moveSpeedSpinner, 1, 3);
        
        // Control buttons
        HBox buttonBox = new HBox(10);
        buttonBox.setAlignment(Pos.CENTER_LEFT);
        
        moveToBtn = new Button("Move To Position");
        moveToBtn.setOnAction(e -> handleMoveTo());
        moveToBtn.setStyle("-fx-background-color: #4CAF50; -fx-text-fill: white;");
        
        homeAllBtn = new Button("Home All");
        homeAllBtn.setOnAction(e -> handleHomeAll());
        
        Button getPositionBtn = new Button("Get Position");
        getPositionBtn.setOnAction(e -> handleGetPosition());
        
        buttonBox.getChildren().addAll(moveToBtn, homeAllBtn, getPositionBtn);
        
        // Gripper quick controls
        HBox gripperBox = new HBox(10);
        gripperBox.setAlignment(Pos.CENTER_LEFT);
        
        Label gripperLabel = new Label("Gripper:");
        gripperLabel.setStyle("-fx-font-weight: bold;");
        
        gripperOpenBtn = new Button("Open");
        gripperOpenBtn.setOnAction(e -> handleGripperOpen());
        
        gripperCloseBtn = new Button("Close");
        gripperCloseBtn.setOnAction(e -> handleGripperClose());
        
        gripperBox.getChildren().addAll(gripperLabel, gripperOpenBtn, gripperCloseBtn);
        
        section.getChildren().addAll(titleLabel, currentPosBox, positionGrid, buttonBox, gripperBox);
        return section;
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
            mqttClient.subscribe("chess/robot/status", this::handleRobotStatus);
            mqttClient.subscribe("chess/diag/stepper/response", this::handleStepperResponse);
            mqttClient.subscribe("chess/diag/servo/response", this::handleServoResponse);
            
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
            moveToBtn.setDisable(!enable);
            homeAllBtn.setDisable(!enable);
            gripperOpenBtn.setDisable(!enable);
            gripperCloseBtn.setDisable(!enable);
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
    
    // Robot coordinate move command handlers
    private void handleMoveTo() {
        try {
            Map<String, Object> payload = new HashMap<>();
            payload.put("command", "move");
            payload.put("x", xPositionSpinner.getValue());
            payload.put("y", yPositionSpinner.getValue());
            payload.put("z", zPositionSpinner.getValue());
            payload.put("speed", moveSpeedSpinner.getValue());
            
            mqttClient.publish("chess/robot/command", payload);
            logMessage("Sent move command: X=" + xPositionSpinner.getValue() + 
                      ", Y=" + yPositionSpinner.getValue() + 
                      ", Z=" + zPositionSpinner.getValue() + 
                      " @ " + moveSpeedSpinner.getValue() + "µs");
        } catch (MqttException e) {
            logger.error("Failed to send move command", e);
            logMessage("Error: " + e.getMessage());
        }
    }
    
    private void handleHomeAll() {
        try {
            Map<String, Object> payload = new HashMap<>();
            payload.put("command", "home");
            
            mqttClient.publish("chess/robot/command", payload);
            logMessage("Sent home all axes command");
        } catch (MqttException e) {
            logger.error("Failed to send home command", e);
            logMessage("Error: " + e.getMessage());
        }
    }
    
    private void handleGetPosition() {
        try {
            Map<String, Object> payload = new HashMap<>();
            payload.put("command", "get_position");
            
            mqttClient.publish("chess/robot/command", payload);
            logMessage("Requested current position");
        } catch (MqttException e) {
            logger.error("Failed to request position", e);
            logMessage("Error: " + e.getMessage());
        }
    }
    
    private void handleGripperOpen() {
        try {
            Map<String, Object> payload = new HashMap<>();
            payload.put("command", "gripper_open");
            
            mqttClient.publish("chess/robot/command", payload);
            logMessage("Sent gripper open command");
        } catch (MqttException e) {
            logger.error("Failed to send gripper open command", e);
            logMessage("Error: " + e.getMessage());
        }
    }
    
    private void handleGripperClose() {
        try {
            Map<String, Object> payload = new HashMap<>();
            payload.put("command", "gripper_close");
            
            mqttClient.publish("chess/robot/command", payload);
            logMessage("Sent gripper close command");
        } catch (MqttException e) {
            logger.error("Failed to send gripper close command", e);
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
    
    private void handleRobotStatus(String payload) {
        Platform.runLater(() -> {
            try {
                JsonObject json = gson.fromJson(payload, JsonObject.class);
                
                // Update position if available
                if (json.has("position")) {
                    JsonObject position = json.getAsJsonObject("position");
                    int x = position.has("x") ? position.get("x").getAsInt() : 0;
                    int y = position.has("y") ? position.get("y").getAsInt() : 0;
                    int z = position.has("z") ? position.get("z").getAsInt() : 0;
                    
                    currentPositionLabel.setText(String.format("X: %d, Y: %d, Z: %d", x, y, z));
                }
                
                // Log status messages
                if (json.has("status")) {
                    String status = json.get("status").getAsString();
                    logMessage("Robot status: " + status);
                }
                
                if (json.has("message")) {
                    String message = json.get("message").getAsString();
                    logMessage("← " + message);
                }
            } catch (Exception e) {
                logger.debug("Failed to parse robot status JSON", e);
                logMessage("← " + payload);
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
