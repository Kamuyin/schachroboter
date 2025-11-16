package org.example.settings;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import org.example.settings.model.CategoryModel;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

public class SettingsManager {
    private static final Logger logger = LoggerFactory.getLogger(SettingsManager.class);
    private static final String SETTINGS_FILE = "settings.json";
    private static SettingsManager instance;
    private AppSettings settings;
    private final Gson gson;
    private final Path settingsPath;
    private List<CategoryModel> categories;

    private SettingsManager() {
        logger.info("Initializing SettingsManager");
        gson = new GsonBuilder().setPrettyPrinting().create();
        settingsPath = Paths.get(System.getProperty("user.home"), ".schachroboter", SETTINGS_FILE);
        logger.debug("Settings path: {}", settingsPath);
        load();
        discoverCategories();
    }

    public static SettingsManager getInstance() {
        if (instance == null) {
            instance = new SettingsManager();
        }
        return instance;
    }

    public AppSettings getSettings() {
        return settings;
    }

    public List<CategoryModel> getCategories() {
        return categories;
    }

    private void discoverCategories() {
        categories = new ArrayList<>();
        categories.add(new CategoryModel(DisplaySettings.class, settings.getDisplay()));
        categories.add(new CategoryModel(GameSettings.class, settings.getGame()));
        categories.add(new CategoryModel(EngineSettings.class, settings.getEngine()));
        categories.add(new CategoryModel(AppearanceSettings.class, settings.getAppearance()));
        categories.add(new CategoryModel(MqttSettings.class, settings.getMqtt()));
        categories.sort(Comparator.comparingInt(CategoryModel::getOrder));
    }

    public void load() {
        logger.debug("Loading settings from {}", settingsPath);
        try {
            if (Files.exists(settingsPath)) {
                String json = Files.readString(settingsPath);
                settings = gson.fromJson(json, AppSettings.class);
                if (settings == null) {
                    logger.warn("Settings file exists but is empty, using defaults");
                    settings = new AppSettings();
                }
                logger.info("Settings loaded successfully");
            } else {
                logger.info("Settings file not found, creating with defaults");
                settings = new AppSettings();
                save();
            }
        } catch (IOException e) {
            logger.error("Failed to load settings", e);
            settings = new AppSettings();
        }
        ensureNonNullSections();
        if (categories != null) {
            discoverCategories();
        }
    }

    public void save() {
        logger.debug("Saving settings to {}", settingsPath);
        try {
            Files.createDirectories(settingsPath.getParent());
            String json = gson.toJson(settings);
            Files.writeString(settingsPath, json);
            logger.info("Settings saved successfully");
        } catch (IOException e) {
            logger.error("Failed to save settings", e);
        }
    }

    public void reset() {
        logger.info("Resetting settings to defaults");
        settings = new AppSettings();
        discoverCategories();
        save();
    }

    private void ensureNonNullSections() {
        if (settings.getDisplay() == null) {
            settings.setDisplay(new DisplaySettings());
        }
        if (settings.getGame() == null) {
            settings.setGame(new GameSettings());
        }
        if (settings.getEngine() == null) {
            settings.setEngine(new EngineSettings());
        }
        if (settings.getAppearance() == null) {
            settings.setAppearance(new AppearanceSettings());
        }
        if (settings.getMqtt() == null) {
            settings.setMqtt(new MqttSettings());
        }
    }
}
