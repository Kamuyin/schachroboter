package org.example.settings.ui;

import javafx.scene.Node;
import javafx.scene.control.*;
import javafx.stage.FileChooser;
import org.example.settings.annotations.SettingType;
import org.example.settings.model.SettingField;

import java.io.File;

public class SettingControlFactory {

    public static Node createControl(SettingField settingField, Object instance) {
        SettingType type = settingField.getType();
        
        return switch (type) {
            case CHECKBOX -> createCheckBox(settingField, instance);
            case TEXT_FIELD -> createTextField(settingField, instance);
            case NUMBER_SPINNER -> createNumberSpinner(settingField, instance);
            case SLIDER -> createSlider(settingField, instance);
            case COMBO_BOX -> createComboBox(settingField, instance);
            case FILE_CHOOSER -> createFileChooser(settingField, instance);
        };
    }

    private static CheckBox createCheckBox(SettingField field, Object instance) {
        CheckBox checkBox = new CheckBox();
        Object value = field.getValue(instance);
        if (value instanceof Boolean) {
            checkBox.setSelected((Boolean) value);
        }
        return checkBox;
    }

    private static TextField createTextField(SettingField field, Object instance) {
        TextField textField = new TextField();
        Object value = field.getValue(instance);
        if (value != null) {
            textField.setText(value.toString());
        }
        return textField;
    }

    private static Spinner<Integer> createNumberSpinner(SettingField field, Object instance) {
        int min = field.hasRange() ? field.getMinValue() : 0;
        int max = field.hasRange() ? field.getMaxValue() : 1000;
        int step = field.hasRange() ? field.getStep() : 1;
        
        Object value = field.getValue(instance);
        int initialValue = (value instanceof Integer) ? (Integer) value : min;
        
        Spinner<Integer> spinner = new Spinner<>(min, max, initialValue, step);
        spinner.setEditable(true);
        return spinner;
    }

    private static Slider createSlider(SettingField field, Object instance) {
        int min = field.hasRange() ? field.getMinValue() : 0;
        int max = field.hasRange() ? field.getMaxValue() : 100;
        
        Object value = field.getValue(instance);
        double initialValue = (value instanceof Number) ? ((Number) value).doubleValue() : min;
        
        Slider slider = new Slider(min, max, initialValue);
        slider.setShowTickLabels(true);
        slider.setShowTickMarks(true);
        slider.setMajorTickUnit((max - min) / 4.0);
        slider.setBlockIncrement(1);
        
        return slider;
    }

    private static ComboBox<String> createComboBox(SettingField field, Object instance) {
        ComboBox<String> comboBox = new ComboBox<>();
        
        if (field.hasOptions()) {
            comboBox.getItems().addAll(field.getOptions());
        }
        
        Object value = field.getValue(instance);
        if (value != null) {
            comboBox.setValue(value.toString());
        }
        
        return comboBox;
    }

    private static javafx.scene.layout.HBox createFileChooser(SettingField field, Object instance) {
        javafx.scene.layout.HBox container = new javafx.scene.layout.HBox(5);
        
        TextField pathField = new TextField();
        Object value = field.getValue(instance);
        if (value != null) {
            pathField.setText(value.toString());
        }
        pathField.setPrefWidth(300);
        
        Button browseButton = new Button("Browse...");
        browseButton.setOnAction(e -> {
            FileChooser fileChooser = new FileChooser();
            fileChooser.setTitle("Select File");
            File file = fileChooser.showOpenDialog(browseButton.getScene().getWindow());
            if (file != null) {
                pathField.setText(file.getAbsolutePath());
            }
        });
        
        container.getChildren().addAll(pathField, browseButton);
        return container;
    }

    public static void saveControlValue(Node control, SettingField field, Object instance) {
        Object value = extractValue(control, field);
        if (value != null) {
            field.setValue(instance, value);
        }
    }

    private static Object extractValue(Node control, SettingField field) {
        return switch (field.getType()) {
            case CHECKBOX -> ((CheckBox) control).isSelected();
            case TEXT_FIELD -> ((TextField) control).getText();
            case NUMBER_SPINNER -> {
                @SuppressWarnings("unchecked")
                Spinner<Integer> spinner = (Spinner<Integer>) control;
                yield spinner.getValue();
            }
            case SLIDER -> (int) ((Slider) control).getValue();
            case COMBO_BOX -> ((ComboBox<?>) control).getValue();
            case FILE_CHOOSER -> {
                javafx.scene.layout.HBox container = (javafx.scene.layout.HBox) control;
                TextField pathField = (TextField) container.getChildren().get(0);
                yield pathField.getText();
            }
        };
    }
}
