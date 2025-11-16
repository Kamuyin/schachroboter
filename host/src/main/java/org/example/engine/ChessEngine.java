package org.example.engine;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.*;
import java.util.concurrent.TimeUnit;

public class ChessEngine {
    private static final Logger logger = LoggerFactory.getLogger(ChessEngine.class);
    
    private Process stockfishProcess;
    private BufferedReader reader;
    private BufferedWriter writer;
    private boolean initialized = false;
    private String stockfishPath;

    public ChessEngine(String stockfishPath) {
        this.stockfishPath = stockfishPath;
    }

    public boolean initialize() {
        logger.info("Initializing Stockfish engine at: {}", stockfishPath);
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
                    logger.info("Stockfish engine initialized successfully");
                    return true;
                }
            }
            logger.error("Stockfish initialization failed: did not receive expected responses");
        } catch (IOException e) {
            logger.error("Failed to initialize Stockfish: {}", e.getMessage(), e);
            return false;
        }
        return false;
    }

    public EngineAnalysisResult analyzePosition(String fen, int thinkingTimeMs) {
        if (!initialized) {
            logger.warn("Analysis requested but engine is not initialized");
            return EngineAnalysisResult.empty();
        }

        logger.debug("Analyzing position, thinking time: {}ms", thinkingTimeMs);
        try {
            sendCommand("position fen " + fen);
            sendCommand("go movetime " + thinkingTimeMs);

            String line;
            String bestMove = null;
            String evaluation = null;

            while ((line = readLine()) != null) {
                if (line.startsWith("info") && line.contains("score")) {
                    String parsedScore = parseEvaluation(line);
                    if (parsedScore != null) {
                        evaluation = parsedScore;
                    }
                } else if (line.startsWith("bestmove")) {
                    String[] parts = line.split(" ");
                    if (parts.length >= 2) {
                        bestMove = parts[1];
                    }
                    break;
                }
            }

            logger.debug("Analysis complete: bestMove={}, evaluation={}", bestMove, evaluation);
            return new EngineAnalysisResult(bestMove, evaluation);
        } catch (IOException e) {
            logger.error("Error analyzing position", e);
            return EngineAnalysisResult.empty();
        }
    }

    public String getBestMove(String fen, int thinkingTimeMs) {
        EngineAnalysisResult result = analyzePosition(fen, thinkingTimeMs);
        return result.bestMove();
    }

    public synchronized void setOption(String name, String value) throws IOException {
        sendCommand("setoption name " + name + " value " + value);
    }

    private synchronized void sendCommand(String command) throws IOException {
        writer.write(command + "\n");
        writer.flush();
    }

    private String readLine() throws IOException {
        return reader.readLine();
    }

    private String parseEvaluation(String line) {
        String[] parts = line.split(" ");
        for (int i = 0; i < parts.length; i++) {
            if ("score".equals(parts[i]) && i + 2 < parts.length) {
                String type = parts[i + 1];
                String rawValue = parts[i + 2];
                if ("cp".equals(type)) {
                    try {
                        double centipawns = Double.parseDouble(rawValue);
                        return String.format("%.2f", centipawns / 100.0);
                    } catch (NumberFormatException ignored) {
                        return null;
                    }
                } else if ("mate".equals(type)) {
                    return "Mate in " + rawValue;
                }
            }
        }
        return null;
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
        logger.info("Shutting down Stockfish engine");
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
                    logger.warn("Stockfish process did not terminate gracefully, forcing shutdown");
                    stockfishProcess.destroyForcibly();
                }
            }
            logger.info("Stockfish engine shutdown complete");
        } catch (Exception e) {
            logger.error("Error shutting down Stockfish", e);
        }
    }

    public boolean isInitialized() {
        return initialized;
    }
}

