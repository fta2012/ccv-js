Module = Module || {};
Module.isImageData = function(element) {
  return element instanceof ImageData || (
    typeof element === 'object' &&
    'width' in element &&
    'height' in element &&
    'data' in element &&
    (element.data instanceof Uint8Array || element.data instanceof Uint8ClampedArray)
  );
};

Module.getElement = function(source) {
  if (!document) {
    console.log("Can't use dom in webworker");
  }
  var element = null;
  // TODO instanceof jQuery?
  if (source instanceof HTMLElement) {
    element = source;
  } else if (typeof source ==='string' || source instanceof String) {
    // Selector to an element or id of an element
    element = document.querySelector(source) || document.getElementById(source);
  }
  return element;
};

// Reads image data from ImageData or HTMLCanvasElement/HTMLImageElement/HTMLVideoElement(or string selector)
Module.readImageData = function(source) {
  console.assert(source);
  var imageData;
  if (Module.isImageData(source)) {
    // ImageData
    imageData = source;
  } else {
    var element = Module.getElement(source);
    if (!element) {
      console.error("Can't find element for selector:", source);
      return;
    }
    var canvas;
    var context;
    if (element instanceof HTMLCanvasElement) {
      // Canvas
      canvas = element;
      context = canvas.getContext('2d');
    } else if (element instanceof Image ||
               element instanceof HTMLImageElement ||
               element instanceof HTMLVideoElement) {
      if (element instanceof HTMLImageElement) {
        console.assert(element.complete);
      }
      // Image or Video
      var width = element.videoWidth || element.naturalWidth || element.width;
      var height = element.videoHeight || element.naturalHeight || element.height;
      canvas = document.createElement('canvas'); // draw image or video to a temp canvas
      canvas.width = width;
      canvas.height = height;
      context = canvas.getContext('2d');
      context.drawImage(element, 0, 0, canvas.width, canvas.height);
    }
    imageData = context.getImageData(0, 0, canvas.width, canvas.height);
  }
  return imageData;
};

// Writes image data directly into ImageData, HTMLCanvasElement, HTMLImageElement or creates a new canvas and appends it
// TODO: take in callback?
Module.writeImageData = function(dest, data, width, height) {
  console.assert(dest);

  if (Module.isImageData(dest)) {
    console.assert(dest.width === width, dest.height === height);
    dest.data.set(data);
    return;
  }

  var element = Module.getElement(dest);
  if (!element) {
    console.error("Can't convert argument to imageData:", dest);
    return;
  }
  console.assert(!(element instanceof HTMLVideoElement), 'Cannot write to video element');

  var canvas;
  if (element instanceof HTMLCanvasElement) {
    canvas = element;
  } else {
    canvas = document.createElement('canvas');
  }

  // Draw the uint8Array data to the imageData in the canvas
  canvas.width = width;
  canvas.height = height;
  var context = canvas.getContext('2d');
  var imageData = context.createImageData(width, height);
  imageData.data.set(data);
  context.putImageData(imageData, 0, 0);

  if (!(element instanceof HTMLCanvasElement)) {
    if (element instanceof Image || element instanceof HTMLImageElement) {
      element.src = canvas.toDataURL();
    } else {
      element.appendChild(canvas);
    }
  }
};

