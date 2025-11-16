package org.example.ui;

import javafx.geometry.Insets;
import javafx.scene.control.Label;
import javafx.scene.layout.HBox;
import javafx.scene.layout.Priority;
import javafx.scene.layout.Region;
import org.example.model.ChessBoard;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class StatusBar {
    private static final Logger logger = LoggerFactory.getLogger(StatusBar.class);
    
    private final ChessBoard chessBoard;
    private final Label statusLabel;
    private final Label positionLabel;
    private final Label infoLabel;
    private final HBox statusBar;
    private boolean isInCheck = false;

    public StatusBar(ChessBoard chessBoard) {
        logger.debug("Initializing StatusBar");
        this.chessBoard = chessBoard;
        this.statusLabel = new Label();
        this.positionLabel = new Label();
        this.infoLabel = new Label();
        this.statusBar = new HBox(15);

        setupView();
        update();
    }

    private void setupView() {
        statusBar.setPadding(new Insets(5, 10, 5, 10));
        statusBar.setStyle("-fx-border-color: #c0c0c0; -fx-border-width: 1 0 0 0; -fx-background-color: #f0f0f0;");

        Region spacer = new Region();
        HBox.setHgrow(spacer, Priority.ALWAYS);

        statusBar.getChildren().addAll(statusLabel, spacer, positionLabel, infoLabel);
    }

    public void update() {
        int moveCount = chessBoard.getMoveHistory().size();
        String turn = chessBoard.getCurrentTurn().toString();

        if (isInCheck) {
            statusLabel.setText("CHECK! - " + turn + " to move");
            statusLabel.setStyle("-fx-text-fill: red; -fx-font-weight: bold;");
        } else {
            statusLabel.setText(turn + " to move");
            statusLabel.setStyle("");
        }

        positionLabel.setText("Move: " + moveCount);
        infoLabel.setText("FEN: " + chessBoard.getFen().substring(0, Math.min(30, chessBoard.getFen().length())) + "...");
    }

    public void setCheckStatus(boolean inCheck) {
        this.isInCheck = inCheck;
        update();
    }

    public HBox getView() {
        return statusBar;
    }
}
