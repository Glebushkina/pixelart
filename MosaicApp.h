#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include <filesystem>
#include <opencv2/opencv.hpp>
#include "MosaicProcessor.h"
#include "PostProcessor.h"
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>

namespace fs = std::filesystem;

class GUI {
private:
    //основные компоненты окна
    sf::RenderWindow window;
    sf::Font font;
    sf::Color backgroundColor;
    sf::Color buttonColor;
    sf::Color textColor;

    //элементы интерфейса
    std::vector<sf::RectangleShape> buttons;
    std::vector<sf::Text> buttonLabels;
    std::vector<sf::RectangleShape> metricButtons;
    std::vector<sf::Text> metricLabels;
    std::vector<sf::RectangleShape> checkboxes;
    std::vector<sf::Text> checkboxLabels;
    sf::Text metricsTitle;
    sf::Text tileSizeLabel;
    sf::Text stepSizeLabel;

    //элементы для угла поворота 
    bool showRotationInput = false;
    sf::Text rotationAngleLabel;

    //элементы для повторов
    bool showRepeatInput = false;
    std::string currentMaxRepeats = "MAX";
    sf::Text maxRepeatsLabel;

    //состояния элементов
    std::vector<bool> checkboxStates;
    std::vector<bool> buttonStates;
    std::vector<bool> metricButtonStates;

    //внутренние квадратики для чекбоксов
    std::vector<sf::RectangleShape> innerCheckboxShapes;

    //текущие настройки
    int currentTileSize;
    int currentStepSize;
    int currentRotationAngle;
    //сообщения
    sf::Text messageText;
    sf::RectangleShape messageBox;
    sf::Clock messageTimer;
    bool showMessageFlag = false;
    const sf::Time messageDuration = sf::seconds(4.0f);

    //элементы загрузки
    sf::Text loadingText;
    sf::RectangleShape loadingBackground;
    bool showLoading = false;

    //пути к данным
    std::string selectedImagePath;
    std::string selectedTilesFolderPath;
    sf::Text selectedImageText;
    sf::Text selectedFolderText;

    //кнопки просмотра изображений и мозаики
    sf::RectangleShape viewImageButton;
    sf::Text viewImageButtonLabel;
    sf::RectangleShape viewMosaicButton;
    sf::Text viewMosaicButtonLabel;

    //текстуры и спрайты
    sf::Texture originalImageTexture;
    sf::Sprite originalImageSprite;
    sf::Texture mosaicTexture;
    sf::Sprite mosaicSprite;

    //флаги отображения
    bool showOriginalImage = false;
    bool showMosaicImage = false;

    //масштабирование мозаики
    float mosaicScale = 1.0f;
    const float minScale = 0.1f;
    const float maxScale = 3.0f;
    const float scaleStep = 0.1f;

    //сохранение результата
    sf::Text resolutionLabel;
    std::string currentFormat = "jpg";
    std::vector<std::string> availableFormats = { "jpg", "png", "bmp", "tiff" };
    cv::Mat currentMosaicResult;

public:
    GUI();
    void run();

private:
    //диалоговые окна
    std::string openFileDialog(const std::string& title);
    std::string openFolderDialog(const std::string& title);
    std::string openSaveDialog(const std::string& title, const std::string& format);

    //инициализация
    void initializeColors();
    void loadFont();
    void setupUI();
    void setupLoadingScreen();
    void centerLoadingScreen();

    //создание элементов интерфейса
    void createButton(const std::string& label, float x, float y);
    void createMetricButton(const std::string& label, float x, float y);
    void createCheckbox(const std::string& label, float x, float y);
    sf::Text createText(const std::string& content, float x, float y, unsigned int size);

    //обработчики событий
    void handleEvents();
    void handleButtonClick(int buttonIndex);
    void handleCheckboxClick(int checkboxIndex);
    void handleMetricButtonClick(int metricIndex);

    //обновление значений
    void updateTileSize();
    void updateStepSize();
    void updateRotationAngle();
    void updateResolutionFormat();

    //вспомогательные методы
    std::string getSelectedMetric() const;
    std::string runMaxRepeatsInput(const std::string& initialValue);
    void showMessage(const std::string& message, bool isError = false);

    //отрисовка
    void render();
};
