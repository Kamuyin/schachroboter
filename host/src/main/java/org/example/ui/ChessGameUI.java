package org.example.ui;

import javafx.geometry.Insets;
import javafx.geometry.Orientation;
import javafx.scene.Scene;
import javafx.scene.control.*;
import javafx.scene.layout.*;
import javafx.stage.Stage;
import org.example.model.*;
import org.example.ui.view.BoardView;
import org.example.ui.view.GameLogView;
import org.example.ui.view.PlayerInfoView;

public class ChessGameUI {
    private final Stage stage;
    private final ChessBoard chessBoard;
    private final BoardView boardView;
    private final GameLogView gameLogView;
    private final PlayerInfoView playerInfoView;
    private final org.example.ui.StatusBar statusBar;

    // Layout components
    private SplitPane mainHorizontalSplit;
    private SplitPane mainVerticalSplit;
    private TabPane bottomTabPane;
    private TabPane rightTabPane;

    public ChessGameUI(Stage stage) {
        this.stage = stage;
        this.chessBoard = new ChessBoard();
        this.boardView = new BoardView(chessBoard, this::onMove);
        this.gameLogView = new GameLogView();
        this.playerInfoView = new PlayerInfoView(chessBoard);
        this.statusBar = new org.example.ui.StatusBar(chessBoard);

        setupStage();
    }

    private void setupStage() {
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

        Button analyzeBtn = new Button("Analyze");
        analyzeBtn.setDisable(true);

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
        TextArea consoleArea = new TextArea();
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

        Label engineLabel = new Label("Engine: Not connected");
        Label evalLabel = new Label("Evaluation: 0.0");
        Label bestMoveLabel = new Label("Best Move: -");

        analysisBox.getChildren().addAll(
            new Label("Position Analysis"),
            new Separator(),
            engineLabel,
            evalLabel,
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
            pasteFEN
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
        MenuItem analyzePosition = new MenuItem("Analyze Position");
        analyzePosition.setDisable(true);
        MenuItem flipBoard = new MenuItem("Flip Board");
        flipBoard.setDisable(true);
        gameMenu.getItems().addAll(
            analyzePosition,
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
        gameLogView.addMove(move);
        playerInfoView.update();
        statusBar.update();
        boardView.refresh();
        checkGameStatus();
    }

    private void checkGameStatus() {
        if (chessBoard.isCheckmate()) {
            PieceColor winner = chessBoard.getCurrentTurn().opposite();
            showGameOverDialog("Checkmate", winner + " wins!");
        } else if (chessBoard.isStalemate()) {
            showGameOverDialog("Stalemate", "Game drawn by stalemate.");
        } else if (chessBoard.isDraw()) {
            showGameOverDialog("Draw", "Game drawn.");
        } else if (chessBoard.isInCheck()) {
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
        chessBoard.reset();
        boardView.refresh();
        gameLogView.clear();
        playerInfoView.update();
        statusBar.update();
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

    public void show() {
        stage.show();
    }
}
