package org.example.model;

public record Move(Position from, Position to, Piece piece, Piece capturedPiece) {
    public String toNotation() {
        String pieceSymbol = piece.getType() == PieceType.PAWN ? "" :
                            piece.getType().name().substring(0, 1);
        String capture = capturedPiece != null ? "x" : "";
        return pieceSymbol + capture + to.toChessNotation();
    }

    @Override
    public String toString() {
        if (capturedPiece != null) {
            return from.toChessNotation() + " x " + to.toChessNotation();
        }
        return from.toChessNotation() + " → " + to.toChessNotation();
    }
}

