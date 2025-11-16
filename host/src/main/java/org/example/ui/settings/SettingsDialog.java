package org.example.ui.settings;

import javafx.geometry.Insets;
import javafx.scene.Scene;
import javafx.scene.control.*;
import javafx.scene.layout.BorderPane;
import javafx.scene.layout.HBox;
import javafx.scene.layout.Priority;
import javafx.scene.layout.VBox;
import javafx.stage.Modality;
import javafx.stage.Stage;
import org.example.settings.SettingsManager;
import org.example.settings.model.CategoryModel;
import org.example.settings.ui.DynamicSettingsPanel;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.HashMap;
import java.util.Map;

public class SettingsDialog {
    private static final Logger logger = LoggerFactory.getLogger(SettingsDialog.class);
    
    private final Stage dialog;
    private final TreeView<String> categoryTree;
    private final BorderPane contentPane;
    private final Map<String, DynamicSettingsPanel> panelMap;

    public SettingsDialog(Stage owner) {
        this.dialog = new Stage();
        this.categoryTree = new TreeView<>();
        this.contentPane = new BorderPane();
        this.panelMap = new HashMap<>();
        
        setupDialog(owner);
        setupCategories();
        setupContent();
    }

    private void setupDialog(Stage owner) {
        dialog.initModality(Modality.APPLICATION_MODAL);
        dialog.initOwner(owner);
        dialog.setTitle("Settings");
        dialog.setResizable(true);
        dialog.setMinWidth(700);
        dialog.setMinHeight(500);
    }

    private void setupCategories() {
        TreeItem<String> root = new TreeItem<>("Settings");
        root.setExpanded(true);

        for (CategoryModel category : SettingsManager.getInstance().getCategories()) {
            TreeItem<String> item = new TreeItem<>(category.getName());
            root.getChildren().add(item);
        }

        categoryTree.setRoot(root);
        categoryTree.setShowRoot(false);
        categoryTree.setPrefWidth(180);
        categoryTree.setMinWidth(150);

        categoryTree.getSelectionModel().selectedItemProperty().addListener((obs, oldVal, newVal) -> {
            if (newVal != null) {
                showCategoryPanel(newVal.getValue());
            }
        });

        if (!root.getChildren().isEmpty()) {
            categoryTree.getSelectionModel().select(root.getChildren().get(0));
        }
    }

    private void setupContent() {
        for (CategoryModel category : SettingsManager.getInstance().getCategories()) {
            DynamicSettingsPanel panel = new DynamicSettingsPanel(category);
            panelMap.put(category.getName(), panel);
        }

        contentPane.setPadding(new Insets(10));
        if (!panelMap.isEmpty()) {
            showCategoryPanel(SettingsManager.getInstance().getCategories().get(0).getName());
        }
    }

    private void showCategoryPanel(String category) {
        DynamicSettingsPanel panel = panelMap.get(category);
        if (panel != null) {
            contentPane.setCenter(panel);
        } else {
            contentPane.setCenter(new Label("Select a category"));
        }
    }

    public void show() {
        BorderPane root = new BorderPane();

        HBox mainContent = new HBox(10);
        mainContent.setPadding(new Insets(10));
        VBox.setVgrow(mainContent, Priority.ALWAYS);

        Separator separator = new Separator();
        separator.setOrientation(javafx.geometry.Orientation.VERTICAL);

        mainContent.getChildren().addAll(categoryTree, separator, contentPane);
        HBox.setHgrow(contentPane, Priority.ALWAYS);

        HBox buttonBar = new HBox(10);
        buttonBar.setPadding(new Insets(10));
        buttonBar.setStyle("-fx-border-color: #c0c0c0; -fx-border-width: 1 0 0 0;");

        Button resetButton = new Button("Reset to Defaults");
        resetButton.setOnAction(e -> resetToDefaults());

        HBox spacer = new HBox();
        HBox.setHgrow(spacer, Priority.ALWAYS);

        Button okButton = new Button("OK");
        okButton.setPrefWidth(80);
        okButton.setDefaultButton(true);
        okButton.setOnAction(e -> {
            saveAndClose();
        });

        Button cancelButton = new Button("Cancel");
        cancelButton.setPrefWidth(80);
        cancelButton.setCancelButton(true);
        cancelButton.setOnAction(e -> dialog.close());

        Button applyButton = new Button("Apply");
        applyButton.setPrefWidth(80);
        applyButton.setOnAction(e -> apply());

        buttonBar.getChildren().addAll(resetButton, spacer, okButton, cancelButton, applyButton);

        root.setCenter(mainContent);
        root.setBottom(buttonBar);

        Scene scene = new Scene(root, 800, 600);
        dialog.setScene(scene);
        dialog.showAndWait();
    }

    private void apply() {
        logger.debug("Applying settings changes");
        for (DynamicSettingsPanel panel : panelMap.values()) {
            panel.saveValues();
        }
        SettingsManager.getInstance().save();
        logger.info("Settings applied and saved");
    }

    private void saveAndClose() {
        logger.debug("Saving settings and closing dialog");
        apply();
        dialog.close();
    }

    private void resetToDefaults() {
        logger.info("Resetting settings to defaults");
        Alert alert = new Alert(Alert.AlertType.CONFIRMATION);
        alert.setTitle("Reset Settings");
        alert.setHeaderText("Reset all settings to defaults?");
        alert.setContentText("This action cannot be undone.");

        alert.showAndWait().ifPresent(response -> {
            if (response == ButtonType.OK) {
                SettingsManager.getInstance().reset();
                panelMap.clear();
                setupContent();
                TreeItem<String> selected = categoryTree.getSelectionModel().getSelectedItem();
                if (selected != null) {
                    showCategoryPanel(selected.getValue());
                }
            }
        });
    }
}
