package org.example.model;

public record Position(int row, int col) {
    public boolean isValid() {
        return row >= 0 && row < 8 && col >= 0 && col < 8;
    }

    public String toChessNotation() {
        char file = (char) ('a' + col);
        int rank = 8 - row;
        return "" + file + rank;
    }

    @Override
    public String toString() {
        return toChessNotation();
    }
}

