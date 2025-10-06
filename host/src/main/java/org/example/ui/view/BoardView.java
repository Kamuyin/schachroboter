package org.example.ui.view;

import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.control.*;
import javafx.scene.layout.*;
import javafx.scene.text.Font;
import org.example.model.*;

import java.util.List;
import java.util.Optional;
import java.util.function.Consumer;

public class BoardView {
    private final ChessBoard chessBoard;
    private final GridPane boardGrid;
    private final SquareView[][] squares;
    private Position selectedPosition;
    private final Consumer<Move> onMoveCallback;
    private List<Position> highlightedMoves;

    public BoardView(ChessBoard chessBoard, Consumer<Move> onMoveCallback) {
        this.chessBoard = chessBoard;
        this.onMoveCallback = onMoveCallback;
        this.squares = new SquareView[8][8];
        this.boardGrid = new GridPane();
        this.selectedPosition = null;
        this.highlightedMoves = List.of();

        setupBoard();
    }

    private void setupBoard() {
        boardGrid.setAlignment(Pos.CENTER);
        boardGrid.setHgap(0);
        boardGrid.setVgap(0);

        // Add column labels (a-h)
        for (int col = 0; col < 8; col++) {
            Label label = new Label(String.valueOf((char) ('a' + col)));
            label.setAlignment(Pos.CENTER);
            label.setPrefWidth(65);
            boardGrid.add(label, col + 1, 0);
        }

        // Add row labels (8-1) and squares
        for (int row = 0; row < 8; row++) {
            Label label = new Label(String.valueOf(8 - row));
            label.setAlignment(Pos.CENTER);
            label.setPrefHeight(65);
            boardGrid.add(label, 0, row + 1);

            for (int col = 0; col < 8; col++) {
                Position pos = new Position(row, col);
                boolean isLight = (row + col) % 2 == 0;
                SquareView square = new SquareView(pos, isLight);
                square.setOnClick(() -> handleSquareClick(pos));

                squares[row][col] = square;
                boardGrid.add(square.getView(), col + 1, row + 1);
            }
        }

        // Add column labels at bottom
        for (int col = 0; col < 8; col++) {
            Label label = new Label(String.valueOf((char) ('a' + col)));
            label.setAlignment(Pos.CENTER);
            label.setPrefWidth(65);
            boardGrid.add(label, col + 1, 9);
        }

        refresh();
    }

    private void handleSquareClick(Position pos) {
        if (selectedPosition == null) {
            // First click - select piece
            Piece piece = chessBoard.getPiece(pos);
            if (piece != null && piece.getColor() == chessBoard.getCurrentTurn()) {
                selectedPosition = pos;
                highlightSquare(pos, true);

                // Highlight legal moves for this piece
                highlightedMoves = chessBoard.getLegalMovesFrom(pos);
                for (Position legalPos : highlightedMoves) {
                    squares[legalPos.row()][legalPos.col()].setLegalMove(true);
                }
            }
        } else {
            // Second click - try to move
            if (pos.equals(selectedPosition)) {
                // Deselect
                clearHighlights();
                selectedPosition = null;
            } else {
                Position from = selectedPosition;
                Piece movingPiece = chessBoard.getPiece(from);
                Piece capturedPiece = chessBoard.getPiece(pos);

                clearHighlights();

                // Check if promotion is needed
                if (chessBoard.isPromotion(from, pos)) {
                    Optional<PieceType> promotionChoice = showPromotionDialog(movingPiece.getColor());
                    if (promotionChoice.isPresent()) {
                        if (chessBoard.makeMove(from, pos, promotionChoice.get())) {
                            Move move = new Move(from, pos, movingPiece, capturedPiece);
                            onMoveCallback.accept(move);
                        }
                    }
                } else {
                    if (chessBoard.makeMove(from, pos)) {
                        Move move = new Move(from, pos, movingPiece, capturedPiece);
                        onMoveCallback.accept(move);
                    }
                }

                selectedPosition = null;
            }
        }
    }

    private void clearHighlights() {
        if (selectedPosition != null) {
            highlightSquare(selectedPosition, false);
        }
        for (Position pos : highlightedMoves) {
            squares[pos.row()][pos.col()].setLegalMove(false);
        }
        highlightedMoves = List.of();
    }

    private Optional<PieceType> showPromotionDialog(PieceColor color) {
        Dialog<PieceType> dialog = new Dialog<>();
        dialog.setTitle("Pawn Promotion");
        dialog.setHeaderText("Choose promotion piece:");

        ButtonType queenButton = new ButtonType("Queen", ButtonBar.ButtonData.OK_DONE);
        ButtonType rookButton = new ButtonType("Rook", ButtonBar.ButtonData.OK_DONE);
        ButtonType bishopButton = new ButtonType("Bishop", ButtonBar.ButtonData.OK_DONE);
        ButtonType knightButton = new ButtonType("Knight", ButtonBar.ButtonData.OK_DONE);
        ButtonType cancelButton = new ButtonType("Cancel", ButtonBar.ButtonData.CANCEL_CLOSE);

        dialog.getDialogPane().getButtonTypes().addAll(queenButton, rookButton, bishopButton, knightButton, cancelButton);

        HBox box = new HBox(15);
        box.setAlignment(Pos.CENTER);
        box.setPadding(new Insets(15));

        Label queenLabel = new Label(color == PieceColor.WHITE ? "♕" : "♛");
        Label rookLabel = new Label(color == PieceColor.WHITE ? "♖" : "♜");
        Label bishopLabel = new Label(color == PieceColor.WHITE ? "♗" : "♝");
        Label knightLabel = new Label(color == PieceColor.WHITE ? "♘" : "♞");

        queenLabel.setFont(Font.font(40));
        rookLabel.setFont(Font.font(40));
        bishopLabel.setFont(Font.font(40));
        knightLabel.setFont(Font.font(40));

        box.getChildren().addAll(queenLabel, rookLabel, bishopLabel, knightLabel);
        dialog.getDialogPane().setContent(box);

        dialog.setResultConverter(buttonType -> {
            if (buttonType == queenButton) return PieceType.QUEEN;
            if (buttonType == rookButton) return PieceType.ROOK;
            if (buttonType == bishopButton) return PieceType.BISHOP;
            if (buttonType == knightButton) return PieceType.KNIGHT;
            return null;
        });

        return dialog.showAndWait();
    }

    private void highlightSquare(Position pos, boolean highlight) {
        if (pos.isValid()) {
            squares[pos.row()][pos.col()].setHighlighted(highlight);
        }
    }

    public void refresh() {
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                Position pos = new Position(row, col);
                Piece piece = chessBoard.getPiece(pos);
                squares[row][col].setPiece(piece);
                squares[row][col].setHighlighted(false);
                squares[row][col].setLegalMove(false);
            }
        }
        selectedPosition = null;
        highlightedMoves = List.of();
    }

    public Pane getView() {
        return boardGrid;
    }
}
