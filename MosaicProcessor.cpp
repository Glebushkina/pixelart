#include "MosaicProcessor.h"
#include <iostream>
#include <algorithm>
#include <numeric>
#include <cmath>

//структура FeatureUtils
//вспомогательные функции вычисления признаков

//вычисляет стандартное отклонение (контрастность) изображения по каналам
cv::Scalar FeatureUtils::computeStdDev(const cv::Mat& image) {
    cv::Mat mean, stddev;
    
    cv::meanStdDev(image, mean, stddev);//вычисляет среднее значение и стандартное отклонение по каналам

    if (image.channels() == 3) {//для цветного BGR изображения возвращаем отклонения по всем трем каналам
        //возвращает скаляр со значениями стандартного отклонения для каждого канала
        return cv::Scalar(stddev.at<double>(0, 0), 
            stddev.at<double>(1, 0),
            stddev.at<double>(2, 0));
    }
    //для одноканального изображения возвращаем одно значение
    return cv::Scalar(stddev.at<double>(0, 0));
}

//вычисляет гистограмму направлений градиентов(HOG - подобный признак)
//гистограмма показывает распределение направлений краев в изображении
cv::Mat FeatureUtils::computeGradientHist(const cv::Mat& image) {
    //проверка входного изображения
    if (image.empty() || image.rows < 3 || image.cols < 3) {
        return cv::Mat::zeros(36, 1, CV_32F);
    }
    cv::Mat gray, grad_x, grad_y, magnitude, angle;
    //преобразование в оттенки серого для цветных изображений
    if (image.channels() == 3) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    }
    else {
        gray = image;
    }
    gray.convertTo(gray, CV_32F);
    //вычисление градиентов по осям X и Y с помощью оператора Собеля
    cv::Sobel(gray, grad_x, CV_32F, 1, 0, 3);
    cv::Sobel(gray, grad_y, CV_32F, 0, 1, 3);
    //преобразование декартовых координат в полярные (величина и угол)
    cv::cartToPolar(grad_x, grad_y, magnitude, angle, true);
    //создание гистограммы с 36 бинами (по 10 градусов на бин)
    const int histSize = 36;
    cv::Mat hist = cv::Mat::zeros(histSize, 1, CV_32F);
    float angle_step = 360.0f / histSize;
    //заполнение гистограммы величинами градиентов
    for (int y = 0; y < angle.rows; ++y) {
        for (int x = 0; x < angle.cols; ++x) {
            float angle_val = angle.at<float>(y, x);
            float mag_val = magnitude.at<float>(y, x);
            int bin = static_cast<int>(angle_val / angle_step);
            bin = std::max(0, std::min(bin, histSize - 1));
            hist.at<float>(bin) += mag_val;
        }
    }
    //нормализация гистограммы
    cv::normalize(hist, hist, 1.0, 0.0, cv::NORM_L1);
    hist.convertTo(hist, CV_32F);
    return hist;
}

//метод для вычисления LBP-значения для пикселя
//LBP (Local Binary Pattern) - метод описания текстуры изображения
uchar FeatureUtils::getLBPValue(const cv::Mat& gray, int r, int c) {
    uchar center = gray.at<uchar>(r, c);
    uchar value = 0;
    //сравнение центрального пикселя с 8 соседями и формирование бинарного кода
    value |= (gray.at<uchar>(r - 1, c - 1) > center) << 7;
    value |= (gray.at<uchar>(r - 1, c) > center) << 6;
    value |= (gray.at<uchar>(r - 1, c + 1) > center) << 5;
    value |= (gray.at<uchar>(r, c + 1) > center) << 4;
    value |= (gray.at<uchar>(r + 1, c + 1) > center) << 3;
    value |= (gray.at<uchar>(r + 1, c) > center) << 2;
    value |= (gray.at<uchar>(r + 1, c - 1) > center) << 1;
    value |= (gray.at<uchar>(r, c - 1) > center) << 0;
    return value;
}

//вычисляет LBP-признаки для описания текстуры изображения
//возвращает гистограмму LBP-кодов
cv::Mat FeatureUtils::computeLBPFeatures(const cv::Mat& image) {
    //проверка входного изображения
    if (image.empty() || image.rows < 3 || image.cols < 3) {
        return cv::Mat::zeros(256, 1, CV_32F);
    }
    //преобразование в оттенки серого для цветных изображений
    cv::Mat gray;
    if (image.channels() == 3) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    }
    else {
        gray = image;
    }
    //создание LBP-карты
    cv::Mat lbp = cv::Mat::zeros(gray.rows - 2, gray.cols - 2, CV_8UC1);
    for (int i = 1; i < gray.rows - 1; i++) {
        for (int j = 1; j < gray.cols - 1; j++) {
            lbp.at<uchar>(i - 1, j - 1) = getLBPValue(gray, i, j);
        }
    }
    //вычисление гистограммы LBP-кодов
    const int histSize = 256;
    float range[] = { 0, 256 };
    const float* ranges[] = { range };
    cv::Mat hist;
    cv::calcHist(&lbp, 1, 0, cv::Mat(), hist, 1, &histSize, ranges, true, false);
    //нормализация гистограммы
    cv::normalize(hist, hist, 1.0, 0.0, cv::NORM_L1);
    hist.convertTo(hist, CV_32F);
    return hist;
}

//класс ColorMetric
//вычисляет параметры клетки
void ColorMetric::computeCellFeatures(Tile& cell, const cv::Mat& cellImage) {
    cell.color = cv::mean(cellImage);
}
//вычисляет параметры тайтла
void ColorMetric::computeTileFeatures(Tile& tile, const cv::Mat& tileImage) {
    tile.color = cv::mean(tileImage);
}
//вычисляет расстояние между параметрами клетки и тайла на основе цвета
double ColorMetric::distance(const Tile& cell, const Tile& tile) const {
    return cv::norm(cell.color, tile.color);
}
//геттер для получения имени метрики
std::string ColorMetric::getName() const {
    return "color";
}

//класс ColorContrastMetric
//вычисляет параметры клетки
void ColorContrastMetric::computeCellFeatures(Tile& cell, const cv::Mat& cellImage) {
    cell.color = cv::mean(cellImage);
    cell.stddev = FeatureUtils::computeStdDev(cellImage);
}
//вычисляет параметры тайтла
void ColorContrastMetric::computeTileFeatures(Tile& tile, const cv::Mat& tileImage) {
    tile.color = cv::mean(tileImage);
    tile.stddev = FeatureUtils::computeStdDev(tileImage);
}
//вычисляет расстояние между параметрами клетки и тайла на основе цвета и контрастности
double ColorContrastMetric::distance(const Tile& cell, const Tile& tile) const {
    double colorDist = cv::norm(cell.color, tile.color);
    double stddevDist = cv::norm(cell.stddev, tile.stddev);
    return colorDist + 2.0 * stddevDist;
}
//геттер для получения имени метрики
std::string ColorContrastMetric::getName() const {
    return "color_contrast";
}

//класс GradientMetric
//вычисляет параметры клетки
void GradientMetric::computeCellFeatures(Tile& cell, const cv::Mat& cellImage) {
    cell.gradientHist = FeatureUtils::computeGradientHist(cellImage);
}
//вычисляет параметры тайтла
void GradientMetric::computeTileFeatures(Tile& tile, const cv::Mat& tileImage) {
    tile.gradientHist = FeatureUtils::computeGradientHist(tileImage);
}
//вычисляет расстояние между параметрами клетки и тайла на основе гистограмм градиентов
double GradientMetric::distance(const Tile& cell, const Tile& tile) const {
    //проверка гистограмм (не пустые)
    if (cell.gradientHist.empty() || tile.gradientHist.empty()) {
        return std::numeric_limits<double>::max();
    }
    //вычисление расстояния Бхаттачарии между гистограммами
    double histDist = cv::compareHist(cell.gradientHist, tile.gradientHist, cv::HISTCMP_BHATTACHARYYA);

    // Масштабируем до диапазона, сравнимого с цветовыми метриками
    return histDist * 1000.0;
}
//геттер для получения имени метрики
std::string GradientMetric::getName() const {
    return "gradient";
}

//класс TextureMetric
//вычисляет параметры клетки
void TextureMetric::computeCellFeatures(Tile& cell, const cv::Mat& cellImage) {
    cell.textureFeatures = FeatureUtils::computeLBPFeatures(cellImage);
}
//вычисляет параметры тайтла
void TextureMetric::computeTileFeatures(Tile& tile, const cv::Mat& tileImage) {
    tile.textureFeatures = FeatureUtils::computeLBPFeatures(tileImage);
}
//вычисляет расстояние между параметрами клетки и тайла на основе текстурных признаков
double TextureMetric::distance(const Tile& cell, const Tile& tile) const {
    const cv::Mat& hist1 = cell.textureFeatures;
    const cv::Mat& hist2 = tile.textureFeatures;
    //проверка гистограмм
    if (hist1.empty() || hist2.empty() || hist1.size() != hist2.size()) {
        return std::numeric_limits<double>::max();
    }
    //вычисление расстояния Бхаттачарии между гистограммами LBP
    double histDist = cv::compareHist(hist1, hist2, cv::HISTCMP_BHATTACHARYYA);
    return histDist * 1000.0;
}
//геттер для получения имени метрики
std::string TextureMetric::getName() const {
    return "texture";
}
//класс MosaicGenerator - класс для создания мозаики
//сеттер метрики по имени
bool MosaicGenerator::setMetric(const std::string& metricName) {
    if (metricName == "color") {
        metric = std::make_unique<ColorMetric>();
    }
    else if (metricName == "color_contrast") {
        metric = std::make_unique<ColorContrastMetric>();
    }
    else if (metricName == "gradient") {
        metric = std::make_unique<GradientMetric>();
    }
    else if (metricName == "texture") {
        metric = std::make_unique<TextureMetric>();
    }
    else {
        return false;
    }

    //пересчитываем параметры для всех уже загруженных тайлов
    for (auto& tile : tiles) {
        computeTileFeatures(tile, tile.image);
    }

    return true;
}

//вычисление признаков для тайла с помощью текущей метрики
void MosaicGenerator::computeTileFeatures(Tile& tile, const cv::Mat& image) const {
    if (metric) {
        metric->computeTileFeatures(tile, image);
    }
}

//загрузка тайлов из указанной папки
bool MosaicGenerator::loadTiles(const fs::path& folder, int size, bool enableRotation, int rotation) {
    //метрика по умолчанию
    if (!metric) {
        setMetric("color");
    }

    tiles.clear();
    int originalIndex = 0;
    //обход всех файлов в указанной папке
    for (const auto& entry : fs::directory_iterator(folder)) {
        if (entry.is_regular_file()) {
            cv::Mat originalTile = cv::imread(entry.path().string(), cv::IMREAD_COLOR);

            if (!originalTile.empty()) {
                //изменение размера тайла до заданного
                cv::Mat resizedTile;
                cv::resize(originalTile, resizedTile, cv::Size(size, size));
                //определение угла поворота для тайла
                std::vector<int> angles;
                if (enableRotation) {
                    angles.push_back(rotation);
                }
                else {
                    angles.push_back(0);
                }
                //поворот всех тайтлов на заданный угол
                for (int angle : angles) {
                    cv::Mat rotatedTile;
                    if (angle != 0) {
                        cv::Point2f center((float)size / 2, (float)size / 2);
                        cv::Mat rot_mat = cv::getRotationMatrix2D(center, angle, 1.0);
                        cv::warpAffine(resizedTile, rotatedTile, rot_mat, resizedTile.size(),
                            cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
                    }
                    else {
                        rotatedTile = resizedTile.clone();
                    }
                    //создание и настройка нового тайла
                    Tile newTile;
                    newTile.image = rotatedTile;
                    newTile.angle = angle;
                    newTile.originalIndex = originalIndex;
                    //вычисление параметров для тайла
                    computeTileFeatures(newTile, newTile.image);
                    tiles.push_back(newTile);
                }
                originalIndex++;
            }
        }
    }
    return !tiles.empty();
}

//создание мозаики без постобработки
cv::Mat MosaicGenerator::createRawMosaic(const cv::Mat& source, const Config& cfg) {
    //метрика по умолчанию
    if (!metric) setMetric("color");

    int targetWidth = source.cols;
    int targetHeight = source.rows;
    //создание пустого изображения для мозаики
    cv::Mat rawMosaic(targetHeight, targetWidth, source.type(), cv::Scalar(0, 0, 0));
    //обработка изображения по клеткам сетки
    for (int y = 0; y < targetHeight; y += cfg.gridStep) {
        for (int x = 0; x < targetWidth; x += cfg.gridStep) {
            Tile currentCell;
            //определение размеров текущей клетки
            int blockWidth = std::min(cfg.gridStep, targetWidth - x);
            int blockHeight = std::min(cfg.gridStep, targetHeight - y);

            cv::Rect region(x, y, blockWidth, blockHeight);
            cv::Mat cellImage = source(region);
            //вычисление признаков для текущей клетки
            metric->computeCellFeatures(currentCell, cellImage);

            //поиск наилучшего тайла по индексу в оригинальном векторе
            int bestIndex = -1;
            double bestDistance = std::numeric_limits<double>::max();

            for (int i = 0; i < tiles.size(); ++i) {
                if (tiles[i].usage < cfg.maxRepeats) { 
                double dist = metric->distance(currentCell, tiles[i]);
                if (dist < bestDistance) {
                    bestDistance = dist;
                    bestIndex = i;
                }
                }
            }
            //если не найден подходящий тайл, используем средний цвет клетки
            if (bestIndex == -1) {
                cv::Mat colorBlock(blockHeight, blockWidth, source.type(), cv::mean(cellImage));
                colorBlock.copyTo(rawMosaic(region));
                continue;
            }

            //увеличиваем счетчик использования
            tiles[bestIndex].usage++;
            //изменение размера тайла и копирование в мозаику
            cv::Mat finalTile;
            cv::resize(tiles[bestIndex].image, finalTile, cv::Size(blockWidth, blockHeight), 0, 0, cv::INTER_CUBIC);
            finalTile.copyTo(rawMosaic(region));
        }
    }
    return rawMosaic;
}

//создание итоговой мозаики с постобработкой
cv::Mat MosaicGenerator::createMosaic(const cv::Mat& source, const Config& cfg) {
    //проверка наличия загруженных тайлов
    if (tiles.empty()) throw std::runtime_error("No tiles loaded");
    //установка метрики сравнения
    if (!setMetric(cfg.metric)) {
        throw std::runtime_error("Invalid metric name specified: " + cfg.metric);
    }
    //сброс счетчиков использования тайтла перед генерацией
    for (auto& tile : tiles) {
        tile.usage = 0;
    }
    //создание мозаики и применение постобработки
    cv::Mat rawMosaic = createRawMosaic(source, cfg);
    return postProcessor.process(rawMosaic, source);
}
