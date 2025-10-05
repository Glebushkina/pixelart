import cv2
import numpy as np
from typing import List, Tuple, Optional

class PostProcessConfig:
    def __init__(self):
        self.effects: List[Tuple[str, float]] = []
        self.grid_size: int = 30
    
    def add_effect(self, name: str, intensity: float = 0.5):
        self.effects.append((name, intensity))
    
    def clear_effects(self):
        self.effects.clear()


class PostProcessEffect:
    def apply(self, mosaic: np.ndarray, original: np.ndarray) -> np.ndarray:
        raise NotImplementedError
    
    def get_name(self) -> str:
        raise NotImplementedError
    
    def set_grid_size(self, grid_size: int):
        pass


class ColorCorrectionEffect(PostProcessEffect):
    def __init__(self, intensity: float = 0.5):
        self.intensity = intensity
    
    def apply(self, mosaic: np.ndarray, original: np.ndarray) -> np.ndarray:
        # Добавляем проверку на нулевую интенсивность
        if self.intensity <= 0.01:
            return mosaic.copy()
            
        corrected = mosaic.copy()
        original_resized = cv2.resize(original, (mosaic.shape[1], mosaic.shape[0]))
        
        mosaic_mean = cv2.mean(mosaic)
        original_mean = cv2.mean(original_resized)
        
        # Разделение на каналы
        mosaic_channels = cv2.split(mosaic)
        corrected_channels = []
        
        for i in range(len(mosaic_channels)):
            scale = original_mean[i] / (mosaic_mean[i] + 1e-5)
            scale = 1.0 + (scale - 1.0) * self.intensity
            
            corrected_channel = mosaic_channels[i].astype(np.float32) * scale
            corrected_channel = np.clip(corrected_channel, 0, 255).astype(np.uint8)
            corrected_channels.append(corrected_channel)
        
        corrected = cv2.merge(corrected_channels)
        return corrected
    
    def get_name(self) -> str:
        return "color_correction"


class AlphaBlendEffect(PostProcessEffect):
    def __init__(self, alpha: float = 0.5):
        self.alpha = alpha
    
    def apply(self, mosaic: np.ndarray, original: np.ndarray) -> np.ndarray:
        # Добавляем проверку на нулевую интенсивность
        if self.alpha <= 0.01:
            return mosaic.copy()
            
        original_resized = cv2.resize(original, (mosaic.shape[1], mosaic.shape[0]))
        blended = cv2.addWeighted(mosaic, 1.0 - self.alpha, original_resized, self.alpha, 0)
        return blended
    
    def get_name(self) -> str:
        return "alpha_blend"


class SeamSmoothingEffect(PostProcessEffect):
    def __init__(self, intensity: float = 0.5, grid_size: int = 30):
        self.intensity = intensity
        self.grid_size = grid_size
    
    def apply(self, mosaic: np.ndarray, original: np.ndarray) -> np.ndarray:
        # Добавляем проверку на нулевую интенсивность
        if self.intensity <= 0.01:
            return mosaic.copy()
            
        smoothed = mosaic.copy()
        seam_mask = self._create_seam_mask(mosaic)
        
        if cv2.countNonZero(seam_mask) == 0:
            return smoothed
        
        # Параметры размытия
        blur_size = 3 + int(self.intensity * 5)
        if blur_size % 2 == 0:
            blur_size += 1
        blur_size = max(3, min(blur_size, min(mosaic.shape[0], mosaic.shape[1])))
        
        if blur_size < 3:
            return mosaic.copy()
        
        sigma = self.intensity * 2.0
        
        # Размытие
        blurred = cv2.GaussianBlur(mosaic, (blur_size, blur_size), sigma)
        
        # Смешивание
        blend_factor = self.intensity * 0.7
        blended = cv2.addWeighted(mosaic, 1.0 - blend_factor, blurred, blend_factor, 0)
        
        # Копирование сглаженных областей - ИСПРАВЛЕННАЯ ЧАСТЬ
        # Используем numpy индексирование вместо copyTo
        mask_indices = seam_mask > 0
        smoothed[mask_indices] = blended[mask_indices]
        
        return smoothed
    
    def get_name(self) -> str:
        return "seam_smoothing"
    
    def set_grid_size(self, grid_size: int):
        self.grid_size = grid_size
    
    def _create_seam_mask(self, mosaic: np.ndarray) -> np.ndarray:
        mask = np.zeros(mosaic.shape[:2], dtype=np.uint8)
        
        if self.grid_size <= 0 or self.intensity < 0.01:
            return mask
        
        line_width = 1 + int(self.intensity * 2)
        
        # Вертикальные швы
        for x in range(self.grid_size, mosaic.shape[1], self.grid_size):
            start_x = max(0, x - line_width // 2)
            end_x = min(mosaic.shape[1], start_x + line_width)
            mask[:, start_x:end_x] = 255
        
        # Горизонтальные швы
        for y in range(self.grid_size, mosaic.shape[0], self.grid_size):
            start_y = max(0, y - line_width // 2)
            end_y = min(mosaic.shape[0], start_y + line_width)
            mask[start_y:end_y, :] = 255
        
        return mask


class EffectFactory:
    @staticmethod
    def create_effect(effect_name: str, intensity: float = 0.5) -> Optional[PostProcessEffect]:
        effects = {
            "color_correction": ColorCorrectionEffect,
            "alpha_blend": AlphaBlendEffect,
            "seam_smoothing": SeamSmoothingEffect
        }
        
        if effect_name in effects:
            effect_class = effects[effect_name]
            effect = effect_class(intensity)
            
            # Для эффекта сглаживания швов устанавливаем интенсивность
            if effect_name == "seam_smoothing":
                effect.intensity = intensity
                
            return effect
        return None


class PostProcessPipeline:
    def __init__(self):
        self.effects: List[PostProcessEffect] = []
        self.grid_size: int = 30
    
    def setup(self, config: PostProcessConfig):
        self.effects.clear()
        self.grid_size = config.grid_size
        
        for effect_name, intensity in config.effects:
            # Пропускаем эффекты с очень низкой интенсивностью
            if intensity <= 0.01:
                continue
                
            effect = EffectFactory.create_effect(effect_name, intensity)
            if effect:
                if isinstance(effect, SeamSmoothingEffect):
                    effect.set_grid_size(self.grid_size)
                self.effects.append(effect)
    
    def process(self, mosaic: np.ndarray, original: np.ndarray) -> np.ndarray:
        result = mosaic.copy()
        
        for effect in self.effects:
            try:
                result = effect.apply(result, original)
            except Exception as e:
                print(f"Error applying effect {effect.get_name()}: {e}")
                # В случае ошибки продолжаем с предыдущим результатом
                continue
        
        return result