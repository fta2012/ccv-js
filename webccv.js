var Module = {};
function loadCCV() {
  return new Promise((resolve, reject) => {
    if (!('WebAssembly' in window)) {
      const error = 'Please use a browser supporting WASM';
      return reject(error);
    }
    fetch('build/ccv_without_filesystem.wasm')
      .then(response => response.arrayBuffer())
      .then(buffer => {
        Module.wasmBinary = buffer;
        script = document.createElement('script');
        doneEvent = new Event('done');
        script.addEventListener('done', buildCCV);
        script.src = 'build/ccv_without_filesystem.js';
        document.body.appendChild(script);
        function buildCCV() {
          return resolve(Module);
        }
      });
  });
}
