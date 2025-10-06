package org.example;

//import com.formdev.flatlaf.FlatLightLaf;
import com.formdev.flatlaf.FlatLightLaf;
import javafx.application.Application;
import javafx.stage.Stage;
import org.example.ui.ChessGameUI;

import javax.swing.*;

public class Main extends Application {
    @Override
    public void start(Stage primaryStage) {
        try {
            UIManager.setLookAndFeel( new FlatLightLaf() );
        } catch( Exception ex ) {
            System.err.println( "Failed to initialize LaF" );
        }
        ChessGameUI gameUI = new ChessGameUI(primaryStage);
        gameUI.show();
    }

    public static void main(String[] args) {
        launch(args);
    }
}