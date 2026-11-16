import cv2

# Taking saved video as input
# vid = cv2.VideoCapture("./Resources/video.mp4")

# Taking webcam video as input live
default_webcam = cv2.VideoCapture(0)

# Taking connected camera as input live
# stereo_cam = cv2.VideoCapture(1)

while (True):
    success, img_captured = default_webcam.read()
    if (success):
        # different processing options
        # img_gray = cv2.cvtColor(img_captured, cv2.COLOR_BGR2GRAY)
        # img_blur = cv2.GaussianBlur(img_gray, (9, 9), 1.2)
        # img_canny = cv2.Canny(img_blur, 50, 50)
        cv2.imshow("Video", img_captured)
    else:
        print("Issue occurred while reading!")
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

# Releasing resources after use
default_webcam.release()
cv2.destroyAllWindows()