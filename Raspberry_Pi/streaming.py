import cv2
import pygame
import os
import math
import time
import threading
import numpy as np

from Adafruit_AMG88xx import Adafruit_AMG88xx
from flask import Flask, Response, render_template
from scipy.interpolate import griddata
from colour import Color

app = Flask(__name__)

# 열화상 센서 초기화
MINTEMP = 26
MAXTEMP = 32
COLORDEPTH = 1024
width = 240  # 원하는 이미지 너비로 수정
height = 240  # 원하는 이미지 높이로 수정

sensor = Adafruit_AMG88xx()

# 열화상 데이터 픽셀 좌표 생성
points = [(math.floor(ix / 8), (ix % 8)) for ix in range(0, 64)]
grid_x, grid_y = np.mgrid[0:7:32j, 0:7:32j]

# 열화상 데이터의 온도에 따라 색상 설정
blue = Color("indigo")
colors = list(blue.range_to(Color("red"), COLORDEPTH))
colors = [(int(c.red * 255), int(c.green * 255), int(c.blue * 255)) for c in colors]

displayPixelWidth = int(width / 30)
displayPixelHeight = int(height / 30)

# 'lcd' 서피스 초기화
pygame.init()
lcd = pygame.Surface((width, height))
lcd.fill((0, 0, 0))
pygame.display.set_mode((width, height))
pygame.display.update()

def thermal_camera_thread():
    while True:
        # 열화상 데이터 읽기
        pixels = sensor.readPixels()
        # 온도 값을 색상으로 변환하여 'lcd' 서피스에 그리기
        pixels = [map_pixel_value(p, MINTEMP, MAXTEMP, 0, COLORDEPTH - 1) for p in pixels]
        bicubic = griddata(points, pixels, (grid_x, grid_y), method='cubic')
        pygame.surfarray.blit_array(lcd, np.array([[colors[clamp(int(pixel), 0, COLORDEPTH - 1)] for pixel in row] for row in bicubic]))
        pygame.display.update()

# Pi Camera 객체 생성
cap_pi = cv2.VideoCapture()
cap_pi.open("libcamerasrc ! video/x-raw, width=1280, height=720, framerate=30/1 ! videoconvert ! videoscale ! video/x-raw, width=480, height=480 ! appsink")  # Pi Camera 설정에 맞게 수정

def generate_frames_pi():
    while True:
        ret, frame = cap_pi.read()
        if not ret:
            break
        else:
            # Pi Camera로부터 읽은 프레임을 JPEG 형식으로 인코딩하여 전송
            frame = cv2.flip(frame, 0)
            ret, buffer = cv2.imencode('.jpg', frame)
            frame = buffer.tobytes()
            yield (b'--frame\r\n'
                   b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n')

def generate_frames_thermal():
    while True:
        # 열화상 데이터 읽기
        pixels = sensor.readPixels()
        # 데이터 처리를 통해 프레임 생성
        frame = process_thermal_data(pixels)  # 데이터 처리 로직에 따라 수정
        # 프레임을 JPEG 형식으로 인코딩하여 전송
        ret, buffer = cv2.imencode('.jpg', frame)
        frame = buffer.tobytes()
        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n')

def process_thermal_data(pixels):
    # 열화상 데이터 처리 및 이미지 프레임 생성
    # 픽셀 데이터를 2D 배열로 재구성
    thermal_image = np.array(pixels).reshape(8, 8)
    
    # 픽셀 값을 0에서 255 범위로 정규화
    normalized_image = cv2.normalize(thermal_image, None, 0, 255, cv2.NORM_MINMAX)
    
    # 8-bit 부호 없는 정수로 변환
    normalized_image = np.uint8(normalized_image)
    
    # 컬러 맵 적용 (낮은 온도는 빨강에서 초록으로)
    processed_frame = cv2.applyColorMap(normalized_image, cv2.COLORMAP_JET)
    
    # 이미지를 시계 방향으로 90도 회전
    processed_frame = cv2.rotate(processed_frame, cv2.ROTATE_90_CLOCKWISE)
    
    return processed_frame

def map_pixel_value(p, in_min, in_max, out_min, out_max):
    return (p - in_min) * (out_max - out_min) / (in_max - in_min) + out_min
    
def clamp(n, min_val, max_val):
    return max(min(n, max_val), min_val)


# YOLO에서 나온 이미지 프레임을 읽어올 경로
#image_url = '192.168.0.110/get_image'
image_url = '10.10.21.30/get_image'

def generate_frames_yolo():
    while True:
        response = requests.get(image.url)
        if respons.status_code == 200:
            with open(image_url, 'wb') as image_file:
                # 경로에서 이미지를 읽어옴
                image_file.write(response.content)
        
        # 프레임을 JPEG 형식으로 인코딩하여 전송
        ret, buffer = cv2.imencode('.jpg', frame)
        frame = buffer.tobytes()
        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n')


# Flask 루트 경로 및 라우팅 설정
@app.route('/')
def index():
    return render_template('index.html')

@app.route('/video_feed_pi')
def video_feed_pi():
    return Response(generate_frames_pi(), mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/thermal_feed_ir')
def thermal_feed():
    return Response(generate_frames_thermal(), mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/object_detection_yolo')
def object_detection_yolo():
    return Response(generate_frames_yolo(), mimetype='multipart/x-mixed-replace; boundary=frame')

# 열화상 스레드 시작
thermal_thread = threading.Thread(target=thermal_camera_thread)
thermal_thread.start()

# Flask 애플리케이션 실행
if __name__ == '__main__':
    app.run(host='169.254.1.2', port=9000)
    #app.run(host='192.168.137.248',port=9000)
    #app.run(host='192.168.189.36', port=9000)
    #app.run(host='172.21.218.133', port=9000)
    #app.run(host='169.254.1.2', port=9000)
    #app.run(host='192.168.0.143',port=9000)
    #app.run(host='192.168.189.36',port=9000)
    #app.run(host='192.168.200.158', port=9000)