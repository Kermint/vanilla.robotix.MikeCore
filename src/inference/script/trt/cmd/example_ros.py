#!/usr/bin/env python3
import sys
sys.path.append("./")

from src.inference.script.preprocess import *

import cv2
from src.inference.script.trt.model_ros_nms import TrtModel
import src.inference.script.trt.config as config
import rospy

import inference.msg._Bboxes as Bboxes
import inference.msg._Bbox as Bbox

from sensor_msgs.msg import Image
from cv_bridge import CvBridge


LABELS = ["background", "bottle", "soup"]
SHOW_IMAGE = True
model = TrtModel(model=config.model_ssd_inception_v2_coco_2017_11_17, labels=LABELS)
obj_publisher = None
prep = ResizePreprocessor(300, 300)
copy = None
cap = cv2.VideoCapture(0)

def step():
    # read frame from the camera
    ret, image = cap.read()
    rospy.loginfo("[INFO] receive image from the mike_camera/raw")

    # if show image - create copy
    if SHOW_IMAGE:
        copy = prep.preprocess(image.copy())

    # predict and publish predicted objects
    objs = model.predict_bboxes(image)
    obj_publisher.publish(objs)

    # show image with bounding boxes if needed
    if SHOW_IMAGE:
        for bbox in objs.bboxes:
            cv2.rectangle(copy, (bbox.x_min, bbox.y_min), (bbox.x_max, bbox.y_max), (172, 217, 153), 2)
            cv2.putText(copy, bbox.label, (bbox.x_min + 10, bbox.y_min + 10), cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 255, 255), 2, cv2.LINE_AA)
        cv2.imshow("Output", copy)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            return


if __name__ == '__main__':
    rospy.init_node('mike_inference', anonymous=True)
    obj_publisher = rospy.Publisher('/bboxes', Bboxes.Bboxes, queue_size=10)

    r = rospy.Rate(10)

    while not rospy.is_shutdown():
        rospy.loginfo("[INFO] camera step...")
        step()
        r.sleep()
