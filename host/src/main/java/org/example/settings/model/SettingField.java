package org.example.settings.model;

import org.example.settings.annotations.*;

import java.lang.reflect.Field;

public class SettingField {
    private final Field field;
    private final Setting annotation;
    private final SettingRange rangeAnnotation;
    private final SettingOptions optionsAnnotation;

    public SettingField(Field field) {
        this.field = field;
        this.annotation = field.getAnnotation(Setting.class);
        this.rangeAnnotation = field.getAnnotation(SettingRange.class);
        this.optionsAnnotation = field.getAnnotation(SettingOptions.class);
        field.setAccessible(true);
    }

    public String getName() {
        return annotation.name();
    }

    public String getDescription() {
        return annotation.description();
    }

    public int getOrder() {
        return annotation.order();
    }

    public SettingType getType() {
        return annotation.type();
    }

    public Field getField() {
        return field;
    }

    public Object getValue(Object instance) {
        try {
            return field.get(instance);
        } catch (IllegalAccessException e) {
            throw new RuntimeException("Failed to get value for field: " + field.getName(), e);
        }
    }

    public void setValue(Object instance, Object value) {
        try {
            field.set(instance, value);
        } catch (IllegalAccessException e) {
            throw new RuntimeException("Failed to set value for field: " + field.getName(), e);
        }
    }

    public boolean hasRange() {
        return rangeAnnotation != null;
    }

    public int getMinValue() {
        return rangeAnnotation != null ? rangeAnnotation.min() : 0;
    }

    public int getMaxValue() {
        return rangeAnnotation != null ? rangeAnnotation.max() : 100;
    }

    public int getStep() {
        return rangeAnnotation != null ? rangeAnnotation.step() : 1;
    }

    public boolean hasOptions() {
        return optionsAnnotation != null;
    }

    public String[] getOptions() {
        return optionsAnnotation != null ? optionsAnnotation.values() : new String[0];
    }

    public Class<?> getFieldType() {
        return field.getType();
    }
}
