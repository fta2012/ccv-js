'use strict';

const loadScript = (url, callback) => {

    // Adding the script tag to the head as suggested before
    var head = document.getElementsByTagName('head')[0];
    var script = document.createElement('script');
    script.type = 'text/javascript';
    script.src = url;

    // Then bind the event to the callback function.
    // There are several events for cross browser compatibility.
    script.onreadystatechange = callback;
    script.onload = callback;

    // Fire the loading
    head.appendChild(script);
}

const swtDetect = (imgElement, container, message, params) => {

  // Allocate an empty ccv_dense_matrix_t* on the emscripten heap
  const image = new CCV.ccv_dense_matrix_t();

  // Read the html image element and create a new matrix into the given pointer.
  // Input could also be video, canvas, or ImageData.
  CCV.ccv_read(imgElement, image);

  // Detect words in the image and return them in a ccv_array_t* that you're responsible for freeing.
  const rects = CCV.ccv_swt_detect_words(image, params);
  const rects_js = rects.toJS();

  console.log(rects_js);

  message.text(`Detected ${rects_js.length} text regions.`);
  container.empty();
  CCV.ccv_write(image, container[0]); // This appends a new canvas into container. Could also output to an existing canvas or an ImageData.
  container.append(rects_js.map((x) => renderRect(x)));

  // Must explicitly free since there are no destructors in javascript
  rects.delete();
  image.delete();
};

const swtDetectOcr = (imgElement, container, message, params) => {

  const init = () => {
    console.log('Tesseract Script Loaded...');
    // Allocate an empty ccv_dense_matrix_t* on the emscripten heap
    const image = new CCV.ccv_dense_matrix_t();

    // Read the html image element and create a new matrix into the given pointer.
    // Input could also be video, canvas, or ImageData.
    CCV.ccv_read(imgElement, image);

    // Detect words in the image and return them in a ccv_array_t* that you're responsible for freeing.
    const rects = CCV.ccv_swt_detect_words(image, params);
    const rects_js = rects.toJS();

    console.log(rects_js);

    const textRegions = `Detected ${rects_js.length} text regions.`;
    message.text(textRegions);
    container.empty();
    CCV.ccv_write(image, container[0]); // This appends a new canvas into container. Could also output to an existing canvas or an ImageData.
    container.append(rects_js.map((x) => renderRect(x)));

    // -->> Tesseract detection
    message.html(`${textRegions}<p class='tesseract'></p><p class='detected'></p>`);
    message.find('.tesseract').text(`Processing ${rects_js.length} text regions with Tesseract OCR...`);

    const ctx = container[0].querySelector("canvas").getContext('2d');
    const rLength = rects_js.length - 1;
    rects_js.map(function(el,idx){

      let imageData = ctx.getImageData(el.x, el.y, el.width, el.height);
      Tesseract.recognize(imageData)
      .then( result => { 
        if (idx == rLength ){
          message.find('.detected').append(`<strong>${result.text}.</strong>`);
          message.find('.tesseract').text(`Processed ${rects_js.length} text regions with Tesseract OCR.`);
        } else {
          message.find('.detected').append(`<strong>${result.text}, </strong>`);
        }

      });

    });
    // <<-- Tesseract detection

    // Must explicitly free since there are no destructors in javascript
    rects.delete();
    image.delete();

  }

  loadScript('https://cdn.rawgit.com/naptha/tesseract.js/1.0.10/dist/tesseract.js', init);
  return;

};

const siftMatch = (imgElement1, imgElement2, container, message, params) => {
  // TODO: Should add UI for taking a photo with webcam for one input and webcam for the other. Small enough images can still run in real time.
  console.time('siftread1');
  const image = new CCV.ccv_dense_matrix_t();
  CCV.ccv_read(imgElement1, image, CCV.CCV_IO_GRAY);
  console.timeEnd('siftread1');

  console.time('sift1');
  const image_keypoints = new CCV.ccv_keypoint_array();
  const image_desc = new CCV.ccv_dense_matrix_t();
  CCV.ccv_sift(image, image_keypoints, image_desc, 0, params);
  const image_keypoints_js = image_keypoints.toJS();
  console.timeEnd('sift1');

  console.time('siftread2');
  const object = new CCV.ccv_dense_matrix_t();
  CCV.ccv_read(imgElement2, object, CCV.CCV_IO_GRAY);
  console.timeEnd('siftread2');

  console.time('sift2');
  const obj_keypoints = new CCV.ccv_keypoint_array();
  const obj_desc = new CCV.ccv_dense_matrix_t();
  CCV.ccv_sift(object, obj_keypoints, obj_desc, 0, params);
  const obj_keypoints_js = obj_keypoints.toJS();
  console.timeEnd('sift2');

  console.time('siftmatch');
  const matches = CCV.ccv_sift_match(image_desc, image_keypoints, obj_desc, obj_keypoints);
  console.timeEnd('siftmatch');

  console.groupCollapsed();
  console.log(image_keypoints_js);
  console.log(obj_keypoints_js);
  console.log(matches);
  console.groupEnd('output');

  message.text(`Detected ${image_keypoints_js.length} and ${obj_keypoints_js.length} keypoints. ${matches.length} matched.`);
  container.empty();
  CCV.ccv_write(image, container[0]);
  CCV.ccv_write(object, container[0]);
  container.append(renderSiftMatches(image.get_cols(), image.get_rows(), object.get_cols(), object.get_rows(), image_keypoints_js, obj_keypoints_js, matches));

  obj_desc.delete();
  obj_keypoints.delete();
  object.delete();
  image_desc.delete();
  image_keypoints.delete();
  image.delete();
};

let scdCascade = null;
const scdDetect = (imgElement, container, message, params) => {
  const image = new CCV.ccv_dense_matrix_t();
  CCV.ccv_read(imgElement, image, CCV.CCV_IO_RGB_COLOR);
  scdCascade = scdCascade || [CCV.ccv_scd_classifier_cascade_read(CCV.CCV_SCD_FACE_FILE)];
  const rects = CCV.ccv_scd_detect_objects(image, scdCascade, 1, params);
  const rects_js = rects.toJS();

  console.log(rects_js);

  message.text(`Detected ${rects.getLength()} faces.`);
  container.empty();
  CCV.ccv_write(image, container[0]);
  container.append(rects_js.map((x) => renderRect(x)));

  rects.delete();
  image.delete();
};

let icfCascade = null;
const icfDetect = (imgElement, container, message, params) => {
  const image = new CCV.ccv_dense_matrix_t();
  CCV.ccv_read(imgElement, image, CCV.CCV_IO_RGB_COLOR); // Doesn't seem to work on gray
  icfCascade = icfCascade || [CCV.ccv_icf_read_classifier_cascade(CCV.CCV_ICF_PEDESTRIAN_FILE)];
  const comps = CCV.ccv_icf_detect_objects(image, icfCascade, 1, params);
  const comps_js = comps.toJS();

  console.log(comps_js);

  message.text(`Detected ${comps.getLength()} pedestrians.`);
  container.empty();
  CCV.ccv_write(image, container[0]);
  container.append(comps_js.map((x) => renderComp(x)));

  comps.delete();
  image.delete();
};

let dpmModels = null;
const dpmDetect = (imgElement, container, message, params) => {
  const image = new CCV.ccv_dense_matrix_t();
  CCV.ccv_read(imgElement, image, CCV.CCV_IO_GRAY);
  dpmModels = dpmModels || [
    CCV.ccv_dpm_read_mixture_model(CCV.CCV_DPM_PEDESTRIAN_FILE),
    CCV.ccv_dpm_read_mixture_model(CCV.CCV_DPM_CAR_FILE)
  ];
  const rootComps = CCV.ccv_dpm_detect_objects(image, dpmModels, 1, params);
  const rootComps_js = rootComps.toJS();

  console.log(rootComps_js);

  const numPedestrians = rootComps_js.filter((x) => x.classification.id === 1).length;
  const numCars = rootComps_js.filter((x) => x.classification.id === 2).length;

  message.text(`Detected ${numPedestrians} pedestrians and ${numCars} cars`);
  container.empty();
  CCV.ccv_write(image, container[0]);
  container.append(rootComps_js.map((x) => renderRootComp(x)));

  rootComps.delete();
  image.delete();
};

const mserMatch = (imgElement, container, message, params) => {
  const fullImage = new CCV.ccv_dense_matrix_t();
  CCV.ccv_read(imgElement, fullImage, CCV.CCV_IO_GRAY);

  const image = new CCV.ccv_dense_matrix_t();
  CCV.ccv_sample_down(fullImage, image, 0, 0, 0);

  const canny = new CCV.ccv_dense_matrix_t();
  CCV.ccv_canny(image, canny, 0, 3, 175, 320);

  const outline = new CCV.ccv_dense_matrix_t();
  CCV.ccv_close_outline(canny, outline, 0);

  const mser = new CCV.ccv_dense_matrix_t();
  const mser_keypoint = CCV.ccv_mser(image, outline, mser, 0, params);

  console.log(mser_keypoint.toJS());

  message.text(`Detected ${mser_keypoint.getLength()} blobs.`);
  container.empty().css('whiteSpace', 'normal');
  CCV.ccv_write(fullImage, container[0]);
  // Can dump the image for intermediate steps to debug:
  // CCV.ccv_write(image, container[0]);
  // CCV.ccv_write(canny, container[0]);
  // CCV.ccv_write(outline, container[0]);
  container.append(renderMserCanvas(mser.get_data(), mser.get_cols(), mser.get_rows()));

  mser_keypoint.delete();
  mser.delete();
  outline.delete();
  canny.delete();
  image.delete();
  fullImage.delete();
};

const tldTrack = (() => {
  let prevFrame;
  let tracker;
  let trackerParams;

  const canvas = document.createElement('canvas');

  const rects = $('<div>')
    .css({
      position: 'absolute',
      top: 0,
      right: 0,
      bottom: 0,
      left: 0,
    })
    .on('click touchstart', (e) => {
      const rect = canvas.getBoundingClientRect();
      const x = canvas.width * (e.clientX - rect.left) / (rect.right - rect.left);
      const y = canvas.height * (e.clientY - rect.top) / (rect.bottom - rect.top);
      const r = canvas.width / 16;
      const clickBox = { // TODO: Allow dragging to select an arbitrary box
        x: x - r,
        y: y - r,
        width: 2 * r,
        height: 2 * r
      };
      console.log('clicked', clickBox);
      // Clear all rects and learned patches
      rects.empty();
      patches.empty();
      if (tracker) {
        tracker.delete();
      }
      // Draw click box
      rects.append(renderRect(clickBox));
      // Initialize tracker
      tracker = new CCV.ccv_tld_new(prevFrame, clickBox, trackerParams);
    });

  const patches = $('<div>').css({
    textAlign: 'left',
    maxHeight: 200,
    overflowY: 'auto'
  });

  return (video, container, message, params, shouldReset) => {
    if (!(video instanceof HTMLVideoElement)) {
      throw Error('Must use webcam or imgur animated gif');
    }

    if (shouldReset) {
      // Clear tracker state
      if (prevFrame) {
        prevFrame.delete();
        prevFrame = null;
      }
      if (tracker) {
        tracker.delete();
        tracker = null;
      }
      trackerParams = params;

      // Clear UI
      message.text('Click a point to track');
      rects.empty();
      patches.empty();
      container
        .append(canvas)
        .append(rects)
        .after(patches);
    }

    // Read the current frame of the video
    const image = new CCV.ccv_dense_matrix_t();
    CCV.ccv_read(video, image, CCV.CCV_IO_GRAY);

    // Display the frame on our canvas. Alternatively you can do CCV.ccv_write(image, canvas);
    canvas.width = video.videoWidth;
    canvas.height = video.videoHeight;
    canvas.getContext('2d').drawImage(video, 0, 0, canvas.width, canvas.height);

    if (!prevFrame || !tracker) {
      // Either first frame or not tracking anything yet, just move on
      if (prevFrame) {
        prevFrame.delete();
      }
      prevFrame = image;
      return;
    }

    const info = new CCV.ccv_tld_info_t();
    const newBox = CCV.ccv_tld_track_object(tracker, prevFrame, image, info);
    const topComps = tracker.top();

    console.log(newBox, topComps, info);

    message.text(JSON.stringify($.extend({}, info)));
    rects.empty().append(topComps.map((comp) =>
      renderRect(comp.rect).css({
        opacity: comp.classification.confidence / 100,
        pointerEvents: 'none'
      })
    ));
    if (info.track_success) {
      rects.append(
        renderRect(newBox.rect).css({
          borderColor: 'red',
          fontSize: 8,
          fontWeight: 'bold',
          color: 'red',
        }).text(`x:${newBox.rect.x} y:${newBox.rect.y}`)
      );
    }
    if (info.perform_learn) {
      patches.append(renderPatch(video, newBox.rect));
    }

    info.delete();
    prevFrame.delete();
    prevFrame = image;
  };
})();

const makeGrid = (width, height, num) => {
  const points = [];
  for (let i = 0; i < num; i++) {
    for (let j = 0; j < num; j++) {
      points.push({
        x: width * (i + 1) / (num + 1),
        y: height * (j + 1) / (num + 1)
      });
    }
  }
  return points;
};

const lucasKanadeTrack = (() => {
  let prevFrame;
  let prevPoints;
  let prevTracks = [];
  return (video, container, message, params, shouldReset) => {
    if (!(video instanceof HTMLVideoElement)) {
      throw Error('Must use webcam or imgur animated gif');
    }

    if (shouldReset) {
      if (prevFrame) {
        prevFrame.delete();
        prevFrame = null;
      }
      if (prevPoints) {
        prevPoints.delete();
        prevPoints = null;
      }
      prevTracks = [];
    }

    const width = video.videoWidth;
    const height = video.videoHeight;
    const frame = new CCV.ccv_dense_matrix_t();
    CCV.ccv_read(video, frame, CCV.CCV_IO_GRAY);

    container.empty();
    CCV.ccv_write(frame, container[0]);

    if (!prevFrame || prevPoints && prevPoints.getLength() === 0) {
      // Is either the first frame or lost all existing tracking points, reinit with a grid of points
      if (prevFrame) {
        prevFrame.delete();
      }
      prevFrame = frame;

      if (prevPoints) {
        prevPoints.delete();
      }
      prevPoints = CCV.ccv_decimal_point_array.fromJS(
        makeGrid(width, height, 30)
      );

      prevTracks = [];
      return;
    }

    console.assert(prevFrame && prevPoints);
    const pointsWithStatus = new CCV.ccv_decimal_point_with_status_array();
    CCV.ccv_optical_flow_lucas_kanade(prevFrame, frame, prevPoints, pointsWithStatus, params);
    const prevPointsJS = prevPoints.toJS();
    const pointsWithStatusJS = pointsWithStatus.toJS();

    const tracks = pointsWithStatusJS.map(({point, status}, i) => status ? [prevPointsJS[i], point] : null).filter((x) => x);
    prevTracks.push(renderLines(width, height, tracks));

    message.text(`Tracking ${tracks.length} points`);
    container
      .append(prevTracks)
      .append(pointsWithStatusJS.map((p) =>
        renderPoint({x: p.point.x, y: p.point.y}).css('borderColor', p.status ? 'green' : 'red')
      ));

    prevFrame.delete();
    prevFrame = frame;

    prevPoints.delete();
    prevPoints = CCV.ccv_decimal_point_array.fromJS( // Convert ccv_decimal_point_with_status_t to ccv_decimal_point_t
      pointsWithStatusJS.filter((p) => p.status).map((p) => p.point)
    );
    pointsWithStatus.delete();

    if (prevTracks.length > 20) { // Limit the number of prev tracks
      prevTracks = prevTracks.slice(1);
    }
  };
})();

const colors = [
  0x1f77b4, 0xaec7e8,
  0xff7f0e, 0xffbb78,
  0x2ca02c, 0x98df8a,
  0xd62728, 0xff9896,
  0x9467bd, 0xc5b0d5,
  0x8c564b, 0xc49c94,
  0xe377c2, 0xf7b6d2,
  0x7f7f7f, 0xc7c7c7,
  0xbcbd22, 0xdbdb8d,
  0x17becf, 0x9edae5
];

const renderRect = ({ x, y, width, height }) => {
  return $('<div class="rect">').css({
    position: 'absolute',
    left: x,
    top: y,
    width,
    height,
    border: '1px solid darkgreen',
    boxSizing: 'border-box',
    backgroundColor: 'rgba(0, 255, 0, 0.5)',
  });
};

const renderComp = ({rect, neighbors, classification: { confidence, id }}) => {
  return renderRect(rect).attr('title', `neighbors: ${neighbors}, confidence: ${confidence}, id: ${id}`);
};

const renderRootComp = (rootComp) => {
  const root = renderComp(rootComp);
  root.attr('title', `pnum: ${rootComp.pnum}, ${root.attr('title')}`);
  return $('<div class="root-comp">')
    .append(
      root.css({
        borderColor: `#${colors[rootComp.classification.id % colors.length].toString(16)}`,
        borderWidth: 2
      })
    )
    .append(
      rootComp.part.slice(0, rootComp.pnum).map((comp) =>
        renderComp(comp).css({
          borderColor: 'green',
          backgroundColor: 'transparent'
        })
      )
    );
};

const renderPoint = ({ x, y }, radius = 3) => {
  return $('<div class="point">').css({
    position: 'absolute',
    left: x,
    top: y,
    width: 0,
    height: 0,
    border: `${radius}px solid`,
    borderColor: 'rgba(0, 255, 0, 0.5)',
    borderRadius: radius,
    margin: -radius
  });
};

const renderKeypoint = (keypoint, color) => {
  // TODO: There are better ways to visualize sift keypoints
  const r = 6;
  return renderPoint({
    x: keypoint.x,
    y: keypoint.y
  }, r)
  .addClass('keypoint')
  .css({
    borderColor: color,
    transform: `rotate(${keypoint.regular.angle}rad) scale(${keypoint.regular.scale / 2})`,
    backfaceVisibility: 'hidden',
  })
  .append($('<div>').css({
    height: r,
    width: 0,
    border:'1px solid white',
    marginLeft: -1
  }));
};

const renderPatch = (canvasImageSource, { x, y, width, height }) => {
  const patch = document.createElement('canvas');
  patch.width = width;
  patch.height = height;
  patch.getContext('2d').drawImage(canvasImageSource, x, y, width, height, 0, 0, width, height);
  return patch;
};

const renderLines = (width, height, lines) => {
  const ret = $('<canvas class="lines">').css({ position: 'absolute', top: 0, left: 0 });
  const canvas = ret[0];
  canvas.width = width;
  canvas.height = height;
  const context = canvas.getContext('2d');
  context.globalAlpha = 0.5;
  lines.forEach(([from, to, color]) => {
    context.beginPath();
    context.moveTo(from.x, from.y);
    context.lineTo(to.x, to.y);
    context.strokeStyle = color || 'blue';
    context.stroke();
  });
  return ret;
};

const renderSiftMatches = (leftImageWidth, leftImageHeight, rightImageWidth, rightImageHeight, keypoints1, keypoints2, matches) => {
  const ret = $('<div>');
  const lines = []; // [from, to, color]
  const keypoints = [];
  matches.forEach(([left, right], i) => {
    const keypoint1 = keypoints1[left];
    const keypoint2 = keypoints2[right];
    keypoint2.x += leftImageWidth; // Drawn side by side
    const color = `hsl(${360 * i / matches.length}, 100%, 50%)`;

    lines.push([keypoint1, keypoint2, color]);

    keypoints.push($('<div class="keypoint-match">')
      .css({ position: 'absolute', top: 0, left: 0, opacity: 0.5, zIndex: 0 })
      .hover(function() {
        ret.find('.keypoint-match').css({opacity: 0, zIndex: 0});
        $(this).css({opacity: 1, zIndex: 1});
      }, function() {
        ret.find('.keypoint-match').css({opacity: 0.5, zIndex: 1});
      })
      .append(renderKeypoint(keypoint1, color))
      .append(renderKeypoint(keypoint2, color))
    );
  });
  return ret
    .append(
      renderLines(
        leftImageWidth + rightImageWidth,
        Math.max(leftImageHeight, rightImageHeight),
        lines
      )
    )
    .append(keypoints);
};

const renderMserCanvas = (mserUint32, width, height) => {
  const canvas = $('<canvas>');
  canvas[0].width = width;
  canvas[0].height = height;
  const context = canvas[0].getContext('2d');
  const imageData = context.getImageData(0, 0, width, height);
  const count = {};
  for (let i = 0; i < height; i++) {
    for (let j = 0; j < width; j++) {
      const label = mserUint32[i * width + j];
      count[label] = (count[label] || 0) + 1;
      const color = label ? colors[mserUint32[i * width + j] % colors.length] : 0xffffff;
      imageData.data[(i * width + j) * 4 + 0] = (color) & 0xff;
      imageData.data[(i * width + j) * 4 + 1] = (color >> 8) & 0xff;
      imageData.data[(i * width + j) * 4 + 2] = (color >> 16) & 0xff;
      imageData.data[(i * width + j) * 4 + 3] = 255;
    }
  }
  //console.log(count);
  context.putImageData(imageData, 0, 0);
  return canvas;
};

const renderDemo = ({id, title, desc, source, update, defaultParams}) => {
  const demoContainer = $('<div class="demo">').css('paddingTop', 50).attr('id', id); // padding-top and id neccessary for scrollspy
  $('#scrollspy ul').append(`<li><a href="#${demoContainer.attr('id')}">${title}</a></li>`);

  const stats = new Stats();
  stats.showPanel(0);
  const statsElement = $(stats.domElement)
    .css({
      position: 'absolute',
      top: 0,
      left: 0
    })
    .hide();

  const demo = () => { // TODO: Need to debounce clicks so you don't start multiple instances
    loadSource(guiObj.source)
      .then((elements) => {
        if (elements.some((x) => x instanceof HTMLVideoElement)) {
          let stop = false;
          guiObj.stop = () => {
            stop = true;
            gui.remove(demoButton);
            demoButton = gui.add(guiObj, 'demo');
          };
          gui.remove(demoButton);
          demoButton = gui.add(guiObj, 'stop');

          const loop = (shouldReset) => {
            if (stop) {
              return;
            }
            stats.begin();
            try {
              update(...elements, imageContainer, message, guiObj.params, shouldReset);
            } catch(e) {
              console.error(e);
              message.empty().append($('<code>').text(e));
              guiObj.stop();
              return;
            }
            stats.end();
            requestAnimationFrame(() => loop());
          };
          statsElement.show();
          loop(true);
        } else {
          statsElement.hide();
          message.text('Processing...');
          setTimeout(() => {
            const start = performance.now();
            try {
              update(...elements, imageContainer, message, guiObj.params);
            } catch(e) {
              console.error(e);
              message.empty().append($('<code>').text(e));
            }
            const end = performance.now();
            message.append($('<p>').text(`${end - start} ms`));
          }, 20);
        }
      })
      .catch((e) => {
        console.error(e);
        message.empty().append($('<code>').text(e));
      });
  };


  const gui = new dat.GUI({ autoPlace: false });
  const guiObj = {
    params: $.extend(true, {}, defaultParams), // Need deep copy to not mess with the real defaults in defaultParams
    source,// TODO: Should also allow file chooser, taking a still photos using the webcam, and choice of webcam resolution
    demo
  };
  const guiElement = $(gui.domElement)
    .css({
      position: 'absolute',
      top: 0,
      right: 0
    });
  const buildGui = (folder, obj) => {
    Object.keys(obj).forEach((key) => {
      const val = obj[key];
      if (val instanceof Object && typeof val !== 'function') {
        buildGui(folder.addFolder(key), val);
      } else {
        folder.add(obj, key);
      }
    });
  };
  const paramsFolder = gui.addFolder('params');
  buildGui(paramsFolder, guiObj.params);
  const sourceFolder = gui.addFolder('source');
  sourceFolder.open();
  source.forEach((x, i) => sourceFolder.add(source, i));
  let demoButton = gui.add(guiObj, 'demo');

  const message = $('<div class="message">').css({ padding: 20 });

  const imageContainer = $('<div class="canvas-wrapper">')
    .css({
      position: 'relative',
      display: 'inline-block',
      whiteSpace: 'nowrap'
    });

  return demoContainer
    .append($('<h2>').text(title))
    .append($('<p>').text(desc))
    .append(
      $('<div class="demo-area">')
        .css({
          backgroundColor: '#eee',
          textAlign: 'center',
          position: 'relative',
          marginTop: 10,
          padding: '50px 0',
          marginBottom: 50,
        })
        .append(message)
        .append(imageContainer)
        .append(statsElement)
        .append(guiElement)
    );
};


$(() => {
  $('#content').append(
    renderDemo({
      id: 'tld',
      title: 'TLD: Track Learn Detect',
      desc: 'A tracker that learns.',
      source: ['WEBCAM'],
      update: tldTrack,
      defaultParams: CCV.ccv_tld_default_params
    })
  ).append(
    renderDemo({
      id: 'swt',
      title: 'SWT: Stroke Width Transform',
      desc: 'Text detector.',
      source: ['https://imgur.com/DpnH0Lf'],
      update:  swtDetect,
      defaultParams: CCV.ccv_swt_default_params
    })
  ).append(
    renderDemo({
      id: 'swt-ocr',
      title: 'SWT: SWT with OCR',
      desc: 'Text detector with Tesseract OCR .',
      source: ['http://i.imgur.com/Q5tLzXR.jpg'],
      update:  swtDetectOcr,
      defaultParams: CCV.ccv_swt_default_params
    })
  ).append(
    renderDemo({
      id: 'sift',
      title: 'SIFT: Scale Invariant Feature Transform',
      desc: 'Feature detector with brute-force matching.',
      source: ['https://imgur.com/VAfL8Ww', 'https://imgur.com/Kr5mrow'],
      update:  siftMatch,
      defaultParams: CCV.ccv_sift_default_params
    })
  ).append(
    renderDemo({
      id: 'scd',
      title: 'SCD: SURF-Cascade Detection',
      desc: 'Face detector.',
      source: ['WEBCAM'],
      update: scdDetect,
      defaultParams: CCV.ccv_scd_default_params
    })
  ).append(
    renderDemo({
      id: 'icf',
      title: 'ICF: Integral Channel Features',
      desc: 'Pedestrian detector.',
      source: ['https://imgur.com/uN5eNym'],
      update: icfDetect,
      defaultParams: CCV.ccv_icf_default_params
    })
  ).append(
    renderDemo({
      id: 'dpm',
      title: 'DPM: Deformable Parts Model',
      desc: 'Car/pedestrian detector. This one might freeze your browser for a bit (~30 seconds).',
      source: ['http://imgur.com/gallery/XyqyM'],
      update: dpmDetect,
      defaultParams: $.extend({}, CCV.ccv_dpm_default_params, { flags: CCV.CCV_DPM_NO_NESTED }) // Without the no nested flag the result will look wrong for the demo image
    })
  ).append(
    renderDemo({
      id: 'lucas',
      title: 'Classics - Lucas-Kanade',
      desc: 'Optical flow.',
      source: ['https://imgur.com/gallery/i6DYBd6'],
      update: lucasKanadeTrack,
      defaultParams: CCV.ccv_lucas_kanade_default_params
    })
  ).append(
    renderDemo({
      id: 'mser',
      title: 'MSER: Maximally stable extremal regions',
      desc: 'Blob detector.',
      source: ['https://imgur.com/9EKhBNa.mp4'],
      update: mserMatch,
      defaultParams: CCV.ccv_mser_default_params
    })
  );

  $('.demo-area').toArray().reverse().forEach((x, i) => $(x).css('zIndex', i)); // dat.gui dropdown was hidden by next demo area

  $(`<style>
      .rect:hover {
        opacity: 0.9;
      }
      .canvas-wrapper img, .canvas-wrapper canvas {
        vertical-align: top;
      }
  </style>`).appendTo('head'); // align top is for the two sift images
});
