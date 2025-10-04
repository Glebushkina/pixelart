#pragma once

#include <vector>
#include <string>
#include <memory>
#include <filesystem>
#include <opencv2/opencv.hpp>
#include "PostProcessor.h"
#include <limits>
#include <algorithm>

namespace fs = std::filesystem;

//структура для вспомогательных функций вычисления признаков
struct FeatureUtils {
    //вычисляет стандартное отклонение (контрастность) изображения
    static cv::Scalar computeStdDev(const cv::Mat& image);
    //вычисляет истограмму градиентов (HOG-подобный признак)
    static cv::Mat computeGradientHist(const cv::Mat& image);
    //вспомогательная функция для вычисления LBP-значения
    static uchar getLBPValue(const cv::Mat& gray, int r, int c);
    //вычисляет гистограмму LBP-признаков для текстуры
    static cv::Mat computeLBPFeatures(const cv::Mat& image);
};
//структура с параметрами тайтлов
struct Tile {
    cv::Mat image;//изображение тайла
    cv::Scalar color;//средний цвет
    cv::Scalar stddev;//стандартное отклонение(контрастность)
    cv::Mat gradientHist;//гистограмма градиентов
    cv::Mat textureFeatures;//LBP-признаки текстуры
    int usage = 0;//счетчик использования тайтла
    int angle = 0;//угол повороты тайтла
    int originalIndex = -1;//индекс исходного изображения
};
//структура с параметрами конфигурации
struct Config {
    int tileSize = 30;//размер тайтла
    int gridStep = 30;//размер клетки
    int maxRepeats = std::numeric_limits<int>::max();//кол-во повторов тайтлов
    bool repeats = false;//разрешение повторов
    bool rotation = false;//разрешение поворота 
    int rotationAngle = 0;//угол поворота тайтла
    std::string metric = "color";//название матрики
};
//родительский класс для всех метрик
//определяет методы, которые должны реализовать все метрики
class IMetric {
public:
    virtual ~IMetric() = default;
    //вычисляет параметры клетки
    virtual void computeCellFeatures(Tile& cell, const cv::Mat& cellImage) = 0;
    //вычисляет параметры тайтла
    virtual void computeTileFeatures(Tile& tile, const cv::Mat& tileImage) = 0;
    //вычисляет расстояние между параметрами клетки и тайла
    virtual double distance(const Tile& cell, const Tile& tile) const = 0;
    //геттер для получения имени метрики
    virtual std::string getName() const = 0;
};
//класс цветной метрики
class ColorMetric : public IMetric {
public:
    void computeCellFeatures(Tile& cell, const cv::Mat& cellImage) override;
    void computeTileFeatures(Tile& tile, const cv::Mat& tileImage) override;
    double distance(const Tile& cell, const Tile& tile) const override;
    std::string getName() const override;
};
//класс метрики цвет+контраст
class ColorContrastMetric : public IMetric {
public:
    void computeCellFeatures(Tile& cell, const cv::Mat& cellImage) override;
    void computeTileFeatures(Tile& tile, const cv::Mat& tileImage) override;
    double distance(const Tile& cell, const Tile& tile) const override;
    std::string getName() const override;
};
//класс метрики градиента
class GradientMetric : public IMetric {
public:
    void computeCellFeatures(Tile& cell, const cv::Mat& cellImage) override;
    void computeTileFeatures(Tile& tile, const cv::Mat& tileImage) override;
    double distance(const Tile& cell, const Tile& tile) const override;
    std::string getName() const override;
};
//класс метрики текстуры
class TextureMetric : public IMetric {
public:
    void computeCellFeatures(Tile& cell, const cv::Mat& cellImage) override;
    void computeTileFeatures(Tile& tile, const cv::Mat& tileImage) override;
    double distance(const Tile& cell, const Tile& tile) const override;
    std::string getName() const override;
};
//класс создания мозаики
class MosaicGenerator {
private:
    std::vector<Tile> tiles;//тайтлы
    std::unique_ptr<IMetric> metric;//текущая метрика сравнения
    PostProcessPipeline postProcessor;//объект класса PostProcessPipeline (для постобработки)
    //вычисляет параметры тайла с помощью текущей метрики
    void computeTileFeatures(Tile& tile, const cv::Mat& image) const;
    //создает мозаику без обработки
    cv::Mat createRawMosaic(const cv::Mat& source, const Config& cfg);

public:
    //загрузка тайтлов из папки
    bool loadTiles(const fs::path& folder, int size, bool enableRotation = false, int rotation = 0);
    //создает итоговую мозаику с постобработкой
    cv::Mat createMosaic(const cv::Mat& source, const Config& cfg);
    //сеттер метрики сравнения по имени
    bool setMetric(const std::string& metricName);
    //считает кол-во загруженных тайтлов
    size_t getTilesCount() const { return tiles.size(); }
    //удаляем тайтлы
    void clearTiles() { tiles.clear(); }
    //настройка параметров постобработки
    void setPostProcessConfig(const PostProcessConfig& config) {
        postProcessor.setup(config);
    }
};
