package org.example.model;

public class Piece {
    private final PieceType type;
    private final PieceColor color;
    private boolean hasMoved;

    public Piece(PieceType type, PieceColor color) {
        this.type = type;
        this.color = color;
        this.hasMoved = false;
    }

    public PieceType getType() {
        return type;
    }

    public PieceColor getColor() {
        return color;
    }

    public boolean hasMoved() {
        return hasMoved;
    }

    public void setMoved(boolean moved) {
        this.hasMoved = moved;
    }

    public String getSymbol() {
        return switch (type) {
            case KING -> color == PieceColor.WHITE ? "♔" : "♚";
            case QUEEN -> color == PieceColor.WHITE ? "♕" : "♛";
            case ROOK -> color == PieceColor.WHITE ? "♖" : "♜";
            case BISHOP -> color == PieceColor.WHITE ? "♗" : "♝";
            case KNIGHT -> color == PieceColor.WHITE ? "♘" : "♞";
            case PAWN -> color == PieceColor.WHITE ? "♙" : "♟";
        };
    }

    @Override
    public String toString() {
        return color + " " + type;
    }
}

