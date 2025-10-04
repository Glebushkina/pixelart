#include "MosaicApp.h"

//класс GUI
//инициализация
GUI::GUI() : window(sf::VideoMode::getFullscreenModes()[0], "Mosaic Creator", sf::Style::Fullscreen) {
    initializeColors();
    loadFont();
    //устанавливаем текущие значения
    currentTileSize = 30;
    currentStepSize = 30;
    currentRotationAngle = 0;
    setupUI();
    //инициализация состояний и значений
    checkboxStates = std::vector<bool>(5, false);
    buttonStates = std::vector<bool>(3, false);
    metricButtonStates = std::vector<bool>(4, false);
    //активируем первую кнопку (по умолчанию метрика Color)
    metricButtonStates[0] = true;
    metricButtons[0].setFillColor(sf::Color(180, 100, 100));
    //активируем чекбокс повторов по умолчанию (MAX)
    checkboxStates[0] = true;
    handleCheckboxClick(0);
    //инициализация элементов загрузки
    setupLoadingScreen();
}

void GUI::run() {
    while (window.isOpen()) {
        handleEvents();
        render();
        if (showMessageFlag && messageTimer.getElapsedTime() > messageDuration) {
            showMessageFlag = false;
        }
    }
}

void GUI::initializeColors() {
    backgroundColor = sf::Color(253, 240, 240);
    buttonColor = sf::Color(31, 65, 114);
    textColor = sf::Color(31, 65, 114);
}

void GUI::loadFont() {
    font.loadFromFile("resources/fonts/font_1.ttf");
}

void GUI::setupUI() {
    //основные кнопки
    createButton("Load Image", 20, 20);
    createButton("Load Tiles Folder", 20, 80);
    createButton("Create Mosaic", 20, 140);

    //метрики
    metricsTitle = createText("Metrics:", 20, 220, 24);
    createMetricButton("color", 20, 260);
    createMetricButton("color+contrast", 20, 320);
    createMetricButton("gradient", 20, 380);
    createMetricButton("texture", 20, 440);

    //поля ввода шага и размера тайтла (метки для клика)
    tileSizeLabel = createText("Tile Size: " + std::to_string(currentTileSize), 20, 520, 20);
    stepSizeLabel = createText("Step Size: " + std::to_string(currentStepSize), 20, 560, 20);

    //чекбоксы
    createCheckbox("Limitation of repetitions", 20, 620);
    createCheckbox("Rotation", 20, 690);
    createCheckbox("Color correction", 20, 760);
    createCheckbox("Seam smoothing", 20, 830);
    createCheckbox("Alpha-blend", 20, 900);

    //скрытое поле ввода угла поворота
    rotationAngleLabel = createText("Rotation Angle: " + std::to_string(currentRotationAngle), 20, 720, 18);

    //скрытое поле ввода повторов
    maxRepeatsLabel = createText("Max Repeats: " + currentMaxRepeats, 20, 650, 18);

    //инициализация текстов для отображения выбранных путей
    selectedImageText = createText("No image selected", 350, 25, 14);
    selectedFolderText = createText("No folder selected", 350, 85, 14);
    //yстановка цвета для лучшей видимости
    selectedImageText.setFillColor(sf::Color(80, 80, 80));
    selectedFolderText.setFillColor(sf::Color(80, 80, 80));

    //инициализация кнопки просмотра исходного изображения
    viewImageButton.setSize(sf::Vector2f(300, 40));
    viewImageButton.setPosition(window.getSize().x - 320, 20);
    viewImageButton.setFillColor(buttonColor);
    viewImageButtonLabel = createText("View Image", window.getSize().x - 310, 25, 18);
    viewImageButtonLabel.setFillColor(sf::Color(253, 240, 240));

    //инициализация кнопки просмотра мозаики
    viewMosaicButton.setSize(sf::Vector2f(300, 40));
    viewMosaicButton.setPosition(window.getSize().x - 320, 80);
    viewMosaicButton.setFillColor(buttonColor);
    viewMosaicButtonLabel = createText("View Mosaic", window.getSize().x - 310, 85, 18);
    viewMosaicButtonLabel.setFillColor(sf::Color(253, 240, 240));

    //сначала скрываем кнопки
    viewImageButton.setFillColor(sf::Color(150, 150, 150)); //серый - неактивная
    viewMosaicButton.setFillColor(sf::Color(150, 150, 150));

    //инициализация кнопки скачивания
    createButton("Download", 20, 970);
    buttons[3].setFillColor(sf::Color(0, 153, 0));

    //поле ввода разрешения
    resolutionLabel = createText("Resolution: " + currentFormat, 20, 1020, 18);
}

//функция для открытия диалогового окна выбора исходного файла
std::string GUI::openFileDialog(const std::string& title) {
    OPENFILENAMEA ofn;//структура для хранения параметров диалогового окна
    char szFile[260] = { 0 };//буфер для хранения выбранного пути к файлу

    ZeroMemory(&ofn, sizeof(ofn));//инициализация структуры нулями
    ofn.lStructSize = sizeof(ofn);//установка размера структуры
    ofn.hwndOwner = window.getSystemHandle();//указатель на родительское окно
    ofn.lpstrFile = szFile;//указатель на буфер для имени файла
    ofn.nMaxFile = sizeof(szFile);//максимальный размер буфера

    //используем фильтры
    const char* filterStr =
        "Image Files\0*.jpg;*.jpeg;*.png;*.bmp;*.tiff;*.tif\0"
        "All Files\0*.*\0";

    ofn.lpstrFilter = filterStr;
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    ofn.lpstrTitle = title.c_str();
    //вызов системной функции для отображения диалога открытия файла
    if (GetOpenFileNameA(&ofn) == TRUE) {
        return std::string(ofn.lpstrFile);
    }

    return "";//возвращаем пустую строку, если пользователь отменил выбор
}

//функция для открытия диалогового окна выбора папки
std::string GUI::openFolderDialog(const std::string& title) {
    BROWSEINFOA bi;//структура для параметров диалога выбора папки
    char szDir[MAX_PATH] = { 0 };//буфер для хранения пути к папке

    ZeroMemory(&bi, sizeof(bi));
    bi.hwndOwner = window.getSystemHandle();
    bi.pszDisplayName = szDir;
    bi.lpszTitle = title.c_str();
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    //открытие диалогового окна выбора папки
    //функция возвращает указатель на список идентификаторов элементов
    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl != NULL) {
        if (SHGetPathFromIDListA(pidl, szDir)) {
            //освобождаем память и возвращаем путь
            CoTaskMemFree(pidl);
            return std::string(szDir);
        }
        CoTaskMemFree(pidl);
    }

    return ""; //пользователь отменил выбор
}

//функция для открытия диалогового окна сохранения файла
std::string GUI::openSaveDialog(const std::string& title, const std::string& format) {
    OPENFILENAMEA ofn;//структура для параметров диалога сохранения
    char szFile[260] = { 0 };//буфер для имени файла
    std::string defaultName = "mosaic_result." + format; //создаем имя файла по умолчанию на основе формата
    strcpy_s(szFile, defaultName.c_str());//копируем имя по умолчанию в буфер

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = window.getSystemHandle();
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);

    //создание фильтра на основе указанного формата
    std::string filter;
    if (format == "jpg" || format == "jpeg") {
        filter = "JPEG Images\0*.jpg;*.jpeg\0All Files\0*.*\0";
    }
    else if (format == "png") {
        filter = "PNG Images\0*.png\0All Files\0*.*\0";
    }
    else if (format == "bmp") {
        filter = "BMP Images\0*.bmp\0All Files\0*.*\0";
    }
    else if (format == "tiff") {
        filter = "TIFF Images\0*.tiff;*.tif\0All Files\0*.*\0";
    }
    else {
        filter = "All Files\0*.*\0";
    }

    ofn.lpstrFilter = filter.c_str();
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
    ofn.lpstrTitle = title.c_str();
    ofn.lpstrDefExt = format.c_str();
    //вызов системной функции для отображения диалога сохранения
    if (GetSaveFileNameA(&ofn) == TRUE) {
        return std::string(ofn.lpstrFile);
    }

    return "";//пользователь отменил выбор
}

//бновление формата разрешения
void GUI::updateResolutionFormat() {
    //находим текущий индекс формата и переключаем на следующий
    auto it = std::find(availableFormats.begin(), availableFormats.end(), currentFormat);
    if (it != availableFormats.end()) {
        int index = std::distance(availableFormats.begin(), it);
        index = (index + 1) % availableFormats.size();
        currentFormat = availableFormats[index];
    }
    else {
        currentFormat = availableFormats[0];
    }
    resolutionLabel.setString("Resolution: " + currentFormat);
}

//функция для вывода сообщений на экран
void GUI::showMessage(const std::string& message, bool isError) {
    window.setView(window.getDefaultView());

    sf::Color msgTextColor = isError ? sf::Color(133, 0, 33) : sf::Color(31, 65, 114);

    messageText.setFont(font);
    messageText.setString(message);
    messageText.setCharacterSize(20);
    messageText.setFillColor(msgTextColor);
    //границы текста для точного позиционирования
    sf::FloatRect textBounds = messageText.getLocalBounds();
    float padding = 20.0f;
    //размеры окна
    float windowWidth = (float)window.getSize().x;
    float windowHeight = (float)window.getSize().y;
    //размеры текста
    float textWidth = textBounds.width;
    float textHeight = textBounds.height;
    //центрируем по горизонтали и размещаем внизу с отступом
    float posX = (windowWidth - textWidth) / 2.0f;
    float posY = windowHeight - textHeight - padding - 20.0f;

    messageText.setPosition(posX, posY);

    showMessageFlag = true;
    messageTimer.restart();
}

//функция для Loading...
void GUI::setupLoadingScreen() {
    //фон для загрузки
    loadingBackground.setSize(sf::Vector2f(200, 80));
    loadingBackground.setFillColor(sf::Color(31, 65, 114, 90)); //с прозрачностью
    loadingBackground.setOutlineThickness(2);
    loadingBackground.setOutlineColor(sf::Color(31, 65, 114));

    //текст загрузки
    loadingText.setFont(font);
    loadingText.setString("Loading...");
    loadingText.setCharacterSize(24);
    loadingText.setFillColor(sf::Color::White);

    //центрируем оба элемента
    centerLoadingScreen();
}
//центрирование элементов загрузки
void GUI::centerLoadingScreen() {
    float windowWidth = (float)window.getSize().x;
    float windowHeight = (float)window.getSize().y;

    loadingBackground.setPosition(windowWidth / 2 - loadingBackground.getSize().x / 2,
        windowHeight / 2 - loadingBackground.getSize().y / 2);

    sf::FloatRect textBounds = loadingText.getLocalBounds();
    loadingText.setPosition(windowWidth / 2 - textBounds.width / 2,
        windowHeight / 2 - textBounds.height / 2);
}

//создание основной кнопки интерфейса
void GUI::createButton(const std::string& label, float x, float y) {
    sf::RectangleShape button(sf::Vector2f(300, 40));
    button.setPosition(x, y);
    button.setFillColor(buttonColor);
    buttons.push_back(button);

    sf::Text text;
    text.setFont(font);
    text.setString(label);
    text.setCharacterSize(18);
    text.setFillColor(sf::Color(253, 240, 240));
    text.setPosition(x + 10, y + 5);
    buttonLabels.push_back(text);
}

//создание кнопок метрик
void GUI::createMetricButton(const std::string& label, float x, float y) {
    sf::RectangleShape button(sf::Vector2f(300, 40));
    button.setPosition(x, y);
    button.setFillColor(sf::Color(254, 187, 187));
    metricButtons.push_back(button);

    sf::Text text = createText(label, x + 10, y + 5, 16);
    metricLabels.push_back(text);
}

//создание чекбоксов
void GUI::createCheckbox(const std::string& label, float x, float y) {
    //создаем внешний квадрат чекбокса
    sf::RectangleShape checkbox(sf::Vector2f(20, 20));
    checkbox.setPosition(x, y);
    checkbox.setFillColor(sf::Color::Transparent); //прозрачная заливка
    checkbox.setOutlineThickness(1);
    checkbox.setOutlineColor(textColor);
    checkboxes.push_back(checkbox);
    //создаем внутренний квадрат для отображения состояния
    sf::RectangleShape innerShape(sf::Vector2f(12, 12));
    innerShape.setPosition(x + 4, y + 4);
    innerShape.setFillColor(sf::Color::Transparent);//изначально прозрачный
    innerCheckboxShapes.push_back(innerShape);
    //создаем текстовую метку для чекбокса
    sf::Text text = createText(label, x + 30, y, 18);
    checkboxLabels.push_back(text);
}

//создание текста
sf::Text GUI::createText(const std::string& content, float x, float y, unsigned int size) {
    sf::Text text;
    text.setFont(font);
    text.setString(content);
    text.setCharacterSize(size);
    text.setFillColor(textColor);
    text.setPosition(x, y);
    return text;
}

//геттер имени метрики
std::string GUI::getSelectedMetric() const {
    // проверяем состояние каждой кнопки метрики и возвращаем соответствующее значение
    if (metricButtonStates[0]) return "color";
    if (metricButtonStates[1]) return "color_contrast";
    if (metricButtonStates[2]) return "gradient";
    if (metricButtonStates[3]) return "texture";
    return "color";
}

//обработка кликов по кнопкам интерфейса
void GUI::handleButtonClick(int buttonIndex) {
    switch (buttonIndex) {
    case 0: { // Load Image - загрузка исходного изображения
        //открываем диалоговое окно выбора файла с правильными фильтрами
        selectedImagePath = openFileDialog("Select Source Image");

        if (!selectedImagePath.empty()) {
            //сокращаем путь для отображения, если он слишком длинный
            std::string displayPath = selectedImagePath;
            if (displayPath.length() > 40) {
                displayPath = "..." + displayPath.substr(displayPath.length() - 37);
            }
            selectedImageText.setString("Image: " + displayPath);
            //загружаем изображение с помощью OpenCV для проверки
            cv::Mat testImage = cv::imread(selectedImagePath);
            if (testImage.empty()) {
                showMessage("ERROR: Cannot load the selected image!", true);
                selectedImagePath.clear();
                selectedImageText.setString("No image selected");
                viewImageButton.setFillColor(sf::Color(150, 150, 150));
            }
            else {
                //успешная загрузка, создаем текстуру SFML
                sf::Texture newTexture;
                if (newTexture.loadFromFile(selectedImagePath)) {
                    originalImageTexture = newTexture;
                    originalImageSprite.setTexture(originalImageTexture, true);
                    //вычисляем масштаб для отображения в области 400x300
                    float scaleX = 400.0f / originalImageTexture.getSize().x;
                    float scaleY = 300.0f / originalImageTexture.getSize().y;
                    float scale = std::min(scaleX, scaleY); //сохраняем пропорции

                    originalImageSprite.setScale(scale, scale);//применяем масштаб
                    originalImageSprite.setPosition(350, 140);//положение изображения

                    viewImageButton.setFillColor(buttonColor);

                    showMessage("Image successfully loaded: " + selectedImagePath);
                }
                else {
                    showMessage("ERROR: Cannot load texture from image!", true);
                    viewImageButton.setFillColor(sf::Color(150, 150, 150));
                }
            }
        }
        else {
            //пользователь отменил выбор
            showMessage("Image selection cancelled");
            viewImageButton.setFillColor(sf::Color(150, 150, 150));
            showOriginalImage = false;
        }
        break;
    }
    case 1: { // Load Tiles Folder - загрузка папки с тайлами
        //открываем диалоговое окно выбора папки
        selectedTilesFolderPath = openFolderDialog("Select Tiles Folder");

        if (!selectedTilesFolderPath.empty()) {
            std::string displayPath = selectedTilesFolderPath;
            //сокращаем путь для отображения
            if (displayPath.length() > 40) {
                displayPath = "..." + displayPath.substr(displayPath.length() - 37);
            }
            selectedFolderText.setString("Folder: " + displayPath);

            int imageCount = 0;//счетчик кол-ва изображений в папке
            try {
                for (const auto& entry : fs::directory_iterator(selectedTilesFolderPath)) {
                    if (entry.is_regular_file()) {
                        std::string ext = entry.path().extension().string();
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        //проверяем поддерживаемые форматы изображений
                        if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" ||
                            ext == ".bmp" || ext == ".tiff") {
                            imageCount++;
                        }
                    }
                }
            }
            catch (const std::exception& e) {
                //ошибка доступа к папке
                showMessage("ERROR: Cannot access the selected folder!", true);
                selectedTilesFolderPath.clear();
                selectedFolderText.setString("No folder selected");
                break;
            }
            //если папка пуста
            if (imageCount == 0) {
                showMessage("WARNING: No image files found in the selected folder!", true);
            }
            else {
                showMessage("Folder loaded: " + std::to_string(imageCount) + " image files found");
            }
        }
        else {
            showMessage("Folder selection cancelled");
        }
        break;
    }
    case 2: { // Create Mosaic - создание мозаики
        //проверяем, что исходное изображение выбрано
        if (selectedImagePath.empty()) {
            showMessage("ERROR: Please select a source image first!", true);
            break;
        }
        //проверяем, что папка с тайлами выбрана
        if (selectedTilesFolderPath.empty()) {
            showMessage("ERROR: Please select a tiles folder first!", true);
            break;
        }

        showLoading = true;//показываем экран загрузки
        render();//принудительная отрисовка, чтоб отобразилась загрузка
        //создаем генератор мозаики и конфигурацию
        MosaicGenerator gen;
        Config cfg;
        //устанавливаем параметры из текущих настроек GUI
        cfg.tileSize = currentTileSize;
        cfg.gridStep = currentStepSize;
        cfg.repeats = checkboxStates[0];
        cfg.rotation = checkboxStates[1];
        cfg.metric = getSelectedMetric();

        if (cfg.repeats) {
            if (currentMaxRepeats == "MAX") {
                cfg.maxRepeats = std::numeric_limits<int>::max();
            }
            else {
                cfg.maxRepeats = std::stoi(currentMaxRepeats);
                if (cfg.maxRepeats <= 0) cfg.maxRepeats = 1;
            }
        }
        else {
            cfg.maxRepeats = std::numeric_limits<int>::max();
        }

        if (cfg.rotation) {
            cfg.rotationAngle = currentRotationAngle;
        }
        else {
            cfg.rotationAngle = 0.0;
        }
        //настройка пост-обработки
        PostProcessConfig postCfg;
        postCfg.gridSize = cfg.gridStep;
        //добавляем эффекты пост-обработки из чекбоксов
        if (checkboxStates[2]) postCfg.addEffect("color_correction", 0.5);
        if (checkboxStates[3]) postCfg.addEffect("seam_smoothing", 0.7);
        if (checkboxStates[4]) postCfg.addEffect("alpha_blend", 0.5);
        //сеттер конфигурации пост-обработки
        gen.setPostProcessConfig(postCfg);

        std::string tilesDir = selectedTilesFolderPath;
        std::string inputImage = selectedImagePath;
        //загружаем тайлы для мозаики
        if (!gen.loadTiles(tilesDir, cfg.tileSize, cfg.rotation, cfg.rotationAngle)) {
            showMessage("ERROR: Failed to load tiles from: " + tilesDir, true);
            showLoading = false;
            break;
        }
        //проверяем, что тайлы загружены
        if (gen.getTilesCount() == 0) {
            showMessage("ERROR: Loaded 0 tiles. Check path and size.", true);
            showLoading = false;
            break;
        }
        //загружаем исходное изображение
        cv::Mat source = cv::imread(inputImage);
        if (source.empty()) {
            showMessage("ERROR: Cannot load source image: " + inputImage, true);
            showLoading = false;
            break;
        }
        //создаем мозаику
        cv::Mat result;
        try {
            result = gen.createMosaic(source, cfg);
        }
        catch (const std::exception& e) {
            showMessage("ERROR during generation: " + std::string(e.what()), true);
            showLoading = false;
            break;
        }

        showLoading = false;//скрываем экран загрузки
        render();

        if (!result.empty()) {
            //конвертируем из BGR (OpenCV) в RGBA (SFML)
            cv::Mat resultRGBA;
            cv::cvtColor(result, resultRGBA, cv::COLOR_BGR2RGBA);
            //создаем текстуру для отображения результата
            sf::Texture newMosaicTexture;
            if (newMosaicTexture.create(resultRGBA.cols, resultRGBA.rows)) {
                newMosaicTexture.update(resultRGBA.data);
                mosaicTexture = newMosaicTexture;
                mosaicSprite.setTexture(mosaicTexture, true);
                //вычисляем масштаб для отображения
                float scaleX = 400.0f / mosaicTexture.getSize().x;
                float scaleY = 300.0f / mosaicTexture.getSize().y;
                float scale = std::min(scaleX, scaleY); //сохраняем пропорции

                mosaicSprite.setScale(scale, scale);//применяем масштаб
                mosaicSprite.setPosition(350, 140);//положение изображения

                currentMosaicResult = result.clone();//сохраняем результат

                viewMosaicButton.setFillColor(buttonColor);

                showMosaicImage = true;//показываем мозаику
                showOriginalImage = false;//скрываем исходное изображение

                showMessage("Mosaic created successfully!");
            }
            else {
                showMessage("ERROR: Cannot create mosaic texture!", true);
            }
        }
        else {
            showMessage("ERROR: Mosaic generation failed (empty result).", true);
        }
        break;
    }
    case 3: {//Download - сохранение результата
        if (selectedImagePath.empty()) {
            showMessage("ERROR: Please create a mosaic first!", true);
            return;
        }
        //открываем диалоговое окно сохранения
        std::string savedPath = openSaveDialog("Save Mosaic As", currentFormat);
        if (!savedPath.empty()) {
            try {
                //сохраняем изображение с помощью OpenCV
                bool success = cv::imwrite(savedPath, currentMosaicResult);
                if (success) {
                    showMessage("Mosaic saved successfully as: " + savedPath);
                }
                else {
                    showMessage("ERROR: Failed to save mosaic to: " + savedPath, true);
                }
            }
            catch (const std::exception& e) {
                showMessage("ERROR: Failed to save mosaic: " + std::string(e.what()), true);
            }
        }
        else {
            showMessage("Save cancelled");
        }
        break;
    }
    }
}

//обработка клика по чекбоксу
void GUI::handleCheckboxClick(int checkboxIndex) {
    //инвертируем состояние чекбокса
    checkboxStates[checkboxIndex] = !checkboxStates[checkboxIndex];
    //обновляем визуальное отображение чекбокса
    if (checkboxStates[checkboxIndex]) {
        innerCheckboxShapes[checkboxIndex].setFillColor(textColor);
    }
    else {
        innerCheckboxShapes[checkboxIndex].setFillColor(sf::Color::Transparent);
    }
    //дополнительные действия для чекбокса угла
    if (checkboxIndex == 1) {
        showRotationInput = checkboxStates[checkboxIndex];
        if (!checkboxStates[checkboxIndex]) {
            currentRotationAngle = 0;
            rotationAngleLabel.setString("Rotation Angle: " + std::to_string(currentRotationAngle));
        }
    }
    //дополнительные действия для чекбокса поворота
    if (checkboxIndex == 0) {
        showRepeatInput = checkboxStates[checkboxIndex];
        if (!showRepeatInput) {
            currentMaxRepeats = "MAX";
            maxRepeatsLabel.setString("Max Repeats: " + currentMaxRepeats);
        }
    }
}

//обработка клика по кнопке выбора метрики
void GUI::handleMetricButtonClick(int metricIndex) {
    //сбрасываем все кнопки метрик
    for (int i = 0; i < metricButtonStates.size(); ++i) {
        metricButtonStates[i] = false;
        metricButtons[i].setFillColor(sf::Color(254, 187, 187));
    }
    //активируем выбранную кнопку
    metricButtonStates[metricIndex] = true;
    metricButtons[metricIndex].setFillColor(sf::Color(180, 100, 100));
}

//запуск диалогового окна для ввода максимального количества повторов
//возвращает строковое значение: число или "MAX"
std::string GUI::runMaxRepeatsInput(const std::string& initialValue) {
    //цветовая схема окна ввода
    sf::Color backColor = backgroundColor;
    sf::Color inputColor = sf::Color(255, 255, 255);
    sf::Color outlineColor = textColor;
    sf::Color activeOutlineColor = buttonColor;

    //параметры окна
    float winWidth = 600.0f;
    float winHeight = 400.0f;
    float screenWidth = (float)window.getSize().x;
    float screenHeight = (float)window.getSize().y;
    //создание окна ввода с заголовком и кнопкой закрытия
    sf::RenderWindow inputWindow(sf::VideoMode(winWidth, winHeight), "Set repeats", sf::Style::Titlebar | sf::Style::Close);
    inputWindow.setPosition(sf::Vector2i(screenWidth / 2.0f - winWidth / 2.0f, screenHeight / 2.0f - winHeight / 2.0f));//размещаем по центру экрана

    std::string currentInput = (initialValue == "MAX") ? "max" : initialValue;
    //текстовое поле для отображения ввода
    sf::Text inputText(currentInput, font, 24);
    inputText.setFillColor(outlineColor);
    //текст-подсказка для пользователя
    sf::Text promptText("Enter a whole number (>0) or 'max':", font, 18);
    promptText.setFillColor(outlineColor);
    promptText.setPosition(20, 20);
    //поле ввода текста
    sf::RectangleShape inputBox(sf::Vector2f(winWidth - 40, 40));
    inputBox.setFillColor(inputColor);
    inputBox.setPosition(20, 70);
    inputBox.setOutlineThickness(2);
    inputBox.setOutlineColor(activeOutlineColor);
    //позиция текста внутри поля ввода
    inputText.setPosition(inputBox.getPosition().x + 10, inputBox.getPosition().y + 5);
    //текст ошибки для некорректного ввода
    sf::Text errorText("Incorrect input!", font, 16);
    errorText.setFillColor(sf::Color(200, 50, 50));
    errorText.setPosition(20, 120);
    bool showError = false;
    //кнопка подтверждения ввода
    sf::RectangleShape okButton(sf::Vector2f(80, 40));
    okButton.setFillColor(buttonColor);
    okButton.setPosition(winWidth / 2.0f - 40.0f, winHeight - 60.0f);
    sf::Text okText("OK", font, 20);
    okText.setFillColor(sf::Color::White);
    okText.setPosition(okButton.getPosition().x + 25, okButton.getPosition().y + 5);
    //значение по умолчанию для возврата
    std::string resultValue = initialValue;
    //главный цикл обработки событий окна ввода
    while (inputWindow.isOpen()) {
        sf::Event event;
        while (inputWindow.pollEvent(event)) {
            //обработка закрытия окна
            if (event.type == sf::Event::Closed) {
                inputWindow.close();
                return resultValue;
            }
            //обработка клавиши Escape для закрытия окна
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
                inputWindow.close();
                return resultValue;
            }
            //обработка клика мышью
            if (event.type == sf::Event::MouseButtonPressed) {
                sf::Vector2f mousePos = inputWindow.mapPixelToCoords(sf::Vector2i(event.mouseButton.x, event.mouseButton.y));
                // проверка клика по кнопке OK
                if (okButton.getGlobalBounds().contains(mousePos)) {
                    std::string tempInput = currentInput;
                    if (tempInput.empty()) tempInput = "max";//значение по умолчанию для пустого ввода

                    if (tempInput == "MAX" || tempInput == "max") {
                        inputWindow.close();
                        return "MAX";
                    }
                    //проверка числового ввода
                    try {
                        int value = std::stoi(tempInput);
                        if (value > 0) {
                            inputWindow.close();
                            return std::to_string(value);
                        }
                        else {
                            showError = true;
                        }
                    }
                    catch (...) {
                        showError = true;
                    }
                }
            }
            //обработка ввода текста
            if (event.type == sf::Event::TextEntered) {
                char c = static_cast<char>(event.text.unicode);
                //обработка Backspace - удаление последнего символа
                if (c == '\b') {
                    if (!currentInput.empty()) currentInput.pop_back();
                    showError = false;
                }
                //обработка Enter - подтверждение ввода
                else if (c == '\r' || c == '\n') {
                    std::string tempInput = currentInput;
                    if (tempInput.empty()) tempInput = "max";

                    if (tempInput == "MAX" || tempInput == "max") {
                        inputWindow.close();
                        return "MAX";
                    }
                    try {
                        int value = std::stoi(tempInput);
                        if (value > 0) {
                            inputWindow.close();
                            return std::to_string(value);
                        }
                        else {
                            showError = true;
                        }
                    }
                    catch (...) {
                        showError = true;
                    }
                }
                //добавление символов в строку ввода
                else if (currentInput.length() < 10) {
                    // разрешаем только цифры и символы для слова "max"
                    if (std::isdigit(c) || std::tolower(c) == 'm' || std::tolower(c) == 'a' || std::tolower(c) == 'x') {
                        currentInput += c;
                        showError = false;
                    }
                }
                inputText.setString(currentInput);
            }
        }
        //отрисовка интерфейса окна ввода
        inputWindow.clear(backColor);
        inputWindow.draw(promptText);
        inputWindow.draw(inputBox);
        inputWindow.draw(okButton);
        inputWindow.draw(okText);
        inputText.setString(currentInput);
        inputWindow.draw(inputText);
        //отображение текста ошибки при необходимости
        if (showError) {
            inputWindow.draw(errorText);
        }

        inputWindow.display();
    }
    return resultValue;
}

//обновление размера плитки мозаики
void GUI::updateTileSize() {
    currentTileSize += 10;
    if (currentTileSize > 100) {
        currentTileSize = 20;
    }
    tileSizeLabel.setString("Tile Size: " + std::to_string(currentTileSize));
}

//обновление шага выборки мозаики
void GUI::updateStepSize() {
    currentStepSize += 10;
    if (currentStepSize > 100) {
        currentStepSize = 20;
    }
    stepSizeLabel.setString("Step Size: " + std::to_string(currentStepSize));
}

//обновление шага выборки мозаики
void GUI::updateRotationAngle() {
    currentRotationAngle += 45;
    if (currentRotationAngle >= 360) {
        currentRotationAngle = 0;
    }
    rotationAngleLabel.setString("Rotation Angle: " + std::to_string(currentRotationAngle));
}

//обработка событий
void GUI::handleEvents() {
    sf::Event event;
    while (window.pollEvent(event)) {
        //обработка закрытия окна
        if (event.type == sf::Event::Closed)
            window.close();
        //обработка нажатия клавиши Escape для закрытия окна
        if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape)
            window.close();
        //обработка нажатия кнопки мыши
        if (event.type == sf::Event::MouseButtonPressed) {
            sf::Vector2i mousePos = sf::Mouse::getPosition(window);
            //клик по основным кнопкам интерфейса
            for (int i = 0; i < buttons.size(); ++i) {
                if (buttons[i].getGlobalBounds().contains(mousePos.x, mousePos.y)) {
                    handleButtonClick(i);
                    break;
                }
            }
            //клик по кнопкам выбора метрики
            for (int i = 0; i < metricButtons.size(); ++i) {
                if (metricButtons[i].getGlobalBounds().contains(mousePos.x, mousePos.y)) {
                    handleMetricButtonClick(i);
                    break;
                }
            }
            //клик по чекбоксам
            for (int i = 0; i < checkboxes.size(); ++i) {
                if (checkboxes[i].getGlobalBounds().contains(mousePos.x, mousePos.y)) {
                    handleCheckboxClick(i);
                    break;
                }
            }
            //клик по метке размера клетки
            if (tileSizeLabel.getGlobalBounds().contains(mousePos.x, mousePos.y)) {
                updateTileSize();
            }
            //клик по метке шага мозаики
            else if (stepSizeLabel.getGlobalBounds().contains(mousePos.x, mousePos.y)) {
                updateStepSize();
            }
            //клик по метке угла поворота, если отображение включено
            if (showRotationInput && rotationAngleLabel.getGlobalBounds().contains(mousePos.x, mousePos.y)) {
                updateRotationAngle();
            }
            //клик по метке максимальных повторов, если отображение включено
            if (showRepeatInput && maxRepeatsLabel.getGlobalBounds().contains(mousePos.x, mousePos.y)) {

                std::string newValue = runMaxRepeatsInput(currentMaxRepeats);
                if (newValue != currentMaxRepeats) {
                    currentMaxRepeats = newValue;
                    maxRepeatsLabel.setString("Max Repeats: " + currentMaxRepeats);
                }
            }
            //клик по кнопке просмотра оригинального изображения
            if (viewImageButton.getGlobalBounds().contains(mousePos.x, mousePos.y) &&
                !selectedImagePath.empty()) {
                showOriginalImage = !showOriginalImage;
                //проверка, что отображается только одно изображение
                if (showOriginalImage && showMosaicImage) {
                    showMosaicImage = false;
                }
            }
            //клик по кнопке просмотра мозаики
            if (viewMosaicButton.getGlobalBounds().contains(mousePos.x, mousePos.y) &&
                mosaicTexture.getSize().x > 0) {
                showMosaicImage = !showMosaicImage;
                //проверка, что отображается только одно изображение
                if (showMosaicImage && showOriginalImage) {
                    showOriginalImage = false;
                }
            }
            //клик по метке разрешения
            if (resolutionLabel.getGlobalBounds().contains(mousePos.x, mousePos.y)) {
                updateResolutionFormat();
            }
        }
        // Обработка колесика мыши для масштабирования мозаики
        if (event.type == sf::Event::MouseWheelScrolled) {
            sf::Vector2i mousePos = sf::Mouse::getPosition(window);
            //масштабирование только если курсор над изображением мозаики
            if (showMosaicImage && mosaicSprite.getGlobalBounds().contains(mousePos.x, mousePos.y)) {
                if (event.mouseWheelScroll.delta > 0) {
                    mosaicScale = std::min(maxScale, mosaicScale + scaleStep);
                }
                else {
                    mosaicScale = std::max(minScale, mosaicScale - scaleStep);
                }
                mosaicSprite.setScale(mosaicScale, mosaicScale);
            }
        }
    }
}
//отрисовка
void GUI::render() {
    window.clear(backgroundColor);
    //отрисовка основных кнопок и их меток
    for (const auto& button : buttons) {
        window.draw(button);
    }
    for (const auto& label : buttonLabels) {
        window.draw(label);
    }
    //отрисовка метрик
    window.draw(metricsTitle);
    for (size_t i = 0; i < metricButtons.size(); ++i) {
        window.draw(metricButtons[i]);
        if (metricButtonStates[i]) {
            sf::Text highlightedLabel = metricLabels[i];
            highlightedLabel.setFillColor(sf::Color(253, 240, 240));
            window.draw(highlightedLabel);
        }
        else {
            window.draw(metricLabels[i]);
        }
    }
    //отрисовка меток размеров
    window.draw(tileSizeLabel);
    window.draw(stepSizeLabel);
    //отрисовка чекбоксов и их меток
    for (const auto& checkbox : checkboxes) {
        window.draw(checkbox);
    }
    for (const auto& label : checkboxLabels) {
        window.draw(label);
    }
    //отрисовка внутренних маркеров чекбоксов
    for (const auto& innerShape : innerCheckboxShapes) {
        window.draw(innerShape);
    }
    //отрисовка дополнительных параметров чекбоксов, если они активны
    if (showRotationInput) {
        window.draw(rotationAngleLabel);
    }

    if (showRepeatInput) {
        window.draw(maxRepeatsLabel);
    }
    //отрисовка сообщения
    if (showMessageFlag) {
        window.draw(messageBox);
        window.draw(messageText);
    }
    //отрисовка информационных текстов
    window.draw(selectedImageText);
    window.draw(selectedFolderText);
    //отрисовка кнопок просмотра изображений
    window.draw(viewImageButton);
    window.draw(viewImageButtonLabel);
    window.draw(viewMosaicButton);
    window.draw(viewMosaicButtonLabel);
    //отрисовка оригинального изображения
    if (showOriginalImage) {
        window.draw(originalImageSprite);
    }
    //отрисовка мозаики
    if (showMosaicImage) {
        window.draw(mosaicSprite);
    }
    //отрисовка индикатора загрузки
    if (showLoading) {
        window.draw(loadingBackground);
        window.draw(loadingText);
    }
    //отрисовка метки разрешения
    window.draw(resolutionLabel);

    window.display();
}
