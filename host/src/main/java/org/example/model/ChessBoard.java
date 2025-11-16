package org.example.model;

import com.github.bhlangonijr.chesslib.Board;
import com.github.bhlangonijr.chesslib.Square;
import com.github.bhlangonijr.chesslib.Side;
import com.github.bhlangonijr.chesslib.move.MoveGenerator;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.ArrayList;
import java.util.List;

public class ChessBoard {
    private static final Logger logger = LoggerFactory.getLogger(ChessBoard.class);
    
    private final Board internalBoard;
    private final List<Move> moveHistory;
    private final List<Piece> capturedWhitePieces;
    private final List<Piece> capturedBlackPieces;

    public ChessBoard() {
        logger.debug("Initializing chess board");
        internalBoard = new Board();
        moveHistory = new ArrayList<>();
        capturedWhitePieces = new ArrayList<>();
        capturedBlackPieces = new ArrayList<>();
    }

    public Piece getPiece(Position pos) {
        if (!pos.isValid()) return null;

        Square square = positionToSquare(pos);
        com.github.bhlangonijr.chesslib.Piece chesslibPiece = internalBoard.getPiece(square);

        if (chesslibPiece == com.github.bhlangonijr.chesslib.Piece.NONE) {
            return null;
        }

        return convertPiece(chesslibPiece);
    }

    public boolean isValidMove(Position from, Position to) {
        if (!from.isValid() || !to.isValid()) return false;

        Piece piece = getPiece(from);
        if (piece == null || piece.getColor() != getCurrentTurn()) return false;

        // Get all legal moves for the current position
        List<com.github.bhlangonijr.chesslib.move.Move> legalMoves = MoveGenerator.generateLegalMoves(internalBoard);
        Square fromSquare = positionToSquare(from);
        Square toSquare = positionToSquare(to);

        // Check if this move is in the legal moves list
        for (com.github.bhlangonijr.chesslib.move.Move move : legalMoves) {
            if (move.getFrom().equals(fromSquare) && move.getTo().equals(toSquare)) {
                return true;
            }
        }

        return false;
    }

    public boolean makeMove(Position from, Position to) {
        return makeMove(from, to, null);
    }

    public boolean makeMove(Position from, Position to, PieceType promotionPiece) {
        if (!isValidMove(from, to)) return false;

        Square fromSquare = positionToSquare(from);
        Square toSquare = positionToSquare(to);
        Piece movingPiece = getPiece(from);
        Piece capturedPiece = getPiece(to);

        // Find the actual move from legal moves
        List<com.github.bhlangonijr.chesslib.move.Move> legalMoves = MoveGenerator.generateLegalMoves(internalBoard);
        com.github.bhlangonijr.chesslib.move.Move moveToMake = null;

        for (com.github.bhlangonijr.chesslib.move.Move move : legalMoves) {
            if (move.getFrom().equals(fromSquare) && move.getTo().equals(toSquare)) {
                // Handle pawn promotion
                if (promotionPiece != null) {
                    com.github.bhlangonijr.chesslib.Piece promotionChesslibPiece =
                            convertToChesslibPiece(promotionPiece, movingPiece.getColor());
                    if (move.getPromotion().equals(promotionChesslibPiece)) {
                        moveToMake = move;
                        break;
                    }
                } else {
                    moveToMake = move;
                    break;
                }
            }
        }

        if (moveToMake == null) return false;

        // Capture piece if present
        if (capturedPiece != null) {
            logger.debug("Piece captured: {}", capturedPiece);
            if (capturedPiece.getColor() == PieceColor.WHITE) {
                capturedWhitePieces.add(capturedPiece);
            } else {
                capturedBlackPieces.add(capturedPiece);
            }
        }

        // Make the move on internal board
        internalBoard.doMove(moveToMake);

        // Record move
        Move move = new Move(from, to, movingPiece, capturedPiece);
        moveHistory.add(move);
        logger.debug("Move made: {} -> {}, total moves: {}", from, to, moveHistory.size());

        return true;
    }

    public PieceColor getCurrentTurn() {
        return internalBoard.getSideToMove() == Side.WHITE ?
                PieceColor.WHITE : PieceColor.BLACK;
    }

    public List<Move> getMoveHistory() {
        return new ArrayList<>(moveHistory);
    }

    public List<Piece> getCapturedPieces(PieceColor color) {
        return color == PieceColor.WHITE ?
                new ArrayList<>(capturedWhitePieces) :
                new ArrayList<>(capturedBlackPieces);
    }

    public void reset() {
        logger.info("Resetting board to initial position");
        internalBoard.loadFromFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        moveHistory.clear();
        capturedWhitePieces.clear();
        capturedBlackPieces.clear();
    }

    public boolean isInCheck() {
        return internalBoard.isKingAttacked();
    }

    public boolean isCheckmate() {
        return internalBoard.isMated();
    }

    public boolean isStalemate() {
        return internalBoard.isStaleMate();
    }

    public boolean isDraw() {
        return internalBoard.isDraw();
    }

    public String getFen() {
        return internalBoard.getFen();
    }

    public List<Position> getLegalMovesFrom(Position from) {
        List<Position> legalDestinations = new ArrayList<>();

        if (!from.isValid()) return legalDestinations;

        Square fromSquare = positionToSquare(from);
        List<com.github.bhlangonijr.chesslib.move.Move> legalMoves = MoveGenerator.generateLegalMoves(internalBoard);

        for (com.github.bhlangonijr.chesslib.move.Move move : legalMoves) {
            if (move.getFrom().equals(fromSquare)) {
                Position toPos = squareToPosition(move.getTo());
                legalDestinations.add(toPos);
            }
        }

        return legalDestinations;
    }

    public boolean isPromotion(Position from, Position to) {
        Piece piece = getPiece(from);
        if (piece == null || piece.getType() != PieceType.PAWN) {
            return false;
        }

        // Check if pawn reaches the last rank
        if (piece.getColor() == PieceColor.WHITE && to.row() == 0) {
            return true;
        }
        if (piece.getColor() == PieceColor.BLACK && to.row() == 7) {
            return true;
        }

        return false;
    }

    // Helper methods to convert between our Position and chesslib Square
    private Square positionToSquare(Position pos) {
        // Our Position: row 0 = rank 8, row 7 = rank 1
        // chesslib Square: A8, B8, ..., H1
        int file = pos.col(); // 0-7 (a-h)
        int rank = 7 - pos.row(); // convert our row to rank (0-7)

        return Square.values()[rank * 8 + file];
    }

    private Position squareToPosition(Square square) {
        int index = square.ordinal();
        int file = index % 8;
        int rank = index / 8;
        int row = 7 - rank;

        return new Position(row, file);
    }

    // Convert chesslib Piece to our Piece
    private Piece convertPiece(com.github.bhlangonijr.chesslib.Piece chesslibPiece) {
        PieceType type = switch (chesslibPiece.getPieceType()) {
            case PAWN -> PieceType.PAWN;
            case KNIGHT -> PieceType.KNIGHT;
            case BISHOP -> PieceType.BISHOP;
            case ROOK -> PieceType.ROOK;
            case QUEEN -> PieceType.QUEEN;
            case KING -> PieceType.KING;
            default -> null;
        };

        PieceColor color = chesslibPiece.getPieceSide() == Side.WHITE ?
                PieceColor.WHITE : PieceColor.BLACK;

        return new Piece(type, color);
    }

    private com.github.bhlangonijr.chesslib.Piece convertToChesslibPiece(PieceType type, PieceColor color) {
        Side side = color == PieceColor.WHITE ? Side.WHITE : Side.BLACK;

        return switch (type) {
            case PAWN -> side == Side.WHITE ?
                    com.github.bhlangonijr.chesslib.Piece.WHITE_PAWN :
                    com.github.bhlangonijr.chesslib.Piece.BLACK_PAWN;
            case KNIGHT -> side == Side.WHITE ?
                    com.github.bhlangonijr.chesslib.Piece.WHITE_KNIGHT :
                    com.github.bhlangonijr.chesslib.Piece.BLACK_KNIGHT;
            case BISHOP -> side == Side.WHITE ?
                    com.github.bhlangonijr.chesslib.Piece.WHITE_BISHOP :
                    com.github.bhlangonijr.chesslib.Piece.BLACK_BISHOP;
            case ROOK -> side == Side.WHITE ?
                    com.github.bhlangonijr.chesslib.Piece.WHITE_ROOK :
                    com.github.bhlangonijr.chesslib.Piece.BLACK_ROOK;
            case QUEEN -> side == Side.WHITE ?
                    com.github.bhlangonijr.chesslib.Piece.WHITE_QUEEN :
                    com.github.bhlangonijr.chesslib.Piece.BLACK_QUEEN;
            case KING -> side == Side.WHITE ?
                    com.github.bhlangonijr.chesslib.Piece.WHITE_KING :
                    com.github.bhlangonijr.chesslib.Piece.BLACK_KING;
        };
    }
}
