package org.example.settings.ui;

import javafx.geometry.Insets;
import javafx.scene.Node;
import javafx.scene.control.Label;
import javafx.scene.control.Separator;
import javafx.scene.layout.GridPane;
import javafx.scene.layout.VBox;
import org.example.settings.model.CategoryModel;
import org.example.settings.model.SettingField;

import java.util.HashMap;
import java.util.Map;

public class DynamicSettingsPanel extends VBox {
    private final CategoryModel categoryModel;
    private final Map<SettingField, Node> controlMap;

    public DynamicSettingsPanel(CategoryModel categoryModel) {
        this.categoryModel = categoryModel;
        this.controlMap = new HashMap<>();
        setupUI();
    }

    private void setupUI() {
        setPadding(new Insets(15));
        setSpacing(10);

        Label titleLabel = new Label(categoryModel.getName());
        titleLabel.setStyle("-fx-font-size: 14px; -fx-font-weight: bold;");

        GridPane grid = new GridPane();
        grid.setHgap(10);
        grid.setVgap(10);

        int row = 0;
        for (SettingField field : categoryModel.getFields()) {
            Label label = new Label(field.getName() + ":");
            Node control = SettingControlFactory.createControl(field, categoryModel.getInstance());
            
            controlMap.put(field, control);
            
            grid.add(label, 0, row);
            grid.add(control, 1, row);
            row++;
        }

        getChildren().addAll(titleLabel, new Separator(), grid);
    }

    public void saveValues() {
        for (Map.Entry<SettingField, Node> entry : controlMap.entrySet()) {
            SettingControlFactory.saveControlValue(
                entry.getValue(),
                entry.getKey(),
                categoryModel.getInstance()
            );
        }
    }

    public CategoryModel getCategoryModel() {
        return categoryModel;
    }
}
