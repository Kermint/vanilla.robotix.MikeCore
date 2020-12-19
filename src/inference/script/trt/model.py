# -- python -----
import numpy as np
import ctypes
import time
import cv2
import os
# -- tensorrt ---
import graphsurgeon as gs
import tensorrt as trt
import uff
# -- cuda -------
import pycuda.driver as cuda
from src.inference.script.preprocess import *

class TrtModel:
    def __init__(self, model, labels):
        self.initialized = False
        self.model = model
        self.labels = labels

    def initialize(self):
        ctypes.CDLL(os.path.abspath("/home/xavier/ROS/vanilla.robotix.MikeCore/src/inference/script/trt/lib/libflattenconcat.so"))
        self.logger = trt.Logger(trt.Logger.INFO)
        trt.init_libnvinfer_plugins(self.logger, '')
        self.prep = ResizePreprocessor(300, 300)
        self.runtime = trt.Runtime(self.logger)
        self.__compile(self.model)

    def preprocess(self, image):
        image = cv2.cvtColor(image, cv2.COLOR_BGR2RGB)
        image = ResizePreprocessor(300, 300).preprocess(image)
        image = (2.0/255.0) * image - 1.0
        image = image.transpose((2, 0, 1))
        return image

    def predict_draw(self, image):
        if not self.initialized:
            self.initialize()
            self.initialized = True

        if image is None:
            print("image is none")
            return None

        copy = self.prep.preprocess(image.copy())
        image = self.preprocess(image)

        context = self.context
        stream = self.stream

        np.copyto(self.host_inputs[0], image.ravel())

        cuda.memcpy_htod_async(self.cuda_inputs[0], self.host_inputs[0], stream)
        context.execute_async(bindings=self.bindings, stream_handle=stream.handle)
        cuda.memcpy_dtoh_async(self.host_outputs[1], self.cuda_outputs[1], stream)
        cuda.memcpy_dtoh_async(self.host_outputs[0], self.cuda_outputs[0], stream)
        stream.synchronize()

        output = self.host_outputs[0]
        height = width = 300

        for i in range(int(len(output) / self.model.layout)):
            prefix = i * self.model.layout
            index = int(output[prefix + 0])
            lid = int(output[prefix + 1])
            conf = output[prefix + 2]
            xmin = int(output[prefix + 3] * width)
            ymin = int(output[prefix + 4] * height)
            xmax = int(output[prefix + 5] * width)
            ymax = int(output[prefix + 6] * height)
            label = self.labels[lid]
            #print(lid)

            if conf > 0.1:
                print(conf)
                cv2.rectangle(copy, (xmin, ymin), (xmax, ymax), (172, 217, 153), 2)
                cv2.putText(copy, label, (xmin + 10, ymin + 10), cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 255, 255), 2, cv2.LINE_AA)

        return copy

    # compile model into TensorRT
    def __compile(self, model):
        import pycuda.autoinit

        # if file doesn't exists
        if not os.path.isfile(model.TRTbin):
            dynamic_graph = model.add_plugin(gs.DynamicGraph(model.path))
            _ = uff.from_tensorflow(dynamic_graph.as_graph_def(), model.output_name, output_filename='tmp.uff')

            with trt.Builder(self.logger) as builder, builder.create_network() as network, trt.UffParser() as parser:
                builder.max_workspace_size = 1 << 28
                builder.max_batch_size = 1
                builder.fp16_mode = True

                parser.register_input('Input', model.dims)
                parser.register_output('MarkOutput_0')
                parser.parse('tmp.uff', network)
                engine = builder.build_cuda_engine(network)

                buf = engine.serialize()
                with open(model.TRTbin, 'wb') as f:
                    f.write(buf)

        # create engine
        with open(model.TRTbin, 'rb') as f:
            buf = f.read()
            engine = self.runtime.deserialize_cuda_engine(buf)

        self.context = engine.create_execution_context()

        # create buffer
        self.host_inputs = []
        self.cuda_inputs = []
        self.host_outputs = []
        self.cuda_outputs = []
        self.bindings = []
        self.stream = cuda.Stream()

        for binding in engine:
            size = trt.volume(engine.get_binding_shape(binding)) * engine.max_batch_size
            host_mem = cuda.pagelocked_empty(size, np.float32)
            cuda_mem = cuda.mem_alloc(host_mem.nbytes)

            self.bindings.append(int(cuda_mem))
            if engine.binding_is_input(binding):
                self.host_inputs.append(host_mem)
                self.cuda_inputs.append(cuda_mem)
            else:
                self.host_outputs.append(host_mem)
                self.cuda_outputs.append(cuda_mem)