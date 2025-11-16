package org.example;

import javafx.application.Application;
import javafx.stage.Stage;
import org.example.ui.ChessGameUI;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class Main extends Application {
    private static final Logger logger = LoggerFactory.getLogger(Main.class);

    @Override
    public void start(Stage primaryStage) {
        logger.info("Starting Chess Robot Host Application");
        ChessGameUI gameUI = new ChessGameUI(primaryStage);
        gameUI.show();
        logger.info("Application UI initialized successfully");
    }

    public static void main(String[] args) {
        logger.info("Chess Robot Host Application starting...");
        launch(args);
    }
}