package org.example.ui.view;

import javafx.geometry.Insets;
import javafx.scene.control.Label;
import javafx.scene.control.Separator;
import javafx.scene.layout.VBox;
import org.example.model.ChessBoard;
import org.example.model.Piece;
import org.example.model.PieceColor;

import java.util.List;

public class PlayerInfoView {
    private final ChessBoard chessBoard;
    private final Label currentTurnLabel;
    private final Label whiteCapturedLabel;
    private final Label blackCapturedLabel;
    private final Label moveCountLabel;
    private final Label statusLabel;

    public PlayerInfoView(ChessBoard chessBoard) {
        this.chessBoard = chessBoard;
        this.currentTurnLabel = new Label();
        this.whiteCapturedLabel = new Label();
        this.blackCapturedLabel = new Label();
        this.moveCountLabel = new Label();
        this.statusLabel = new Label();

        update();
    }

    public void update() {
        // Current turn
        PieceColor turn = chessBoard.getCurrentTurn();
        currentTurnLabel.setText("Turn: " + turn.toString());

        // Move count
        int moveCount = chessBoard.getMoveHistory().size();
        moveCountLabel.setText("Moves: " + moveCount);

        // Game status
        if (chessBoard.isCheckmate()) {
            statusLabel.setText("Status: Checkmate");
            statusLabel.setStyle("-fx-text-fill: red;");
        } else if (chessBoard.isStalemate()) {
            statusLabel.setText("Status: Stalemate");
            statusLabel.setStyle("-fx-text-fill: orange;");
        } else if (chessBoard.isInCheck()) {
            statusLabel.setText("Status: Check");
            statusLabel.setStyle("-fx-text-fill: red;");
        } else {
            statusLabel.setText("Status: Active");
            statusLabel.setStyle("");
        }

        // Captured pieces
        List<Piece> whiteCaptured = chessBoard.getCapturedPieces(PieceColor.WHITE);
        List<Piece> blackCaptured = chessBoard.getCapturedPieces(PieceColor.BLACK);

        whiteCapturedLabel.setText("White lost: " + formatCapturedPieces(whiteCaptured));
        blackCapturedLabel.setText("Black lost: " + formatCapturedPieces(blackCaptured));
    }

    private String formatCapturedPieces(List<Piece> pieces) {
        if (pieces.isEmpty()) {
            return "None";
        }

        StringBuilder sb = new StringBuilder();
        for (Piece piece : pieces) {
            sb.append(piece.getSymbol()).append(" ");
        }
        return sb.toString().trim();
    }

    public VBox getView() {
        VBox container = new VBox(8);
        container.setPadding(new Insets(10));

        Label gameStatusTitle = new Label("Game Status");
        gameStatusTitle.setStyle("-fx-font-weight: bold;");

        VBox statusBox = new VBox(5);
        statusBox.getChildren().addAll(
                currentTurnLabel,
                moveCountLabel,
                statusLabel
        );

        Label capturedTitle = new Label("Captured Pieces");
        capturedTitle.setStyle("-fx-font-weight: bold;");

        VBox capturedBox = new VBox(5);
        capturedBox.getChildren().addAll(
                blackCapturedLabel,
                whiteCapturedLabel
        );

        container.getChildren().addAll(
                gameStatusTitle,
                statusBox,
                new Separator(),
                capturedTitle,
                capturedBox
        );

        return container;
    }
}
