from flask import Flask, request, jsonify, send_file, render_template
import cv2
import numpy as np
import os
import io
import base64
import gc
import tempfile
import shutil
from mosaic_processor import MosaicGenerator, Config
from post_processor import PostProcessConfig, PostProcessPipeline
import json
import time
import threading

app = Flask(__name__, static_folder='static', static_url_path='')
app.config['UPLOAD_FOLDER'] = 'uploads'
app.config['MAX_CONTENT_LENGTH'] = 500 * 1024 * 1024  # 500MB
app.config['JSONIFY_PRETTYPRINT_REGULAR'] = False

# Глобальные переменные для хранения состояния
mosaic_generator = MosaicGenerator()
post_processor = PostProcessPipeline()
mosaic_generator.post_processor = post_processor

current_config = Config()
current_post_config = PostProcessConfig()
last_mosaic_result = None

# Глобальные переменные для батчевой загрузки
upload_lock = threading.Lock()
current_upload_session = None
uploaded_tiles_count = 0

class UploadSession:
    def __init__(self):
        self.temp_dir = tempfile.mkdtemp()
        self.processed_tiles = 0
        self.total_batches = 0
        self.tile_size = 30
        self.enable_rotation = False
        self.rotation_angle = 0

    def cleanup(self):
        if os.path.exists(self.temp_dir):
            shutil.rmtree(self.temp_dir)

def get_file_size_mb(file_path):
    """Получение размера файла в МБ"""
    return os.path.getsize(file_path) / (1024 * 1024)

@app.route('/')
def index():
    return app.send_static_file('index.html')

@app.route('/api/upload-tiles-batch', methods=['POST'])
def upload_tiles_batch():
    """Загрузка батча тайлов"""
    global current_upload_session, uploaded_tiles_count
    
    try:
        if 'tiles' not in request.files:
            return jsonify({'error': 'No tiles provided'}), 400
        
        tiles = request.files.getlist('tiles')
        if not tiles:
            return jsonify({'error': 'No tiles in batch'}), 400
        
        batch_index = int(request.form.get('batchIndex', 0))
        total_batches = int(request.form.get('totalBatches', 1))
        tile_size = int(request.form.get('tileSize', 30))
        enable_rotation = request.form.get('enableRotation', 'false') == 'true'
        rotation_angle = int(request.form.get('rotationAngle', 0))
        
        print(f"Processing batch {batch_index + 1}/{total_batches} with {len(tiles)} tiles")
        
        with upload_lock:
            if current_upload_session is None:
                current_upload_session = UploadSession()
                current_upload_session.tile_size = tile_size
                current_upload_session.total_batches = total_batches
                current_upload_session.enable_rotation = enable_rotation
                current_upload_session.rotation_angle = rotation_angle
            
            # Сохранение файлов батча
            batch_saved = 0
            for tile in tiles:
                if tile.filename:
                    try:
                        file_path = os.path.join(current_upload_session.temp_dir, tile.filename)
                        tile.save(file_path)
                        batch_saved += 1
                    except Exception as e:
                        print(f"Error saving tile {tile.filename}: {e}")
                        continue
            
            current_upload_session.processed_tiles += batch_saved
            
            print(f"Batch {batch_index + 1} saved {batch_saved} tiles, total: {current_upload_session.processed_tiles}")
            
            return jsonify({
                'success': True,
                'message': f'Batch {batch_index + 1} processed successfully',
                'processedInBatch': batch_saved
            })
            
    except Exception as e:
        print(f"Error in upload_tiles_batch: {str(e)}")
        return jsonify({'error': str(e)}), 500

@app.route('/api/finalize-upload', methods=['POST'])
def finalize_upload():
    """Завершение загрузки и обработка всех тайлов"""
    global current_upload_session, uploaded_tiles_count, mosaic_generator
    
    try:
        with upload_lock:
            if current_upload_session is None:
                return jsonify({'error': 'No upload session active'}), 400
            
            data = request.get_json()
            total_files = data.get('totalFiles', 0)
            enable_rotation = data.get('enableRotation', False)
            rotation_angle = data.get('rotationAngle', 0)
            
            print(f"Finalizing upload with {total_files} files from temp directory")
            print(f"Rotation enabled: {enable_rotation}, angle: {rotation_angle}")
            
            # Загрузка тайлов в генератор
            start_time = time.time()
            success = mosaic_generator.load_tiles_optimized(
                current_upload_session.temp_dir, 
                current_upload_session.tile_size, 
                enable_rotation, 
                rotation_angle
            )
            load_time = time.time() - start_time
            
            # Очистка временной директории
            current_upload_session.cleanup()
            current_upload_session = None
            
            if success:
                tiles_count = mosaic_generator.get_tiles_count()
                uploaded_tiles_count = tiles_count
                
                print(f"Successfully loaded {tiles_count} tiles in {load_time:.2f} seconds")
                
                return jsonify({
                    'success': True,
                    'tilesCount': tiles_count,
                    'message': f'Successfully loaded {tiles_count} tiles in {load_time:.2f} seconds',
                    'loadTime': load_time
                })
            else:
                return jsonify({'error': 'Failed to load tiles into generator'}), 500
                
    except Exception as e:
        print(f"Error in finalize_upload: {str(e)}")
        if current_upload_session:
            current_upload_session.cleanup()
            current_upload_session = None
        return jsonify({'error': str(e)}), 500

@app.route('/api/generate-mosaic', methods=['POST'])
def generate_mosaic():
    """Генерация мозаики"""
    global last_mosaic_result
    
    try:
        if 'image' not in request.files:
            return jsonify({'error': 'No image provided'}), 400
        
        image_file = request.files['image']
        if image_file.filename == '':
            return jsonify({'error': 'No image selected'}), 400
        
        # Проверяем, есть ли тайлы
        if mosaic_generator.get_tiles_count() == 0:
            return jsonify({'error': 'No tiles loaded. Please upload tiles first.'}), 400
        
        # Чтение и обработка изображения
        image_data = image_file.read()
        nparr = np.frombuffer(image_data, np.uint8)
        source_image = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
        
        if source_image is None:
            return jsonify({'error': 'Failed to decode image'}), 400
        
        # Обновление конфигурации мозаики
        current_config.tile_size = int(request.form.get('tileSize', 30))
        current_config.grid_step = int(request.form.get('gridStep', 30))
        current_config.max_repeats = int(request.form.get('maxRepeats', 1000000))
        current_config.repeats = request.form.get('repeats', 'false') == 'true'
        current_config.rotation = request.form.get('rotation', 'false') == 'true'
        current_config.rotation_angle = int(request.form.get('rotationAngle', 0))
        current_config.metric = request.form.get('metric', 'color')
        
        # Обновление конфигурации постобработки
        current_post_config.clear_effects()
        current_post_config.grid_size = current_config.grid_step
        
        # Добавляем эффекты если они включены и интенсивность > 0
        if request.form.get('alphaBlend') == 'true':
            intensity = float(request.form.get('alphaBlendIntensity', 0.5))
            if intensity > 0.01:  # Добавляем проверку на минимальную интенсивность
                current_post_config.add_effect('alpha_blend', intensity)
        
        if request.form.get('colorCorrection') == 'true':
            intensity = float(request.form.get('colorCorrectionIntensity', 0.5))
            if intensity > 0.01:  # Добавляем проверку на минимальную интенсивность
                current_post_config.add_effect('color_correction', intensity)
        
        if request.form.get('seamSmoothing') == 'true':
            intensity = float(request.form.get('seamSmoothingIntensity', 0.5))
            if intensity > 0.01:  # Добавляем проверку на минимальную интенсивность
                current_post_config.add_effect('seam_smoothing', intensity)
        
        # Настраиваем постпроцессор
        post_processor.setup(current_post_config)
        
        print(f"Generating mosaic with config: {current_config.__dict__}")
        print(f"Post-processing effects: {current_post_config.effects}")
        print(f"Source image size: {source_image.shape}")
        print(f"Tiles available: {mosaic_generator.get_tiles_count()}")
        
        # Генерация мозаики
        print("Starting mosaic generation...")
        start_time = time.time()
        
        result_image = mosaic_generator.create_mosaic(source_image, current_config)
        last_mosaic_result = result_image.copy()
        
        generation_time = time.time() - start_time
        print(f"Mosaic generation completed in {generation_time:.2f} seconds")
        
        # Кодирование результата в base64
        _, buffer = cv2.imencode('.png', result_image)
        mosaic_base64 = base64.b64encode(buffer).decode('utf-8')
        
        return jsonify({
            'success': True,
            'mosaic': f'data:image/png;base64,{mosaic_base64}',
            'message': f'Mosaic generated successfully in {generation_time:.2f} seconds'
        })
        
    except Exception as e:
        print(f"Error in generate_mosaic: {str(e)}")
        import traceback
        traceback.print_exc()
        return jsonify({'error': str(e)}), 500

@app.route('/api/download-mosaic', methods=['POST'])
def download_mosaic():
    """Скачивание мозаики"""
    global last_mosaic_result
    
    try:
        if last_mosaic_result is None:
            return jsonify({'error': 'No mosaic generated yet'}), 400
        
        data = request.get_json()
        format_type = data.get('format', 'png')
        resolution = data.get('resolution', 'original')
        
        result_image = last_mosaic_result.copy()
        
        # Изменение размера если нужно
        if resolution != 'original' and 'x' in resolution:
            width, height = map(int, resolution.split('x'))
            result_image = cv2.resize(result_image, (width, height))
        
        # Кодирование в выбранный формат
        if format_type in ['jpg', 'jpeg']:
            mime_type = 'image/jpeg'
            _, buffer = cv2.imencode('.jpg', result_image, [cv2.IMWRITE_JPEG_QUALITY, 95])
        elif format_type == 'webp':
            mime_type = 'image/webp'
            _, buffer = cv2.imencode('.webp', result_image, [cv2.IMWRITE_WEBP_QUALITY, 95])
        elif format_type == 'bmp':
            mime_type = 'image/bmp'
            _, buffer = cv2.imencode('.bmp', result_image)
        elif format_type == 'tiff':
            mime_type = 'image/tiff'
            _, buffer = cv2.imencode('.tiff', result_image)
        else:  # PNG по умолчанию
            mime_type = 'image/png'
            _, buffer = cv2.imencode('.png', result_image)
        
        # Создание data URL
        data_url = f'data:{mime_type};base64,{base64.b64encode(buffer).decode()}'
        
        return jsonify({
            'success': True,
            'dataUrl': data_url,
            'mimeType': mime_type
        })
        
    except Exception as e:
        print(f"Error in download_mosaic: {str(e)}")
        return jsonify({'error': str(e)}), 500

@app.route('/api/tiles-count', methods=['GET'])
def get_tiles_count():
    """Получение количества загруженных тайлов"""
    count = mosaic_generator.get_tiles_count()
    return jsonify({
        'count': count,
        'message': f'Loaded {count} tiles'
    })

@app.route('/api/clear-tiles', methods=['POST'])
def clear_tiles():
    """Очистка загруженных тайлов"""
    global uploaded_tiles_count
    mosaic_generator.clear_tiles()
    uploaded_tiles_count = 0
    gc.collect()
    return jsonify({'success': True, 'message': 'Tiles cleared'})

@app.route('/api/set-metric', methods=['POST'])
def set_metric():
    """Установка метрики сравнения"""
    data = request.get_json()
    metric_name = data.get('metric', 'color')
    
    success = mosaic_generator.set_metric(metric_name)
    
    if success:
        return jsonify({'success': True, 'message': f'Metric set to {metric_name}'})
    else:
        return jsonify({'error': f'Invalid metric: {metric_name}'}), 400

@app.errorhandler(413)
def too_large(e):
    return jsonify({'error': 'File too large. Maximum size is 500MB.'}), 413

if __name__ == '__main__':
    # Создание папок для загрузок
    os.makedirs(app.config['UPLOAD_FOLDER'], exist_ok=True)
    
    print("Starting Mosaic Generator server...")
    print(f"Upload folder: {app.config['UPLOAD_FOLDER']}")
    print(f"Max upload size: {app.config['MAX_CONTENT_LENGTH'] / (1024*1024)} MB")
    print("Server will be available at: http://localhost:5000")
    
    app.run(host='localhost', port=5000, debug=True, threaded=True)