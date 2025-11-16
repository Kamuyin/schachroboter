package org.example.settings.model;

import org.example.settings.annotations.Setting;
import org.example.settings.annotations.SettingCategory;

import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

public class CategoryModel {
    private final Class<?> categoryClass;
    private final SettingCategory annotation;
    private final List<SettingField> fields;
    private final Object instance;

    public CategoryModel(Class<?> categoryClass, Object instance) {
        this.categoryClass = categoryClass;
        this.annotation = categoryClass.getAnnotation(SettingCategory.class);
        this.instance = instance;
        this.fields = discoverFields();
    }

    private List<SettingField> discoverFields() {
        List<SettingField> settingFields = new ArrayList<>();
        
        for (Field field : categoryClass.getDeclaredFields()) {
            if (field.isAnnotationPresent(Setting.class)) {
                settingFields.add(new SettingField(field));
            }
        }
        
        settingFields.sort(Comparator.comparingInt(SettingField::getOrder));
        return settingFields;
    }

    public String getName() {
        return annotation != null ? annotation.name() : categoryClass.getSimpleName();
    }

    public String getDescription() {
        return annotation != null ? annotation.description() : "";
    }

    public int getOrder() {
        return annotation != null ? annotation.order() : 0;
    }

    public List<SettingField> getFields() {
        return fields;
    }

    public Object getInstance() {
        return instance;
    }

    public Class<?> getCategoryClass() {
        return categoryClass;
    }
}
