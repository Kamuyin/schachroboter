package org.example.engine;

import org.example.model.PieceColor;
import org.example.settings.EngineSettings;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.util.Locale;
import java.util.Objects;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.function.Consumer;

public class EngineController {
    private static final Logger logger = LoggerFactory.getLogger(EngineController.class);
    
    private final ExecutorService executor;
    private final Consumer<String> errorHandler;
    private ChessEngine chessEngine;
    private EngineSettings settings;
    private String currentPath;

    public EngineController(Consumer<String> errorHandler) {
        this.errorHandler = errorHandler;
        this.executor = Executors.newSingleThreadExecutor(runnable -> {
            Thread thread = new Thread(runnable, "stockfish-engine-thread");
            thread.setDaemon(true);
            return thread;
        });
    }

    public synchronized void applySettings(EngineSettings newSettings) {
        logger.debug("Applying engine settings");
        this.settings = newSettings;
        if (newSettings == null || !newSettings.isEngineEnabled()) {
            logger.info("Engine disabled by settings");
            disableEngine();
            return;
        }

        String desiredPath = newSettings.getStockfishPath();
        if (desiredPath == null || desiredPath.isBlank()) {
            logger.warn("Engine enabled but Stockfish path is not configured");
            disableEngine();
            notifyError("Stockfish path is not configured.");
            return;
        }

        if (chessEngine != null && Objects.equals(desiredPath, currentPath) && chessEngine.isInitialized()) {
            logger.debug("Engine already initialized with same path, reconfiguring options");
            configureEngineOptions();
            return;
        }

        logger.info("Initializing engine at path: {}", desiredPath);
        disableEngine();
        ChessEngine newEngine = new ChessEngine(desiredPath);
        if (newEngine.initialize()) {
            logger.info("Engine initialized successfully");
            chessEngine = newEngine;
            currentPath = desiredPath;
            configureEngineOptions();
        } else {
            logger.error("Failed to initialize engine at: {}", desiredPath);
            notifyError("Failed to initialize Stockfish at " + desiredPath);
        }
    }

    private void configureEngineOptions() {
        if (!isReady() || settings == null) {
            return;
        }

        logger.debug("Configuring engine options: skill={}, threads={}, hash={}MB", 
            settings.getSkillLevel(), settings.getThreads(), settings.getHashSizeMB());
        try {
            chessEngine.setOption("Skill Level", String.valueOf(settings.getSkillLevel()));
            chessEngine.setOption("Threads", String.valueOf(settings.getThreads()));
            chessEngine.setOption("Hash", String.valueOf(settings.getHashSizeMB()));
        } catch (IOException e) {
            logger.error("Failed to configure engine options", e);
            notifyError("Failed to configure Stockfish: " + e.getMessage());
        }
    }

    public boolean isReady() {
        return chessEngine != null && chessEngine.isInitialized();
    }

    public PieceColor getEngineSide() {
        if (settings == null) {
            return null;
        }

        String value = settings.getEnginePlayAs();
        if (value == null) {
            return null;
        }

        return switch (value.toLowerCase(Locale.ROOT)) {
            case "white" -> PieceColor.WHITE;
            case "black" -> PieceColor.BLACK;
            default -> null;
        };
    }

    public boolean playsAsOpponent() {
        return getEngineSide() != null && isReady();
    }

    public CompletableFuture<EngineAnalysisResult> analyzePosition(String fen) {
        CompletableFuture<EngineAnalysisResult> future = new CompletableFuture<>();
        if (!isReady()) {
            logger.warn("Analysis requested but engine is not ready");
            future.completeExceptionally(new IllegalStateException("Stockfish engine is disabled"));
            return future;
        }

        logger.debug("Submitting position for analysis: {}", fen);
        executor.submit(() -> {
            try {
                EngineAnalysisResult result = chessEngine.analyzePosition(fen, settings.getThinkingTimeMs());
                if (result == null) {
                    result = EngineAnalysisResult.empty();
                }
                logger.debug("Analysis result: bestMove={}, eval={}", result.bestMove(), result.evaluation());
                future.complete(result);
            } catch (Exception ex) {
                logger.error("Exception during position analysis", ex);
                future.completeExceptionally(ex);
            }
        });

        return future;
    }

    public synchronized void disableEngine() {
        if (chessEngine != null) {
            logger.info("Disabling engine");
            chessEngine.shutdown();
            chessEngine = null;
        }
        currentPath = null;
    }

    public void shutdown() {
        logger.info("Shutting down engine controller");
        disableEngine();
        executor.shutdownNow();
    }

    private void notifyError(String message) {
        if (errorHandler != null) {
            errorHandler.accept(message);
        }
    }
}
