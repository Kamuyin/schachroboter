package org.example.ui.view;

import javafx.collections.FXCollections;
import javafx.collections.ObservableList;
import javafx.geometry.Insets;
import javafx.scene.control.ListView;
import javafx.scene.layout.VBox;
import org.example.model.Move;
import org.example.model.PieceColor;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class GameLogView {
    private static final Logger logger = LoggerFactory.getLogger(GameLogView.class);
    
    private final ListView<String> moveListView;
    private final ObservableList<String> moves;
    private int moveNumber;

    public GameLogView() {
        this.moves = FXCollections.observableArrayList();
        this.moveListView = new ListView<>(moves);
        this.moveNumber = 1;

        setupView();
    }

    private void setupView() {
        moveListView.setPrefHeight(200);
    }

    public void addMove(Move move) {
        String moveText;

        if (move.piece().getColor() == PieceColor.WHITE) {
            moveText = String.format("%d. %s", moveNumber, formatMove(move));
        } else {
            moveText = String.format("%d. ... %s", moveNumber, formatMove(move));
            moveNumber++;
        }

        logger.debug("Adding move to game log: {}", moveText);
        moves.add(moveText);
        moveListView.scrollTo(moves.size() - 1);
    }

    private String formatMove(Move move) {
        String piece = move.piece().getType().name().substring(0, 1);
        if (move.piece().getType().name().equals("PAWN")) {
            piece = "";
        }

        String capture = move.capturedPiece() != null ? "x" : "";
        return piece + capture + move.to().toChessNotation();
    }

    public void clear() {
        logger.debug("Clearing game log");
        moves.clear();
        moveNumber = 1;
    }

    public VBox getView() {
        VBox container = new VBox();
        container.setPadding(new Insets(5));
        container.getChildren().add(moveListView);
        VBox.setVgrow(moveListView, javafx.scene.layout.Priority.ALWAYS);
        return container;
    }
}
