#include "PostProcessor.h"
#include <iostream>
#include <algorithm>

//класс ColorCorrectionEffect
//цветокоррекция мозаики на основе оригинального изображения
// (выравнивает средние значения цветовых каналов мозаики по оригиналу)
cv::Mat ColorCorrectionEffect::apply(const cv::Mat& mosaic, const cv::Mat& original) {
    cv::Mat corrected = mosaic.clone();
    cv::Mat originalResized;

    //приводим оригинальное изображение к размеру мозаики
    cv::resize(original, originalResized, mosaic.size());

    //вычисляем средние значения цветов для обоих изображений
    cv::Scalar mosaicMean = cv::mean(mosaic);
    cv::Scalar originalMean = cv::mean(originalResized);

    //разделяем изображение на цветовые каналы для независимой коррекции
    std::vector<cv::Mat> mosaicChannels, correctedChannels;
    cv::split(mosaic, mosaicChannels);

    //корректируем каждый цветовой канал отдельно
    for (int i = 0; i < mosaic.channels(); i++) {
        //коэффициент масштабирования для канала
        // +1e-5 для избежания деления на ноль
        double scale = originalMean[i] / (mosaicMean[i] + 1e-5);

        //применяем интенсивность эффекта к коэффициенту
        scale = 1.0 + (scale - 1.0) * intensity;

        //применяем коррекцию к каналу
        cv::Mat correctedChannel;
        mosaicChannels[i].convertTo(correctedChannel, CV_32F); //float для точных вычислений
        correctedChannel *= scale; //применяем масштабирование
        correctedChannel.convertTo(correctedChannel, mosaicChannels[i].type()); //возвращаем к исходному типу

        correctedChannels.push_back(correctedChannel);
    }

    //объединяем скорректированные каналы обратно в цветное изображение
    cv::merge(correctedChannels, corrected);
    return corrected;
}

//геттер эффекта цветокоррекции
std::string ColorCorrectionEffect::getName() const {
    return "color_correction";
}

//класс AlphaBlendEffectс
//смешивает мозаику с оригинальным изображением через альфа-канал
cv::Mat AlphaBlendEffect::apply(const cv::Mat& mosaic, const cv::Mat& original) {
    cv::Mat originalResized;
    //приводим оригинальное изображение к размеру мозаики
    cv::resize(original, originalResized, mosaic.size());

    //взвешенное сложение двух изображений
    cv::Mat blended;
    cv::addWeighted(mosaic, 1.0 - alpha, originalResized, alpha, 0, blended);

    return blended;
}

//геттер эффекта альфа-смешивания
std::string AlphaBlendEffect::getName() const {
    return "alpha_blend";
}

//класс SeamSmoothingEffect 
//сглаживает видимые швы между плитками мозаики(размытием)
cv::Mat SeamSmoothingEffect::apply(const cv::Mat& mosaic, const cv::Mat& original) {
    cv::Mat smoothed = mosaic.clone();

    //маска областей швов
    cv::Mat seamMask = createSeamMask(mosaic);

    //если нет швов для сглаживания, возвращаем исходное
    if (cv::countNonZero(seamMask) == 0) {
        return smoothed;
    }

    //параметры размытия
    int blurSize = 3 + static_cast<int>(intensity * 5);
    if (blurSize % 2 == 0) blurSize++;
    blurSize = std::max(3, std::min(blurSize, std::min(mosaic.rows, mosaic.cols)));

    if (blurSize < 3) return mosaic.clone();

    double sigma = intensity * 2.0;

    //размываем только швы
    cv::Mat blurred;
    cv::GaussianBlur(mosaic, blurred, cv::Size(blurSize, blurSize), sigma);

    //смешиваем размытую версию с оригиналом в областях швов
    double blendFactor = intensity * 0.7;

    //временное изображение для смешивания
    cv::Mat blended = mosaic.clone();
    cv::addWeighted(mosaic, 1.0 - blendFactor, blurred, blendFactor, 0, blended);

    //копируем сглаженные области швов из blended в результат
    blended.copyTo(smoothed, seamMask);

    return smoothed;
}

//геттер эффекта сглаживания швов
std::string SeamSmoothingEffect::getName() const {
    return "seam_smoothing";
}

//сеттер для размера сетки
void SeamSmoothingEffect::setGridSize(int gridSize) {
    this->gridSize = gridSize;
}

//бинарная маска областей швов между плитками мозаики
//определяет вертикальные и горизонтальные границы сетки мозаики
cv::Mat SeamSmoothingEffect::createSeamMask(const cv::Mat& mosaic) const {
    cv::Mat mask = cv::Mat::zeros(mosaic.size(), CV_8UC1);

    if (gridSize <= 0 || intensity < 0.01) return mask;

    //фиксированная ширина 1-3 пикселя
    int lineWidth = 1 + static_cast<int>(intensity * 2);

    //вертикальные швы
    for (int x = gridSize; x < mosaic.cols; x += gridSize) {
        int startX = std::max(0, x - lineWidth / 2);
        int endX = std::min(mosaic.cols, startX + lineWidth);

        cv::rectangle(mask,
            cv::Rect(startX, 0, endX - startX, mosaic.rows),
            cv::Scalar(255), -1);
    }

    //горизонтальные швы  
    for (int y = gridSize; y < mosaic.rows; y += gridSize) {
        int startY = std::max(0, y - lineWidth / 2);
        int endY = std::min(mosaic.rows, startY + lineWidth);

        cv::rectangle(mask,
            cv::Rect(0, startY, mosaic.cols, endY - startY),
            cv::Scalar(255), -1);
    }

    return mask;
}

//класс EffectFactory
//создает экземпляр эффекта постобработки по его имени
//возвращает unique_ptr на созданный эффект или nullptr для неизвестных эффектов
std::unique_ptr<PostProcessEffect> EffectFactory::createEffect(const std::string& effectName) {
    if (effectName == "color_correction") {
        return std::make_unique<ColorCorrectionEffect>();
    }
    else if (effectName == "alpha_blend") {
        return std::make_unique<AlphaBlendEffect>();
    }
    else if (effectName == "seam_smoothing") {
        return std::make_unique<SeamSmoothingEffect>();
    }
    else {
        return nullptr; //неизвестный эффект
    }
}

//класс PostProcessPipeline
//сборка постобработки
void PostProcessPipeline::setup(const PostProcessConfig& config) {
    effects.clear(); //очищаем предыдущие эффекты
    gridSize = config.gridSize; //сохраняем размер сетки

    //создаем и настраиваем каждый эффект из конфигурации
    for (const auto& [effectName, intensity] : config.effects) {
        auto effect = EffectFactory::createEffect(effectName);
        if (effect) {
            //для эффекта сглаживания швов дополнительно устанавливаем размер сетки
            if (auto seamEffect = dynamic_cast<SeamSmoothingEffect*>(effect.get())) {
                seamEffect->setGridSize(gridSize);
            }

            //добавляем эффект в цепочку
            effects.push_back(std::move(effect));
        }
    }
}

//применяет всю цепочку эффектов к изображению мозаики
//(эффекты применяются последовательно в порядке их добавления в конфигурации)
cv::Mat PostProcessPipeline::process(const cv::Mat& mosaic, const cv::Mat& original) {
    cv::Mat result = mosaic.clone();

    //последовательно применяем все эффекты из цепочки
    for (const auto& effect : effects) {
        result = effect->apply(result, original);
    }

    return result; //возвращаем финальный результат
}
