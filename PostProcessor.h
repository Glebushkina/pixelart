#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <memory>

//структура постобработки мозаики
struct PostProcessConfig {
    std::vector<std::pair<std::string, double>> effects; //пары: эффект и интенсивность
    int gridSize = 30; //размер сетки для эффектов, зависящих от структуры мозаики
    //добавление эффекта в структуру
    void addEffect(const std::string& name, double intensity = 0.5) {
        effects.emplace_back(name, intensity);
    }
    //очищение списка эффектов
    void clearEffects() {
        effects.clear();
    }
};

//родительский класс для всех эффектов постобработки
//определяет методы, которые должны реализовать все эффекты
class PostProcessEffect {
public:
    virtual ~PostProcessEffect() = default;
    //применение эффекта к мозаике, возвращает обработанное изображение
    virtual cv::Mat apply(const cv::Mat& mosaic, const cv::Mat& original) = 0;
    //геттер имени эффекта
    virtual std::string getName() const = 0;
    //сеттер для размера сетки
    virtual void setGridSize(int gridSize) {}
};

//класс: цветокоррекция мозаики
//выравнивает цветовые характеристики мозаики по оригинальному изображению
class ColorCorrectionEffect : public PostProcessEffect {
private:
    double intensity = 0.5; //интенсивность коррекции цвета

public:
    cv::Mat apply(const cv::Mat& mosaic, const cv::Mat& original) override;
    std::string getName() const override;
};

//класс: альфа-смешивание с оригинальным изображением
//сохраняет детали оригинального изображения
class AlphaBlendEffect : public PostProcessEffect {
private:
    double alpha = 0.5; //коэффициент смешивания

public:
    cv::Mat apply(const cv::Mat& mosaic, const cv::Mat& original) override;
    std::string getName() const override;
};

//класс: сглаживание швов между клетками мозаики
//уменьшает видимость границ между отдельными тайлами
class SeamSmoothingEffect : public PostProcessEffect {
private:
    double intensity = 0.5; //интенсивность сглаживания
    int gridSize = 30; //размер сетки мозаики для определения положения швов

    //создание маски областей швов для применения размытия
    //бинарная маска (255 - области швов, 0 - остальное)
    cv::Mat createSeamMask(const cv::Mat& mosaic) const;

public:
    cv::Mat apply(const cv::Mat& mosaic, const cv::Mat& original) override;
    std::string getName() const override;;
    void setGridSize(int gridSize) override;
};

//создание эффектов постобработки
class EffectFactory {
public:
    //возращает unique_ptr на созданный эффект или nullptr, если имя неизвестно
    static std::unique_ptr<PostProcessEffect> createEffect(const std::string& effectName);
};

//сборка постобработки
class PostProcessPipeline {
private:
    std::vector<std::unique_ptr<PostProcessEffect>> effects;//эффекты
    int gridSize = 30;//размер сетки для передачи эффектам

public:
    //постобработка на основе параметров из PostProcessConfig
    void setup(const PostProcessConfig& config);
    //променение эффектов к изображению
    cv::Mat process(const cv::Mat& mosaic, const cv::Mat& original);
};
