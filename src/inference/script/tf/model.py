from keras.backend.tensorflow_backend import set_session
import tensorflow as tf
import numpy as np
import cv2

from src.inference.script.predict.data_process.minthresh import MinThreshold
from src.inference.script.predict.data_process.nms import NonMaximumSuppression


class TFModel:
    def __init__(self, model_path):
        # configure session
        self.__gpu_allow_growth()
        # initialize the model
        self.model = tf.Graph()
        self.__load(model_path)
        self.__init_tensors()

    # returns zipped (box, score, label_id)'s
    def predict(self, image, data_processors=None):
        # init processors
        if data_processors is None:
            data_processors = [MinThreshold, NonMaximumSuppression]
        image = cv2.cvtColor(image.copy(), cv2.COLOR_BGR2RGB)
        image = np.expand_dims(image, axis=0)
        # preform inference and compute the bounding boxes, probabilities and class labels
        (bboxes, scores, labels, N) = self.sess.run([
            self.boxes_tensor,
            self.scores_tensor,
            self.classes_tensor,
            self.num_detections],
            feed_dict={self.image_tensor: image})
        # squeeze the lists into a single dimension
        bboxes = np.squeeze(bboxes)
        scores = np.squeeze(scores)
        labels = np.squeeze(labels)
        # apply data processors
        for p in data_processors:
            bboxes, scores, labels = p.process(bboxes, scores, labels)
        # return stuff
        return bboxes, scores, labels

    def __load(self, path):
        # create a context manager that makes this model the default one for execution
        with self.model.as_default():
            # initialize the graph definition
            graph_def = tf.compat.v1.GraphDef()
            file = tf.io.gfile.GFile(path, "rb")
            # load the graph from disk
            serialized_graph = file.read()
            graph_def.ParseFromString(serialized_graph)
            tf.import_graph_def(graph_def, name="")
            # create a session to perform inference
            self.sess = tf.compat.v1.Session(graph=self.model)

    def __init_tensors(self):
        # grab a reference to the input image tensor and the boxes tensor
        self.image_tensor = self.model.get_tensor_by_name("image_tensor:0")
        self.boxes_tensor = self.model.get_tensor_by_name("detection_boxes:0")
        # for each bounding box we would like to know the score (i.e. probability) and class label
        self.scores_tensor = self.model.get_tensor_by_name("detection_scores:0")
        self.classes_tensor = self.model.get_tensor_by_name("detection_classes:0")
        self.num_detections = self.model.get_tensor_by_name("num_detections:0")

    @staticmethod
    def __gpu_allow_growth():
        config = tf.compat.v1.ConfigProto()
        config.gpu_options.allow_growth = True
        sess = tf.compat.v1.Session(config=config)
        set_session(sess)

