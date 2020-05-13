class CropPreprocessor:
    def __init__(self, width, height, horiz=True, inter=cv2.INTER_AREA):
        # store the target image width, height, whether or not
        # horizontal flips should be included, along with the
        # interpolation method used when resizing
        self.width = width
        self.height = height
        self.horiz = horiz
        self.inter = inter

    def preprocess(self, image):
        # initialize the list of crops
        crops = []
        # grab the width and height of the image then use these
        # dimensions to define the corners of the image based
        (h, w) = image.shape[:2]
        coords = [
            [0, 0, self.width, self.height],
            [w - self.width, 0, w, self.height],
            [w - self.width, h - self.height, w, h],
            [0, h - self.height, self.width, h]
        ]
        # compute the center crop of the image as well
        dw = int(0.5 * (w-self.width))
        dh = int(0.5 * (h-self.height))
        coords.append([dw, dh, w - dw, h - dh])
        # loop over the coordinates, extract each of th crops,
        # and resize each of them to a fixed size
        for (start_x, start_y, end_x, end_y) in coords:
            crop = image[start_y:end_y, start_x:end_x]
            crop = cv2.resize(crop, (self.width, self.height), interpolation=self.inter)
            crops.append(crop)
        # check to see if the horizontal flips should be taken
        if self.horiz:
            # compute the horizontal mirror flips for each crop
            mirrors = [cv2.flip(c, 1) for c in crops]
            crops.extend(mirrors)
        # return the set of crops
        return np.array(crops)

