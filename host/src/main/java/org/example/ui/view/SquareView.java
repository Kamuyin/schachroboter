package org.example.ui.view;

import javafx.geometry.Pos;
import javafx.scene.control.Label;
import javafx.scene.layout.StackPane;
import javafx.scene.paint.Color;
import javafx.scene.shape.Circle;
import javafx.scene.text.Font;
import org.example.model.Piece;
import org.example.model.Position;

public class SquareView {
    private final Position position;
    private final boolean isLight;
    private final StackPane pane;
    private final Label pieceLabel;
    private final Circle legalMoveIndicator;
    private Piece piece;
    private boolean highlighted;
    private boolean isLegalMove;

    private static final String LIGHT_SQUARE = "#F0D9B5";
    private static final String DARK_SQUARE = "#B58863";
    private static final String HIGHLIGHT_COLOR = "#F7F769";

    public SquareView(Position position, boolean isLight) {
        this.position = position;
        this.isLight = isLight;
        this.pane = new StackPane();
        this.pieceLabel = new Label();
        this.legalMoveIndicator = new Circle(6);
        this.highlighted = false;
        this.isLegalMove = false;

        setupView();
    }

    private void setupView() {
        pane.setPrefSize(65, 65);
        pane.setMinSize(65, 65);
        pane.setMaxSize(65, 65);
        pane.setAlignment(Pos.CENTER);

        pieceLabel.setFont(Font.font("Segoe UI Symbol", 40));
        pieceLabel.setAlignment(Pos.CENTER);

        legalMoveIndicator.setFill(Color.web("#555555", 0.4));
        legalMoveIndicator.setVisible(false);

        pane.getChildren().addAll(legalMoveIndicator, pieceLabel);
        updateBackground();
    }

    public void setPiece(Piece piece) {
        this.piece = piece;
        if (piece != null) {
            pieceLabel.setText(piece.getSymbol());
        } else {
            pieceLabel.setText("");
        }
    }

    public void setHighlighted(boolean highlighted) {
        this.highlighted = highlighted;
        updateBackground();
    }

    public void setLegalMove(boolean isLegalMove) {
        this.isLegalMove = isLegalMove;
        legalMoveIndicator.setVisible(isLegalMove);
        updateBackground();
    }

    private void updateBackground() {
        String bgColor;
        if (highlighted) {
            bgColor = HIGHLIGHT_COLOR;
        } else {
            bgColor = isLight ? LIGHT_SQUARE : DARK_SQUARE;
        }

        pane.setStyle("-fx-background-color: " + bgColor + ";");
    }

    public void setOnClick(Runnable action) {
        pane.setOnMouseClicked(e -> action.run());
        pane.setOnMouseEntered(e -> pane.setCursor(javafx.scene.Cursor.HAND));
        pane.setOnMouseExited(e -> pane.setCursor(javafx.scene.Cursor.DEFAULT));
    }

    public StackPane getView() {
        return pane;
    }
}
