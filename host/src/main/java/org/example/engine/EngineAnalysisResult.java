package org.example.engine;

public record EngineAnalysisResult(String bestMove, String evaluation) {
    public static EngineAnalysisResult empty() {
        return new EngineAnalysisResult(null, null);
    }
}
