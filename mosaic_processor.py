import cv2
import numpy as np
import os
from typing import List, Tuple, Optional
import math

class FeatureUtils:
    @staticmethod
    def compute_std_dev(image: np.ndarray) -> Tuple[float, ...]:
        """Вычисляет стандартное отклонение (контрастность) изображения по каналам"""
        if image.size == 0:
            return (0.0,) * image.shape[2] if len(image.shape) == 3 else (0.0,)
        
        mean, stddev = cv2.meanStdDev(image)
        
        if len(image.shape) == 3 and image.shape[2] == 3:
            return (float(stddev[0, 0]), float(stddev[1, 0]), float(stddev[2, 0]))
        return (float(stddev[0, 0]),)

    @staticmethod
    def compute_gradient_hist(image: np.ndarray) -> np.ndarray:
        """Вычисляет гистограмму направлений градиентов (HOG-подобный признак)"""
        if image.size == 0 or image.shape[0] < 3 or image.shape[1] < 3:
            return np.zeros((36, 1), dtype=np.float32)
        
        # Преобразование в оттенки серого
        if len(image.shape) == 3 and image.shape[2] == 3:
            gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
        else:
            gray = image
        
        gray = gray.astype(np.float32)
        
        # Вычисление градиентов
        grad_x = cv2.Sobel(gray, cv2.CV_32F, 1, 0, ksize=3)
        grad_y = cv2.Sobel(gray, cv2.CV_32F, 0, 1, ksize=3)
        
        # Преобразование в полярные координаты
        magnitude, angle = cv2.cartToPolar(grad_x, grad_y, angleInDegrees=True)
        
        # Создание гистограммы
        hist_size = 36
        hist = np.zeros((hist_size, 1), dtype=np.float32)
        angle_step = 360.0 / hist_size
        
        for y in range(angle.shape[0]):
            for x in range(angle.shape[1]):
                angle_val = angle[y, x]
                mag_val = magnitude[y, x]
                bin_idx = int(angle_val / angle_step)
                bin_idx = max(0, min(bin_idx, hist_size - 1))
                hist[bin_idx] += mag_val
        
        # Нормализация
        cv2.normalize(hist, hist, 1.0, 0.0, cv2.NORM_L1)
        return hist.astype(np.float32)

    @staticmethod
    def get_lbp_value(gray: np.ndarray, r: int, c: int) -> int:
        """Вычисляет LBP-значение для пикселя"""
        center = gray[r, c]
        value = 0
        
        # Сравнение с 8 соседями
        neighbors = [
            (r-1, c-1), (r-1, c), (r-1, c+1),
            (r, c+1), (r+1, c+1), (r+1, c),
            (r+1, c-1), (r, c-1)
        ]
        
        for i, (nr, nc) in enumerate(neighbors):
            if gray[nr, nc] > center:
                value |= (1 << (7 - i))
        
        return value

    @staticmethod
    def compute_lbp_features(image: np.ndarray) -> np.ndarray:
        """Вычисляет LBP-признаки для описания текстуры изображения"""
        if image.size == 0 or image.shape[0] < 3 or image.shape[1] < 3:
            return np.zeros((256, 1), dtype=np.float32)
        
        # Преобразование в оттенки серого
        if len(image.shape) == 3 and image.shape[2] == 3:
            gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
        else:
            gray = image
        
        # Создание LBP-карты
        lbp = np.zeros((gray.shape[0]-2, gray.shape[1]-2), dtype=np.uint8)
        
        for i in range(1, gray.shape[0]-1):
            for j in range(1, gray.shape[1]-1):
                lbp[i-1, j-1] = FeatureUtils.get_lbp_value(gray, i, j)
        
        # Вычисление гистограммы
        hist = cv2.calcHist([lbp], [0], None, [256], [0, 256])
        
        # Нормализация
        cv2.normalize(hist, hist, 1.0, 0.0, cv2.NORM_L1)
        return hist.astype(np.float32)


class Tile:
    def __init__(self):
        self.image: Optional[np.ndarray] = None
        self.original_image: Optional[np.ndarray] = None  # Добавляем хранение оригинала
        self.color: Optional[Tuple[float, ...]] = None
        self.stddev: Optional[Tuple[float, ...]] = None
        self.gradient_hist: Optional[np.ndarray] = None
        self.texture_features: Optional[np.ndarray] = None
        self.usage: int = 0
        self.angle: int = 0
        self.original_index: int = -1

class Config:
    def __init__(self):
        self.tile_size: int = 30
        self.grid_step: int = 30
        self.max_repeats: int = 1000000
        self.repeats: bool = False
        self.rotation: bool = False
        self.rotation_angle: int = 0
        self.metric: str = "color"


class IMetric:
    def compute_cell_features(self, cell: Tile, cell_image: np.ndarray):
        raise NotImplementedError
    
    def compute_tile_features(self, tile: Tile, tile_image: np.ndarray):
        raise NotImplementedError
    
    def distance(self, cell: Tile, tile: Tile) -> float:
        raise NotImplementedError
    
    def get_name(self) -> str:
        raise NotImplementedError


class ColorMetric(IMetric):
    def compute_cell_features(self, cell: Tile, cell_image: np.ndarray):
        cell.color = cv2.mean(cell_image)[:3]  # BGR mean
    
    def compute_tile_features(self, tile: Tile, tile_image: np.ndarray):
        tile.color = cv2.mean(tile_image)[:3]
    
    def distance(self, cell: Tile, tile: Tile) -> float:
        return np.linalg.norm(np.array(cell.color) - np.array(tile.color))
    
    def get_name(self) -> str:
        return "color"


class ColorContrastMetric(IMetric):
    def compute_cell_features(self, cell: Tile, cell_image: np.ndarray):
        cell.color = cv2.mean(cell_image)[:3]
        cell.stddev = FeatureUtils.compute_std_dev(cell_image)
    
    def compute_tile_features(self, tile: Tile, tile_image: np.ndarray):
        tile.color = cv2.mean(tile_image)[:3]
        tile.stddev = FeatureUtils.compute_std_dev(tile_image)
    
    def distance(self, cell: Tile, tile: Tile) -> float:
        color_dist = np.linalg.norm(np.array(cell.color) - np.array(tile.color))
        stddev_dist = np.linalg.norm(np.array(cell.stddev) - np.array(tile.stddev))
        return color_dist + 2.0 * stddev_dist
    
    def get_name(self) -> str:
        return "color_contrast"


class GradientMetric(IMetric):
    def compute_cell_features(self, cell: Tile, cell_image: np.ndarray):
        cell.gradient_hist = FeatureUtils.compute_gradient_hist(cell_image)
    
    def compute_tile_features(self, tile: Tile, tile_image: np.ndarray):
        tile.gradient_hist = FeatureUtils.compute_gradient_hist(tile_image)
    
    def distance(self, cell: Tile, tile: Tile) -> float:
        if (cell.gradient_hist is None or tile.gradient_hist is None or 
            cell.gradient_hist.size == 0 or tile.gradient_hist.size == 0):
            return float('inf')
        
        hist_dist = cv2.compareHist(cell.gradient_hist, tile.gradient_hist, cv2.HISTCMP_BHATTACHARYYA)
        return hist_dist * 1000.0
    
    def get_name(self) -> str:
        return "gradient"


class TextureMetric(IMetric):
    def compute_cell_features(self, cell: Tile, cell_image: np.ndarray):
        cell.texture_features = FeatureUtils.compute_lbp_features(cell_image)
    
    def compute_tile_features(self, tile: Tile, tile_image: np.ndarray):
        tile.texture_features = FeatureUtils.compute_lbp_features(tile_image)
    
    def distance(self, cell: Tile, tile: Tile) -> float:
        if (cell.texture_features is None or tile.texture_features is None or
            cell.texture_features.size == 0 or tile.texture_features.size == 0 or
            cell.texture_features.shape != tile.texture_features.shape):
            return float('inf')
        
        hist_dist = cv2.compareHist(cell.texture_features, tile.texture_features, cv2.HISTCMP_BHATTACHARYYA)
        return hist_dist * 1000.0
    
    def get_name(self) -> str:
        return "texture"


class MosaicGenerator:
    def __init__(self):
        self.tiles: List[Tile] = []
        self.metric: Optional[IMetric] = None
        self.post_processor = None  # Будет установлен позже
    
    def set_metric(self, metric_name: str) -> bool:
        metrics = {
            "color": ColorMetric,
            "color_contrast": ColorContrastMetric,
            "gradient": GradientMetric,
            "texture": TextureMetric
        }
        
        if metric_name not in metrics:
            return False
        
        self.metric = metrics[metric_name]()
        
        # Пересчитываем параметры для всех загруженных тайлов
        for tile in self.tiles:
            self._compute_tile_features(tile, tile.image)
        
        return True
    
    def _compute_tile_features(self, tile: Tile, image: np.ndarray):
        if self.metric and image is not None:
            self.metric.compute_tile_features(tile, image)
    
    def load_tiles(self, folder_path: str, size: int, enable_rotation: bool = False, rotation_angle: int = 0) -> bool:
        if not self.metric:
            self.set_metric("color")
        
        self.tiles.clear()
        original_index = 0
        
        try:
            for filename in os.listdir(folder_path):
                file_path = os.path.join(folder_path, filename)
                if os.path.isfile(file_path) and any(filename.lower().endswith(ext) for ext in ['.jpg', '.jpeg', '.png', '.bmp']):
                    original_tile = cv2.imread(file_path)
                    
                    if original_tile is not None:
                        # Изменение размера тайла
                        resized_tile = cv2.resize(original_tile, (size, size))
                        
                        # Определение углов поворота
                        angles = [rotation_angle] if enable_rotation else [0]
                        
                        for angle in angles:
                            if angle != 0:
                                center = (size / 2, size / 2)
                                rot_mat = cv2.getRotationMatrix2D(center, angle, 1.0)
                                rotated_tile = cv2.warpAffine(resized_tile, rot_mat, (size, size),
                                                            flags=cv2.INTER_LINEAR,
                                                            borderMode=cv2.BORDER_CONSTANT,
                                                            borderValue=(0, 0, 0))
                            else:
                                rotated_tile = resized_tile.copy()
                            
                            new_tile = Tile()
                            new_tile.image = rotated_tile
                            new_tile.angle = angle
                            new_tile.original_index = original_index
                            
                            self._compute_tile_features(new_tile, new_tile.image)
                            self.tiles.append(new_tile)
                        
                        original_index += 1
        except Exception as e:
            print(f"Error loading tiles: {e}")
            return False
        
        return len(self.tiles) > 0
    
    def load_tiles_optimized(self, folder_path: str, size: int, enable_rotation: bool = False, rotation_angle: int = 0) -> bool:
        """Оптимизированная загрузка тайлов для большого количества файлов"""
        if not self.metric:
            self.set_metric("color")
        
        self.tiles.clear()
        original_index = 0
        
        try:
            # Получаем список файлов
            image_files = [f for f in os.listdir(folder_path) 
                          if any(f.lower().endswith(ext) for ext in ['.jpg', '.jpeg', '.png', '.bmp'])]
            
            print(f"Found {len(image_files)} image files")
            
            for filename in image_files:
                file_path = os.path.join(folder_path, filename)
                
                try:
                    original_tile = cv2.imread(file_path)
                    
                    if original_tile is not None:
                        # Сохраняем оригинальное изображение
                        new_tile = Tile()
                        new_tile.original_image = original_tile.copy()
                        new_tile.image = cv2.resize(original_tile, (size, size))
                        new_tile.angle = 0  # По умолчанию без поворота
                        new_tile.original_index = original_index
                        
                        self._compute_tile_features(new_tile, new_tile.image)
                        self.tiles.append(new_tile)
                        
                        original_index += 1
                        
                        # Периодически выводим прогресс
                        if original_index % 500 == 0:
                            print(f"Processed {original_index} tiles...")
                            
                except Exception as e:
                    print(f"Error processing tile {filename}: {e}")
                    continue
                    
        except Exception as e:
            print(f"Error loading tiles: {e}")
            return False
        
        print(f"Successfully loaded {len(self.tiles)} tiles")
        return len(self.tiles) > 0
    
    def _apply_rotation(self, image: np.ndarray, angle: int, size: int) -> np.ndarray:
        """Применяет поворот к изображению"""
        if angle == 0:
            return image
        
        center = (size / 2, size / 2)
        rot_mat = cv2.getRotationMatrix2D(center, angle, 1.0)
        rotated_tile = cv2.warpAffine(image, rot_mat, (size, size),
                                    flags=cv2.INTER_LINEAR,
                                    borderMode=cv2.BORDER_CONSTANT,
                                    borderValue=(0, 0, 0))
        return rotated_tile
    
    def _create_raw_mosaic(self, source: np.ndarray, cfg: Config) -> np.ndarray:
        if not self.metric:
            self.set_metric("color")
        
        target_height, target_width = source.shape[:2]
        raw_mosaic = np.zeros((target_height, target_width, 3), dtype=np.uint8)
        
        # Определяем угол поворота
        rotation_angle = cfg.rotation_angle if cfg.rotation else 0
        
        for y in range(0, target_height, cfg.grid_step):
            for x in range(0, target_width, cfg.grid_step):
                current_cell = Tile()
                
                block_width = min(cfg.grid_step, target_width - x)
                block_height = min(cfg.grid_step, target_height - y)
                
                region = source[y:y+block_height, x:x+block_width]
                
                # Вычисление признаков для текущей клетки
                self.metric.compute_cell_features(current_cell, region)
                
                # Поиск наилучшего тайла
                best_index = -1
                best_distance = float('inf')
                
                for i, tile in enumerate(self.tiles):
                    if tile.usage < cfg.max_repeats:
                        dist = self.metric.distance(current_cell, tile)
                        if dist < best_distance:
                            best_distance = dist
                            best_index = i
                
                # Если не найден подходящий тайл, используем средний цвет
                if best_index == -1:
                    color_block = np.full((block_height, block_width, 3), cv2.mean(region)[:3], dtype=np.uint8)
                    raw_mosaic[y:y+block_height, x:x+block_width] = color_block
                    continue
                
                # Увеличиваем счетчик использования
                self.tiles[best_index].usage += 1
                
                # Подготавливаем тайл с учетом поворота
                best_tile = self.tiles[best_index]
                
                # Если требуется поворот, создаем повернутую версию
                if rotation_angle != 0 and best_tile.original_image is not None:
                    # Используем оригинальное изображение для поворота
                    resized_rotated = self._apply_rotation(
                        cv2.resize(best_tile.original_image, (cfg.tile_size, cfg.tile_size)), 
                        rotation_angle, 
                        cfg.tile_size
                    )
                    final_tile = cv2.resize(resized_rotated, (block_width, block_height), interpolation=cv2.INTER_CUBIC)
                else:
                    # Используем уже подготовленный тайл
                    final_tile = cv2.resize(best_tile.image, (block_width, block_height), interpolation=cv2.INTER_CUBIC)
                
                raw_mosaic[y:y+block_height, x:x+block_width] = final_tile
        
        return raw_mosaic
    
    def create_mosaic(self, source: np.ndarray, cfg: Config) -> np.ndarray:
        if not self.tiles:
            raise ValueError("No tiles loaded")
        
        if not self.set_metric(cfg.metric):
            raise ValueError(f"Invalid metric name specified: {cfg.metric}")
        
        # Сброс счетчиков использования
        for tile in self.tiles:
            tile.usage = 0
        
        # Создание мозаики
        raw_mosaic = self._create_raw_mosaic(source, cfg)
        
        # Применение постобработки
        if self.post_processor:
            return self.post_processor.process(raw_mosaic, source)
        
        return raw_mosaic
    
    def get_tiles_count(self) -> int:
        return len(self.tiles)
    
    def clear_tiles(self):
        self.tiles.clear()
    
    def set_post_process_config(self, config):
        if self.post_processor:
            self.post_processor.setup(config)