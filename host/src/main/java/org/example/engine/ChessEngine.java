package org.example.engine;

import com.github.bhlangonijr.chesslib.Board;
import com.github.bhlangonijr.chesslib.move.Move;
import com.github.bhlangonijr.chesslib.move.MoveGenerator;
import com.github.bhlangonijr.chesslib.move.MoveList;

import java.io.*;
import java.util.List;
import java.util.concurrent.TimeUnit;

public class ChessEngine {
    private Process stockfishProcess;
    private BufferedReader reader;
    private BufferedWriter writer;
    private boolean initialized = false;
    private String stockfishPath;

    public ChessEngine(String stockfishPath) {
        this.stockfishPath = stockfishPath;
    }

    public boolean initialize() {
        try {
            ProcessBuilder pb = new ProcessBuilder(stockfishPath);
            stockfishProcess = pb.start();
            reader = new BufferedReader(new InputStreamReader(stockfishProcess.getInputStream()));
            writer = new BufferedWriter(new OutputStreamWriter(stockfishProcess.getOutputStream()));

            sendCommand("uci");
            String response = readResponse();
            if (response.contains("uciok")) {
                sendCommand("isready");
                response = readResponse();
                if (response.contains("readyok")) {
                    initialized = true;
                    return true;
                }
            }
        } catch (IOException e) {
            System.err.println("Failed to initialize Stockfish: " + e.getMessage());
            return false;
        }
        return false;
    }

    public String getBestMove(String fen, int thinkingTimeMs) {
        if (!initialized) {
            return null;
        }

        try {
            sendCommand("position fen " + fen);
            sendCommand("go movetime " + thinkingTimeMs);

            String line;
            String bestMove = null;
            while ((line = readLine()) != null) {
                if (line.startsWith("bestmove")) {
                    String[] parts = line.split(" ");
                    if (parts.length >= 2) {
                        bestMove = parts[1];
                    }
                    break;
                }
            }
            return bestMove;
        } catch (IOException e) {
            System.err.println("Error getting best move: " + e.getMessage());
            return null;
        }
    }

    public int evaluatePosition(String fen) {
        if (!initialized) {
            return 0;
        }

        try {
            sendCommand("position fen " + fen);
            sendCommand("eval");

            String line;
            while ((line = readLine()) != null) {
                if (line.contains("Final evaluation")) {
                    // Parse evaluation score
                    break;
                }
            }
        } catch (IOException e) {
            System.err.println("Error evaluating position: " + e.getMessage());
        }
        return 0;
    }

    private void sendCommand(String command) throws IOException {
        writer.write(command + "\n");
        writer.flush();
    }

    private String readLine() throws IOException {
        return reader.readLine();
    }

    private String readResponse() throws IOException {
        StringBuilder response = new StringBuilder();
        String line;
        long startTime = System.currentTimeMillis();

        while ((line = readLine()) != null) {
            response.append(line).append("\n");
            if (line.contains("uciok") || line.contains("readyok")) {
                break;
            }
            // Timeout after 5 seconds
            if (System.currentTimeMillis() - startTime > 5000) {
                break;
            }
        }
        return response.toString();
    }

    public void shutdown() {
        try {
            if (writer != null) {
                sendCommand("quit");
                writer.close();
            }
            if (reader != null) {
                reader.close();
            }
            if (stockfishProcess != null) {
                stockfishProcess.waitFor(2, TimeUnit.SECONDS);
                if (stockfishProcess.isAlive()) {
                    stockfishProcess.destroyForcibly();
                }
            }
        } catch (Exception e) {
            System.err.println("Error shutting down Stockfish: " + e.getMessage());
        }
    }

    public boolean isInitialized() {
        return initialized;
    }
}

