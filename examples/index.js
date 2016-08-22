"use strict";

let renderSpinner = () =>
  $('<i class="fa fa-spinner fa-spin"></i>');

let renderRect = ({ x, y, width, height }) =>
  $('<div class="rect">').css({
    position: 'absolute',
    left: x,
    top: y,
    width,
    height,
    border: '1px solid black',
    boxSizing: 'border-box',
    backgroundColor: 'rgba(0, 255, 0, 0.5)',
    pointerEvents: 'none'
  });

let renderRects = (rects) =>
  $($.map(rects, (rect) => renderRect(rect).get()));

let renderPoint = ({ x, y, radius }) => {
  radius = radius || 3;
  return $('<div class="dot">').css({
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

let renderKeypoint = (keypoint, color) => {
  let r = 6;
  return renderPoint({
    x: keypoint.x,
    y: keypoint.y,
    radius: r,
  })
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

let renderLines = (width, height, lines) => {
  let ret = $('<canvas class="lines">').css({ position: 'absolute', top: 0, left: 0 });
  let canvas = ret[0];
  canvas.width = width;
  canvas.height = height;
  let context = canvas.getContext('2d');
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




let startWebcam = (loop) => {
  if (window.video) {
    loop();
  } else {
    window.video = document.createElement('video');
    navigator.mediaDevices.getUserMedia({
      // TODO: these constraints don't work in firefox
      video: {
        mandatory: {
          maxWidth: 160,
          maxHeight: 120
        }
      }
    }).then((stream) => {
      window.video.src = window.URL.createObjectURL(stream);
      window.video.oncanplay = loop;
      window.video.play();
    }).catch((e) => {
      console.log(e);
      alert(e.toString());
    });
  }
}

$('#tld-demo-btn').on('click', (event) => {
  $('#tld-demo-btn').prop('disabled', true);
  let stats = new Stats();
  stats.showPanel(0);

  var tracker = null;

  let message = $('<p>').text('Starting webcam...');
  let canvasWrapper = $('<div>').css({position: 'relative', display: 'inline-block'});
  let canvas = $('<canvas>')
    .on('click touchstart', (e) => {
      let rect = canvas.getBoundingClientRect();
      let x = canvas.width * (e.clientX - rect.left) / (rect.right - rect.left);
      let y = canvas.height * (e.clientY - rect.top) / (rect.bottom - rect.top);
      let clickBox = {
        x: x - 10,
        y: y - 10,
        width: 20,
        height: 20
      };
      console.log('clicked', clickBox);
      canvasWrapper.find('.rect').remove();
      canvasWrapper.append(renderRect(clickBox));
      patches.empty();
      let ccvImage = new CCV.CCVImage(window.video).flipX();
      if (tracker) {
        tracker.delete();
      }
      tracker = new CCV.TLDTracker(ccvImage, clickBox, CCV.ccv_tld_default_params);
      ccvImage.delete();
    })[0];
  let patches = $('<div>').css({
    textAlign: 'left',
    maxHeight: 200,
    overflowY: 'auto'
  });

  $('#tld')
    .append($(stats.domElement).css({ position: 'absolute', top: 0, left: 0 }))
    .append(message)
    .append(canvasWrapper.append(canvas))
    .append(patches);

  let loop = () => {
    try {
      stats.begin();
      message.text('Click a point to track');
      let coloredImage = new CCV.CCVImage(window.video, CCV.CCV_IO_RGB_COLOR).flipX().write(canvas);
      let grayImage = new CCV.CCVImage(coloredImage, CCV.CCV_IO_GRAY);
      if (tracker) {
          tracker.track(grayImage, (info, found, topComps) => {
            canvasWrapper.find('.rect').remove();
            canvasWrapper.append(topComps.map((comp) => {
              return renderRect(comp.rect).css('opacity', comp.classification.confidence / 100);
            }));
            if (info.track_success) {
              canvasWrapper.append(
                renderRect(found.rect).css({
                  borderColor: 'red',
                  fontSize: 8,
                  fontWeight: 'bold',
                  color: 'red',
                }).text(`x:${found.rect.x} y:${found.rect.y}`)
              );
            }
            if (info.perform_learn) {
              new CCV.CCVImage(coloredImage, CCV.CCV_IO_RGB_COLOR)
                .slice(found.rect)
                .write(patches[0])
                .delete();
            }
          });
      }
      grayImage.delete();
      coloredImage.delete();
      stats.end();
    } catch(e) {
        console.log(e);
        alert(e.toString());
        return;
    }
    requestAnimationFrame(loop);
  };

  startWebcam(loop);
});


// TODO: Hook this up with tesseract.js: http://tesseract.projectnaptha.com/
$('#swt-demo-btn').on('click', () => {
  $(event.target).prepend(renderSpinner());
  $('#swt .rect').remove()
  setTimeout(() => {
    let swtImage = new CCV.CCVImage('#swt img');
    swtImage.swt_detect(CCV.ccv_swt_default_params, (words) => {
      $('#swt').append(renderRects(words));
    });
    $('#swt-demo-btn .fa-spinner').remove();
  }, 0);
});


// TODO need webworkers
$('#sift-demo-btn').on('click', (event) => {
  $(event.target).prepend(renderSpinner());
  $('#sift .lines, #sift .keypoint-match').remove();
  setTimeout(() => {
    let siftImage1 = new CCV.CCVImage($('#sift img')[0]);
    let siftImage2 = new CCV.CCVImage($('#sift img')[1]);
    siftImage1.sift_match(siftImage2, CCV.ccv_sift_default_params, (matches, keypoints1, keypoints2) => {
      let leftImageWidth = siftImage1.getWidth();
      let lines = []; // [from, to, color]
      let keypoints = matches.map(([left, right], i) => {
        let keypoint1 = keypoints1[left];
        let keypoint2 = keypoints2[right];
        keypoint2.x += leftImageWidth; // Drawn side by side
        let color = `hsl(${360 * i / matches.length}, 100%, 50%)`;

        lines.push([keypoint1, keypoint2, color]);

        return $('<div class="keypoint-match">')
          .css({ position: 'absolute', top: 0, left: 0, opacity: 0.5, zIndex: 0 })
          .hover(function() {
            $(this).css({opacity: 1, zIndex: 1})
          }, function() {
            $(this).css({opacity: 0.5, zIndex: 0})
          })
          .append(renderKeypoint(keypoint1, color))
          .append(renderKeypoint(keypoint2, color));
      });
      $('#sift')
        .append(
          renderLines(
            siftImage1.getWidth() + siftImage2.getWidth(),
            Math.max(siftImage1.getHeight(), siftImage1.getHeight()),
            lines
          )
        )
        .append(keypoints);
      $('#sift-demo-btn .fa-spinner').remove();
    });
    siftImage1.delete();
    siftImage2.delete();
  }, 0);
});


$('#scd-demo-btn').on('click', () => {
  $(event.target).prepend(renderSpinner());
  $('#scd .rect').remove();
  setTimeout(() => {
    let scdImage = new CCV.CCVImage('#scd img');
    scdImage.scd_detect(CCV.ccv_scd_default_params, (rects) => {
      $('#scd').append(renderRects(rects));
    });
    scdImage.delete();
    $('#scd-demo-btn .fa-spinner').remove();
  }, 0);
});


$('#lucas-kanade-demo-btn').on('click', () => {

	// TODO: should use some sort of goodFeaturesToTrack instead
  let makeGrid = (num, width, height) => {
    let points = [];
    for (let i = 0; i < num; i++) {
      for (let j = 0; j < num; j++) {
        points.push({
          x: width * (i + 1) / (num + 1),
          y: height * (j + 1) / (num + 1)
        });
      }
    }
    return points;
  }

	let width = 160;
	let height = 120;

	if (window.lkTracker) {
		window.lkTracker.addPoints(makeGrid(10, width, height));
		return;
	} else {
		window.lkTracker = new CCV.LucasKanadeTracker($.extend({}, CCV.ccv_lucas_kanade_default_params, {min_eigen: 0.001, win_size: { width: 20, height: 20 }}));
		window.lkTracker.addPoints(makeGrid(10, width, height));
	}


  let stats = new Stats();
  stats.showPanel(0);
  $('#lucas-kanade').append($(stats.domElement).css({ position: 'absolute', top: 0, left: 0 }))
	let canvasWrapper = $('#lucas-kanade').find('.canvas-wrapper');
	let message = $('#lucas-kanade').find('.message');
  let canvas = canvasWrapper.find('canvas')[0];


  let loop = () => {
    stats.begin();
    let image = new CCV.CCVImage(window.video).write(canvas);
		window.lkTracker.track(image, (prevPoints, points) => {
			canvasWrapper.find('.dot').remove();
			if (canvasWrapper.find('.lines').length > 20) {
				canvasWrapper.find('.lines').first().remove(); // Only keep last 20 frames of tracks
			}
			let tracks = points.map((_, i) => points[i].status ? [prevPoints[i], points[i].point] : null).filter((x) => x)
			message.text(`Tracking ${tracks.length} points`);
			canvasWrapper
				.append(points.map((p) =>
					renderPoint({x: p.point.x, y: p.point.y})
						.css('borderColor', p.status == 0 ? 'red' : 'green')
				))
				.append(renderLines(canvas.width, canvas.height, tracks));
		});
    image.delete();
    stats.end();
    requestAnimationFrame(loop);
  };

  startWebcam(loop);
});


$('#canny-demo-btn').on('click', () => {
  $('.result').remove();
  let container1 = $('<div class="result">')[0];
  let container2 = $('<div class="result">')[0];
  $('#canny').append(container1).append(container2);

  let cannyImage1 = new CCV.CCVImage('#canny img');
  let cannyImage2 = new CCV.CCVImage(cannyImage1);
  cannyImage1
    .write(container1)
    .canny(3, 50, 150)
    .write(container1)
    .delete();
  cannyImage2
    .blur(3)
    .write(container2)
    .canny(3, 50, 150)
    .write(container2)
    .delete();
});
