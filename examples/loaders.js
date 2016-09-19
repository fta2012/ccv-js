'use strict';

const loadSourceCache = {};

/**
 * Returns a promise that will resolve to either an img or video
 * If the url === 'WEBCAM', load a video from webcam.
 * If the url is an imgur animated gif, load it as a mp4 video
 * If the url ends in mp4, load it as a mp4 video
 * Otherwise just fetch as img
 * If passed in an array of urls it will resolve when all loaded
 */
const loadSource = (source) => {
  if (Array.isArray(source)) {
    return Promise.all(source.map(loadSource));
  }

  if (loadSourceCache[source]) {
    return loadSourceCache[source];
  }

  const promise = new Promise((resolve, reject) => {
    if (source === 'WEBCAM') {
      resolve(loadWebcam());
    } else {
      const url = new URL(source);
      if (url.hostname === 'imgur.com' || url.hostname === 'i.imgur.com') {
        // Figure out which imgur api to use based on url
        console.assert(url.pathname[0] === '/');
        const path = url.pathname.split('.')[0].slice(1).split('/'); // strip extension and split into parts
        let api;
        let imgurId;
        if (path.length === 1) {
          api = 'image';
          imgurId = path[0];
        } else {
          console.assert(path.length === 2);
          if (path[0] === 'a' || path[0] === 'album') {
            api = 'album';
          } else if (path[0] === 'g' || path[0] === 'gallery') {
            api = 'gallery';
          }
          imgurId = path[1];
        }
        resolve(loadImgur(api, imgurId));
      } else {
        if (source.endsWith('.mp4')) {
          resolve(loadImage(source));
        } else {
          resolve(loadVideo(source));
        }
      }
    }
  });

  loadSourceCache[source] = promise;
  promise.catch((e) => {
    console.error(e);
    delete loadSourceCache[source]; // Uncache the promise on error
  });
  return promise;
};

const videoPromise = (objectURL) => {
  return new Promise((resolve, reject) => {
    const video = document.createElement('video');
    video.crossOrigin = 'Anonymous';
    video.src = objectURL;
    video.autoplay = true;
    video.loop = true;
    video.oncanplay = () => resolve(video);
    video.onerror = (err) => reject(err);
  });
};

const loadWebcam = () => {
  return navigator.mediaDevices.getUserMedia({
      // TODO: these constraints don't work in firefox
      video: {
        mandatory: {
          maxWidth: 160,
          maxHeight: 120
        }
      }
    })
    .then((stream) => {
      return videoPromise(URL.createObjectURL(stream));
    });
};

const loadVideo = (url) => {
  return fetch(url)
    .then((response) => response.blob())
    .then((blob) => {
      return videoPromise(URL.createObjectURL(blob));
    });
};

const loadImage = (url) => {
  return fetch(url)
    .then((response) => response.blob())
    .then((blob) => {
      return new Promise((resolve, reject) => {
        const img = new Image();
        img.crossOrigin = 'Anonymous';
        img.src = URL.createObjectURL(blob);
        img.onload = () => resolve(img);
        img.onerror = (err) => reject(err);
      });
    });
};

const loadImgur = (api, imgurId) => {
  return fetch(`https://api.imgur.com/3/${api}/${imgurId}`, {
      headers: {
        Authorization: 'Client-ID 6a5400948b3b376' //  TODO took this from their example app, should make own id instead
      }
    })
    .then((res) => {
      return res.json();
    })
    .then((json) => {
      if (!json.success) {
        throw Error(json.data.error);
      }
      const { data } = json;
      const { mp4, link } = data.images ? data.images[0] : data; // Only take first image if album or gallery
      if (mp4) {
        return loadVideo(mp4);
      } else {
        console.assert(link);
        return loadImage(link);
      }
    });
};
