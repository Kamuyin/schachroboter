package org.example.ui;

import javafx.application.Platform;
import javafx.geometry.Insets;
import javafx.geometry.Orientation;
import javafx.scene.Scene;
import javafx.scene.control.*;
import javafx.scene.layout.*;
import javafx.stage.Stage;
import org.example.engine.EngineAnalysisResult;
import org.example.engine.EngineController;
import org.example.mqtt.MqttBrokerService;
import org.example.model.*;
import org.example.settings.AppSettings;
import org.example.settings.EngineSettings;
import org.example.settings.MqttSettings;
import org.example.settings.SettingsManager;
import org.example.ui.settings.SettingsDialog;
import org.example.ui.view.BoardView;
import org.example.ui.view.GameLogView;
import org.example.ui.view.PlayerInfoView;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class ChessGameUI {
    private static final Logger logger = LoggerFactory.getLogger(ChessGameUI.class);
    
    private final Stage stage;
    private final ChessBoard chessBoard;
    private final BoardView boardView;
    private final GameLogView gameLogView;
    private final PlayerInfoView playerInfoView;
    private final org.example.ui.StatusBar statusBar;
    private final EngineController engineController;
    private final MqttBrokerService mqttBrokerService;

    // Layout components
    private SplitPane mainHorizontalSplit;
    private SplitPane mainVerticalSplit;
    private TabPane bottomTabPane;
    private TabPane rightTabPane;
    private Button analyzeBtn;
    private MenuItem analyzeMenuItem;
    private TextArea consoleArea;
    private Label engineStatusLabel;
    private Label evaluationLabel;
    private Label bestMoveLabel;
    private EngineSettings engineSettings;
    private boolean engineMoveInProgress = false;
    private boolean analysisInProgress = false;

    public ChessGameUI(Stage stage) {
        logger.info("Initializing Chess Game UI");
        SettingsManager.getInstance();
        
        this.stage = stage;
        this.chessBoard = new ChessBoard();
        this.boardView = new BoardView(chessBoard, this::onMove);
        this.gameLogView = new GameLogView();
        this.playerInfoView = new PlayerInfoView(chessBoard);
        this.statusBar = new org.example.ui.StatusBar(chessBoard);
    this.engineController = new EngineController(this::showEngineError);
    this.mqttBrokerService = new MqttBrokerService();

        setupStage();
        applySettings();
        logger.info("Chess Game UI initialization complete");
    }

    private void setupStage() {
        logger.debug("Setting up main stage");
        stage.setTitle("Seminararbeit - Schachroboter");
        stage.setResizable(true);
        stage.setMinWidth(900);
        stage.setMinHeight(600);

        BorderPane root = new BorderPane();

        // Menu bar
        MenuBar menuBar = createMenuBar();

        // Toolbar
        ToolBar toolBar = createToolBar();

        VBox topContainer = new VBox(menuBar, toolBar);
        root.setTop(topContainer);

        // Workspace
        mainHorizontalSplit = createWorkspace();
        root.setCenter(mainHorizontalSplit);

        // Status bar
        root.setBottom(statusBar.getView());

        Scene scene = new Scene(root, 1100, 750);
        stage.setScene(scene);
        stage.setOnCloseRequest(event -> {
            logger.info("Application close requested, shutting down services");
            engineController.shutdown();
            mqttBrokerService.shutdown();
        });
    }

    private ToolBar createToolBar() {
        ToolBar toolBar = new ToolBar();

        Button newGameBtn = new Button("New Game");
        newGameBtn.setOnAction(e -> resetGame());

        Separator sep1 = new Separator();
        sep1.setOrientation(Orientation.VERTICAL);

        Button undoBtn = new Button("Undo");
        undoBtn.setDisable(true);

        Button redoBtn = new Button("Redo");
        redoBtn.setDisable(true);

        Separator sep2 = new Separator();
        sep2.setOrientation(Orientation.VERTICAL);

        Button flipBoardBtn = new Button("Flip Board");
        flipBoardBtn.setDisable(true);

        analyzeBtn = new Button("Analyze");
        analyzeBtn.setDisable(true);
        analyzeBtn.setOnAction(e -> analyzeCurrentPosition());

        toolBar.getItems().addAll(
            newGameBtn, sep1,
            undoBtn, redoBtn, sep2,
            flipBoardBtn, analyzeBtn
        );

        return toolBar;
    }

    private SplitPane createWorkspace() {
        // Main horizontal split: Center+Bottom | Right
        SplitPane horizontalSplit = new SplitPane();
        horizontalSplit.setOrientation(Orientation.HORIZONTAL);

        // Center: Vertical split with Board (top) and Bottom tabs
        mainVerticalSplit = new SplitPane();
        mainVerticalSplit.setOrientation(Orientation.VERTICAL);

        // Board in center
        VBox boardContainer = new VBox(boardView.getView());
        boardContainer.setPadding(new Insets(10));

        // Bottom tabs
        bottomTabPane = createBottomTabPane();
        bottomTabPane.setMinHeight(150);
        bottomTabPane.setPrefHeight(180);

        mainVerticalSplit.getItems().addAll(boardContainer, bottomTabPane);
        mainVerticalSplit.setDividerPositions(0.70);

        // Right panel
        rightTabPane = createRightTabPane();
        rightTabPane.setPrefWidth(250);
        rightTabPane.setMinWidth(200);

        // Add all to horizontal split
        horizontalSplit.getItems().addAll(mainVerticalSplit, rightTabPane);
        horizontalSplit.setDividerPositions(0.78);

        return horizontalSplit;
    }

    private TabPane createBottomTabPane() {
        TabPane tabPane = new TabPane();

        // Move History tab
        Tab moveHistoryTab = new Tab("Move History");
        moveHistoryTab.setClosable(false);
        moveHistoryTab.setContent(gameLogView.getView());

        // Console tab
        Tab consoleTab = new Tab("Console");
        consoleTab.setClosable(false);
        consoleArea = new TextArea();
        consoleArea.setEditable(false);
        consoleArea.setText("Chess Game Console\n");
        consoleArea.appendText("==================\n\n");
        consoleArea.appendText("Ready to play. White to move.\n");
        consoleTab.setContent(consoleArea);

        // Analysis tab
        Tab analysisTab = new Tab("Analysis");
        analysisTab.setClosable(false);
        VBox analysisBox = new VBox(8);
        analysisBox.setPadding(new Insets(10));

        engineStatusLabel = new Label("Engine: Not connected");
        evaluationLabel = new Label("Evaluation: -");
        bestMoveLabel = new Label("Best Move: -");

        analysisBox.getChildren().addAll(
            new Label("Position Analysis"),
            new Separator(),
            engineStatusLabel,
            evaluationLabel,
            bestMoveLabel
        );
        analysisTab.setContent(analysisBox);

        tabPane.getTabs().addAll(moveHistoryTab, consoleTab, analysisTab);
        return tabPane;
    }

    private TabPane createRightTabPane() {
        TabPane tabPane = new TabPane();

        // Game Info tab
        Tab gameInfoTab = new Tab("Game Info");
        gameInfoTab.setClosable(false);
        ScrollPane scrollPane = new ScrollPane(playerInfoView.getView());
        scrollPane.setFitToWidth(true);
        gameInfoTab.setContent(scrollPane);

        // Settings tab
        Tab settingsTab = new Tab("Settings");
        settingsTab.setClosable(false);
        VBox settingsBox = new VBox(10);
        settingsBox.setPadding(new Insets(10));

        Label displayLabel = new Label("Display");
        CheckBox showLegalMoves = new CheckBox("Show legal moves");
        showLegalMoves.setSelected(true);
        CheckBox showCoordinates = new CheckBox("Show coordinates");
        showCoordinates.setSelected(true);
        CheckBox highlightLastMove = new CheckBox("Highlight last move");

        settingsBox.getChildren().addAll(
            displayLabel,
            showLegalMoves,
            showCoordinates,
            highlightLastMove,
            new Separator(),
            new Label("Game"),
            new CheckBox("Enable sound"),
            new CheckBox("Auto-queen promotion")
        );

        ScrollPane settingsScroll = new ScrollPane(settingsBox);
        settingsScroll.setFitToWidth(true);
        settingsTab.setContent(settingsScroll);

        tabPane.getTabs().addAll(gameInfoTab, settingsTab);
        return tabPane;
    }

    private MenuBar createMenuBar() {
        MenuBar menuBar = new MenuBar();

        // File menu
        Menu fileMenu = new Menu("File");
        MenuItem newGame = new MenuItem("New Game");
        newGame.setOnAction(e -> resetGame());
        MenuItem loadGame = new MenuItem("Open...");
        loadGame.setDisable(true);
        MenuItem saveGame = new MenuItem("Save");
        saveGame.setDisable(true);
        MenuItem saveGameAs = new MenuItem("Save As...");
        saveGameAs.setDisable(true);
        MenuItem exit = new MenuItem("Exit");
        exit.setOnAction(e -> stage.close());
        fileMenu.getItems().addAll(
            newGame,
            new SeparatorMenuItem(),
            loadGame,
            saveGame,
            saveGameAs,
            new SeparatorMenuItem(),
            exit
        );

        // Edit menu
        Menu editMenu = new Menu("Edit");
        MenuItem undoMove = new MenuItem("Undo");
        undoMove.setDisable(true);
        MenuItem redoMove = new MenuItem("Redo");
        redoMove.setDisable(true);
        MenuItem copyFEN = new MenuItem("Copy FEN");
        copyFEN.setDisable(true);
        MenuItem pasteFEN = new MenuItem("Paste FEN");
        pasteFEN.setDisable(true);
        editMenu.getItems().addAll(
            undoMove,
            redoMove,
            new SeparatorMenuItem(),
            copyFEN,
            pasteFEN,
            new SeparatorMenuItem(),
            createSettingsMenuItem()
        );

        // View menu
        Menu viewMenu = new Menu("View");
        CheckMenuItem showBottomPanel = new CheckMenuItem("Show Console");
        showBottomPanel.setSelected(true);
        showBottomPanel.setOnAction(e -> toggleBottomPanel(showBottomPanel.isSelected()));

        CheckMenuItem showRightPanel = new CheckMenuItem("Show Side Panel");
        showRightPanel.setSelected(true);
        showRightPanel.setOnAction(e -> toggleRightPanel(showRightPanel.isSelected()));

        MenuItem resetLayout = new MenuItem("Reset Layout");
        resetLayout.setOnAction(e -> resetLayout());

        viewMenu.getItems().addAll(
            showBottomPanel,
            showRightPanel,
            new SeparatorMenuItem(),
            resetLayout
        );

        // Game menu
        Menu gameMenu = new Menu("Game");
        analyzeMenuItem = new MenuItem("Analyze Position");
        analyzeMenuItem.setDisable(true);
        analyzeMenuItem.setOnAction(e -> analyzeCurrentPosition());
        MenuItem flipBoard = new MenuItem("Flip Board");
        flipBoard.setDisable(true);
        gameMenu.getItems().addAll(
            analyzeMenuItem,
            flipBoard
        );

        // Help menu
        Menu helpMenu = new Menu("Help");
        MenuItem about = new MenuItem("About");
        about.setOnAction(e -> showAboutDialog());
        helpMenu.getItems().add(about);

        menuBar.getMenus().addAll(fileMenu, editMenu, viewMenu, gameMenu, helpMenu);
        return menuBar;
    }

    private void toggleBottomPanel(boolean show) {
        if (show && !mainVerticalSplit.getItems().contains(bottomTabPane)) {
            mainVerticalSplit.getItems().add(bottomTabPane);
            mainVerticalSplit.setDividerPositions(0.70);
        } else if (!show) {
            mainVerticalSplit.getItems().remove(bottomTabPane);
        }
    }

    private void toggleRightPanel(boolean show) {
        if (show && !mainHorizontalSplit.getItems().contains(rightTabPane)) {
            mainHorizontalSplit.getItems().add(rightTabPane);
            mainHorizontalSplit.setDividerPositions(0.78);
        } else if (!show) {
            mainHorizontalSplit.getItems().remove(rightTabPane);
        }
    }

    private void resetLayout() {
        mainHorizontalSplit.setDividerPositions(0.78);
        mainVerticalSplit.setDividerPositions(0.70);
    }

    private void onMove(Move move) {
        logger.debug("Move executed: {}", move);
        gameLogView.addMove(move);
        playerInfoView.update();
        statusBar.update();
        boardView.refresh();
        checkGameStatus();
        maybeTriggerEngineMove();
    }

    private void checkGameStatus() {
        if (chessBoard.isCheckmate()) {
            PieceColor winner = chessBoard.getCurrentTurn().opposite();
            logger.info("Game over: Checkmate - {} wins", winner);
            showGameOverDialog("Checkmate", winner + " wins!");
        } else if (chessBoard.isStalemate()) {
            logger.info("Game over: Stalemate");
            showGameOverDialog("Stalemate", "Game drawn by stalemate.");
        } else if (chessBoard.isDraw()) {
            logger.info("Game over: Draw");
            showGameOverDialog("Draw", "Game drawn.");
        } else if (chessBoard.isInCheck()) {
            logger.debug("Check detected");
            statusBar.setCheckStatus(true);
        } else {
            statusBar.setCheckStatus(false);
        }
    }

    private void showGameOverDialog(String title, String message) {
        Alert alert = new Alert(Alert.AlertType.INFORMATION);
        alert.setTitle(title);
        alert.setHeaderText(null);
        alert.setContentText(message + "\n\nStart a new game?");

        ButtonType newGameButton = new ButtonType("New Game");
        ButtonType closeButton = new ButtonType("Close", ButtonBar.ButtonData.CANCEL_CLOSE);
        alert.getButtonTypes().setAll(newGameButton, closeButton);

        alert.showAndWait().ifPresent(response -> {
            if (response == newGameButton) {
                resetGame();
            }
        });
    }

    private void resetGame() {
        logger.info("Resetting game to initial position");
        chessBoard.reset();
        boardView.refresh();
        gameLogView.clear();
        playerInfoView.update();
        statusBar.update();
        maybeTriggerEngineMove();
    }

    private void showAboutDialog() {
        Alert alert = new Alert(Alert.AlertType.INFORMATION);
        alert.setTitle("About");
        alert.setHeaderText("Host Application");
        alert.setContentText(
            ""
        );
        alert.showAndWait();
    }

    private MenuItem createSettingsMenuItem() {
        MenuItem settingsItem = new MenuItem("Settings...");
        settingsItem.setOnAction(e -> openSettings());
        return settingsItem;
    }

    private void openSettings() {
        logger.debug("Opening settings dialog");
        SettingsDialog dialog = new SettingsDialog(stage);
        dialog.show();
        applySettings();
    }

    private void applySettings() {
        logger.debug("Applying settings");
        AppSettings settings = SettingsManager.getInstance().getSettings();
        boardView.applySettings(settings.getDisplay());
        engineSettings = settings.getEngine();
        engineController.applySettings(engineSettings);
        updateEngineUIState();
        maybeTriggerEngineMove();
        updateMqttBroker(settings.getMqtt());
    }

    private void updateMqttBroker(MqttSettings mqttSettings) {
        if (mqttSettings == null) {
            mqttBrokerService.shutdown();
            return;
        }

        try {
            MqttBrokerService.BrokerStatus status = mqttBrokerService.applySettings(mqttSettings);
            switch (status.change()) {
                case STARTED -> logBrokerStatus("Broker started", status.runtimeInfo());
                case RESTARTED -> logBrokerStatus("Broker restarted", status.runtimeInfo());
                case STOPPED -> appendConsole("[MQTT] Broker stopped.");
                case NO_CHANGE -> { /* nothing to log */ }
            }
        } catch (Exception ex) {
            appendConsole("[MQTT] Broker error: " + ex.getMessage());
            showMqttError(ex.getMessage());
        }
    }

    private void logBrokerStatus(String prefix, MqttBrokerService.RuntimeInfo info) {
        if (info == null) {
            appendConsole("[MQTT] " + prefix + ".");
            return;
        }

        StringBuilder builder = new StringBuilder("[MQTT] ")
            .append(prefix)
            .append(" on ")
            .append(info.host())
            .append(":")
            .append(info.port());

        if (info.websocketEnabled()) {
            builder.append(" (WebSocket port ")
                .append(info.websocketPort())
                .append(")");
        }

        appendConsole(builder.toString());
    }

    public void show() {
        stage.show();
    }

    private void analyzeCurrentPosition() {
        if (!engineController.isReady()) {
            logger.warn("Analysis requested but engine is disabled");
            showEngineError("Stockfish engine is disabled.");
            return;
        }

        if (analysisInProgress) {
            logger.debug("Analysis already in progress, ignoring request");
            return;
        }

        logger.debug("Starting position analysis");
        analysisInProgress = true;
        if (engineStatusLabel != null) {
            engineStatusLabel.setText("Engine: Analyzing...");
        }
        setAnalysisControlsEnabled(false);

        engineController.analyzePosition(chessBoard.getFen())
            .thenAccept(result -> Platform.runLater(() -> {
                logger.debug("Analysis complete");
                analysisInProgress = false;
                updateAnalysisResult(result);
                updateEngineUIState();
            }))
            .exceptionally(ex -> {
                Platform.runLater(() -> {
                    logger.error("Engine analysis failed", ex);
                    analysisInProgress = false;
                    showEngineError("Engine analysis failed: " + ex.getMessage());
                    updateEngineUIState();
                });
                return null;
            });
    }

    private void updateAnalysisResult(EngineAnalysisResult result) {
        if (result == null) {
            return;
        }

        if (engineStatusLabel != null && engineController.isReady()) {
            engineStatusLabel.setText("Engine: Ready");
        }

        if (evaluationLabel != null) {
            String evalText = result.evaluation() != null ? result.evaluation() : "-";
            evaluationLabel.setText("Evaluation: " + evalText);
        }

        if (bestMoveLabel != null) {
            String moveText = result.bestMove() != null ? result.bestMove() : "-";
            bestMoveLabel.setText("Best Move: " + moveText);
        }
    }

    private void updateEngineUIState() {
        boolean ready = engineController.isReady();
        if (engineStatusLabel != null) {
            if (!ready) {
                engineStatusLabel.setText("Engine: Disabled");
            } else if (!analysisInProgress && !engineMoveInProgress) {
                engineStatusLabel.setText("Engine: Ready");
            }
        }

        setAnalysisControlsEnabled(ready && !analysisInProgress && !engineMoveInProgress);

        if (!ready && evaluationLabel != null && bestMoveLabel != null) {
            evaluationLabel.setText("Evaluation: -");
            bestMoveLabel.setText("Best Move: -");
        }
    }

    private void setAnalysisControlsEnabled(boolean enabled) {
        boolean allow = enabled && engineController.isReady();
        if (analyzeBtn != null) {
            analyzeBtn.setDisable(!allow);
        }
        if (analyzeMenuItem != null) {
            analyzeMenuItem.setDisable(!allow);
        }
    }

    private void maybeTriggerEngineMove() {
        if (!engineController.playsAsOpponent()) {
            boardView.getView().setDisable(false);
            return;
        }

        PieceColor engineColor = engineController.getEngineSide();
        boolean engineTurn = engineColor != null && chessBoard.getCurrentTurn() == engineColor;
        boardView.getView().setDisable(engineTurn);

        if (engineTurn && !engineMoveInProgress && !isGameOver()) {
            triggerEngineMove();
        }
    }

    private void triggerEngineMove() {
        if (!engineController.isReady()) {
            return;
        }

        logger.debug("Triggering engine move for {}", chessBoard.getCurrentTurn());
        engineMoveInProgress = true;
        if (engineStatusLabel != null) {
            engineStatusLabel.setText("Engine: Thinking...");
        }
        setAnalysisControlsEnabled(false);
        appendConsole("Stockfish thinking for " + chessBoard.getCurrentTurn() + "...");

        engineController.analyzePosition(chessBoard.getFen())
            .thenAccept(result -> Platform.runLater(() -> handleEngineMoveResult(result)))
            .exceptionally(ex -> {
                Platform.runLater(() -> {
                    logger.error("Engine move failed", ex);
                    engineMoveInProgress = false;
                    showEngineError("Engine move failed: " + ex.getMessage());
                    updateEngineUIState();
                });
                return null;
            });
    }

    private void handleEngineMoveResult(EngineAnalysisResult result) {
        engineMoveInProgress = false;
        updateAnalysisResult(result);

        if (result == null || result.bestMove() == null) {
            logger.warn("Engine did not provide a move");
            showEngineError("Stockfish did not provide a move.");
            updateEngineUIState();
            return;
        }

        if (!applyEngineMove(result.bestMove())) {
            logger.error("Failed to apply engine move: {}", result.bestMove());
            showEngineError("Failed to apply engine move: " + result.bestMove());
        }

        updateEngineUIState();
    }

    private boolean applyEngineMove(String moveNotation) {
        if (moveNotation == null || moveNotation.length() < 4) {
            return false;
        }

        Position from = parsePosition(moveNotation.substring(0, 2));
        Position to = parsePosition(moveNotation.substring(2, 4));
        PieceType promotion = moveNotation.length() > 4 ? parsePromotion(moveNotation.charAt(4)) : null;

        if (from == null || to == null) {
            return false;
        }

        Piece movingPiece = chessBoard.getPiece(from);
        Piece capturedPiece = chessBoard.getPiece(to);
        boolean success = promotion != null ? chessBoard.makeMove(from, to, promotion) : chessBoard.makeMove(from, to);

        if (success && movingPiece != null) {
            Move move = new Move(from, to, movingPiece, capturedPiece);
            onMove(move);
            appendConsole("Stockfish plays " + move.toString());
            return true;
        }

        return false;
    }

    private Position parsePosition(String notation) {
        if (notation == null || notation.length() != 2) {
            return null;
        }

        int col = notation.charAt(0) - 'a';
        int rank = Character.getNumericValue(notation.charAt(1));
        int row = 8 - rank;
        Position position = new Position(row, col);
        return position.isValid() ? position : null;
    }

    private PieceType parsePromotion(char symbol) {
        return switch (Character.toLowerCase(symbol)) {
            case 'q' -> PieceType.QUEEN;
            case 'r' -> PieceType.ROOK;
            case 'b' -> PieceType.BISHOP;
            case 'n' -> PieceType.KNIGHT;
            default -> null;
        };
    }

    private boolean isGameOver() {
        return chessBoard.isCheckmate() || chessBoard.isStalemate() || chessBoard.isDraw();
    }

    private void appendConsole(String message) {
        if (consoleArea != null && message != null) {
            consoleArea.appendText(message + "\n");
        }
    }

    private void showEngineError(String message) {
        if (message == null || message.isBlank()) {
            return;
        }

        Runnable task = () -> {
            appendConsole("[Engine] " + message);
            if (engineStatusLabel != null) {
                engineStatusLabel.setText("Engine: Error");
            }

            Alert alert = new Alert(Alert.AlertType.ERROR);
            alert.setTitle("Stockfish Error");
            alert.setHeaderText("Engine Error");
            alert.setContentText(message);
            alert.showAndWait();
        };

        if (Platform.isFxApplicationThread()) {
            task.run();
        } else {
            Platform.runLater(task);
        }
    }

    private void showMqttError(String message) {
        if (message == null || message.isBlank()) {
            return;
        }
        Alert alert = new Alert(Alert.AlertType.ERROR);
        alert.setTitle("MQTT Broker Error");
        alert.setHeaderText("Embedded Broker Error");
        alert.setContentText(message);
        alert.showAndWait();
    }
}
