#include <emscripten.h>
#include <emscripten/bind.h>
#include <array>
#include <string>
#include <utility>

extern "C" {
#include <ccv.h>
#include <ccv_internal.h>
}

using namespace emscripten;

int main() {
  ccv_enable_default_cache();

  EM_ASM({
    //console.log('ready', Module);
  });
}

const ccv_mser_param_t ccv_mser_default_params = { // From ccv/bin/msermatch.c
  .min_area = 60,
  .max_area = 10000, // Changed
  .min_diversity = 0.2,
  .area_threshold = 1.01,
  .min_margin = 0.003,
  .max_evolution = 200,
  .edge_blur_sigma = sqrt(3.0),
  .delta = 5,
  .max_variance = 0.25,
  .direction = CCV_DARK_TO_BRIGHT,
};

typedef struct { // See ccv_tld_param_t
  ccv_size_t win_size;
  int level;
  float min_eigen;
} ccv_lucas_kanade_param_t;

const ccv_lucas_kanade_param_t ccv_lucas_kanade_default_params = {
  .win_size = {
    .width = ccv_tld_default_params.win_size.width,
    .height = ccv_tld_default_params.win_size.height
  },
  .level = ccv_tld_default_params.level,
  .min_eigen = 0.001,//ccv_tld_default_params.min_eigen,
};




// Reverse of _ccv_read_rgba_raw from ccv/lib/io/_ccv_io_raw.c
void _ccv_write_rgba_raw(ccv_dense_matrix_t* x, unsigned char* data) {
  int c = CCV_GET_CHANNEL(x->type);
  assert(CCV_GET_DATA_TYPE(x->type) == CCV_8U);
  assert(c == CCV_C3 || c == CCV_C1);

  unsigned char* mdata = x->data.u8;
  int step = x->step;
  int width = x->cols;
  int height = x->rows;

  if (c == 3) { // colored image
    for (int i = 0; i < height; i++) {
      for (int j = 0; j < width; j++) {
        data[4 * (i * width + j) + 0] = mdata[i * step + c * j + 0];
        data[4 * (i * width + j) + 1] = mdata[i * step + c * j + 1];
        data[4 * (i * width + j) + 2] = mdata[i * step + c * j + 2];
        data[4 * (i * width + j) + 3] = 255;
      }
    }
  } else {
    unsigned char max = 0;
    for (int i = 0; i < height; i++) {
      for (int j = 0; j < width; j++) {
        max = std::max(max, mdata[i * step + j]);
      }
    }
    if (max == 1) { // binary image TODO: remove this case
      for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
          data[4 * (i * width + j) + 0] = mdata[i * step + j] ? 255 : 0;
          data[4 * (i * width + j) + 1] = mdata[i * step + j] ? 255 : 0;
          data[4 * (i * width + j) + 2] = mdata[i * step + j] ? 255 : 0;
          data[4 * (i * width + j) + 3] = 255;
        }
      }
    } else { // grayscale image
      for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
          data[4 * (i * width + j) + 0] = mdata[i * step + j];
          data[4 * (i * width + j) + 1] = mdata[i * step + j];
          data[4 * (i * width + j) + 2] = mdata[i * step + j];
          data[4 * (i * width + j) + 3] = 255;
        }
      }
    }
  }
}

int ccv_write_html(ccv_dense_matrix_t* matrix, const val& imageDataOrElement) {
  // Convert ccv_dense_matrix_t::data into rgba layout first
  int width = matrix->cols;
  int height = matrix->rows;
  unsigned char* rgba = (unsigned char*)malloc(4 * width * height); // TODO: should just write directly into the js array
  _ccv_write_rgba_raw(matrix, rgba);

  // Copy the data into the given ImageData/HTMLCanvasElement/HTMLImageElement or into a new canvas child of the element
  val view(typed_memory_view(4 * width * height, rgba));
  val::module_property("writeImageData")(imageDataOrElement, view, width, height);

  free(rgba);

  return 0;
}

int ccv_read_html(const val& imageDataOrCanvasImageSource, ccv_dense_matrix_t** mat, int type) {
  // Get ImageData if it is a CanvasImageSource
  val imageData = val::module_property("readImageData")(imageDataOrCanvasImageSource);

  // Copy the rgba raw data into emscripten heap as the content of a string
  std::string s = imageData["data"]["buffer"].as<std::string>(); // TODO: Remove `["buffer"]` after https://github.com/kripken/emscripten/pull/4511
  int width = imageData["width"].as<int>();
  int height = imageData["height"].as<int>();

  // Read the rgba raw data into a ccv_dense_matrix_t*
  assert(type == CCV_IO_GRAY || type == CCV_IO_RGB_COLOR);
  ccv_read(s.c_str(), mat, CCV_IO_RGBA_RAW | type, height, width, width * 4);
  return 0;
}

template<typename T>
struct Deleter;
template<>
struct Deleter<ccv_dense_matrix_t> {
  void operator()(ccv_dense_matrix_t* ptr) {
    ccv_matrix_free(ptr);
  }
};
template<>
struct Deleter<ccv_array_t> {
  void operator()(ccv_array_t* ptr) {
    ccv_array_free(ptr);
  }
};
template<>
struct Deleter<ccv_tld_info_t> { // TODO: should just use unique_ptr's default_delete
  void operator()(ccv_tld_info_t* ptr) {
    free(ptr);
  }
};
template<>
struct Deleter<ccv_tld_t> {
  void operator()(ccv_tld_t* ptr) {
    ccv_tld_free(ptr);
  }
};
template<>
struct Deleter<ccv_dpm_mixture_model_t> {
  void operator()(ccv_dpm_mixture_model_t* ptr) {
    ccv_dpm_mixture_model_free(ptr);
  }
};
template<>
struct Deleter<ccv_icf_classifier_cascade_t> {
  void operator()(ccv_icf_classifier_cascade_t* ptr) {
    ccv_icf_classifier_cascade_free(ptr);
  }
};
template<>
struct Deleter<ccv_scd_classifier_cascade_t> {
  void operator()(ccv_scd_classifier_cascade_t* ptr) {
    ccv_scd_classifier_cascade_free(ptr);
  }
};

template<typename T>
class PtrBase {
public:
  PtrBase() {};
  PtrBase(T* ptr) : pointer(ptr) {}

  operator T*() const {
    return pointer.get();
  }
  operator T**() {
    // TODO: This is bad. Want to allow setting the unique_ptr via a T**
    return (T**)&pointer;
  }

protected:
  std::unique_ptr<T, Deleter<T>> pointer = nullptr;
};

template<typename T, typename E=void> // E is only needed as the element type when T == ccv_array_t
class Ptr : public PtrBase<T> {
public:
  Ptr() {};
  Ptr(T* ptr) : PtrBase<T>(ptr) {};
};

template<>
class Ptr<ccv_dense_matrix_t> : public PtrBase<ccv_dense_matrix_t> {
public:
  Ptr() {};
  Ptr(ccv_dense_matrix_t* ptr) : PtrBase<ccv_dense_matrix_t>(ptr) {};

  int getRows() {
    return pointer->rows;
  }

  int getCols() {
    return pointer->cols;
  }

  int getStep() {
    return pointer->step;
  }

  int getType() {
    return pointer->type;
  }

  int getDataType() {
    return CCV_GET_DATA_TYPE(pointer->type);
  }

  int getDataTypeSize() {
    return CCV_GET_DATA_TYPE_SIZE(pointer->type);
  }

  int getChannel() {
    return CCV_GET_CHANNEL(pointer->type);
  }

  int getDataSize() {
    return pointer->step * pointer->rows;
  }

  val getData() {
    int numElement = getDataSize() / getDataTypeSize();
    switch(getDataType()) {
      case CCV_8U:
        return val(typed_memory_view(numElement, pointer->data.u8));
      case CCV_32S:
        return val(typed_memory_view(numElement, pointer->data.i32));
      case CCV_32F:
        return val(typed_memory_view(numElement, pointer->data.f32));
      case CCV_64S:
        return val(typed_memory_view(numElement, pointer->data.i64));
      case CCV_64F:
        return val(typed_memory_view(numElement, pointer->data.f64));
      default:
        return val::null();
    }
  }
};

template<typename T>
class Ptr<ccv_array_t, T> : public PtrBase<ccv_array_t> {
public:
  Ptr() {};
  Ptr(ccv_array_t* ptr) : PtrBase<ccv_array_t>(ptr) {};

  void init() {
    pointer.reset(ccv_array_new(sizeof(T), 0, 0));
  }

  int size() {
    assert(pointer);
    return pointer->rnum;
  }

  void push(const T& x) {
    assert(pointer);
    ccv_array_push(pointer.get(), &x);
  }

  val get(int i) {
    assert(pointer);
    return val(*(T*)ccv_array_get(pointer.get(), i));
  }

  val toJS() {
    assert(pointer);
    val jsArray = val::array();
    for (int i = 0; i < pointer->rnum; i++) {
      jsArray.call<void>("push", get(i));
    }
    return jsArray;
  }
};

template<>
class Ptr<ccv_tld_t> : public PtrBase<ccv_tld_t> {
public:
  Ptr() {};
  Ptr(ccv_tld_t* ptr) : PtrBase<ccv_tld_t>(ptr) {};

  val getTop() {
    val jsArray = val::array();
    for (int i = 0; i < pointer->top->rnum; i++) {
      jsArray.call<void>("push", val(*(ccv_comp_t*)ccv_array_get(pointer->top, i)));
    }
    return jsArray;
  }

  val getBox() {
    return val(pointer->box);
  }
};

template<>
class Ptr<ccv_tld_info_t> : public PtrBase<ccv_tld_info_t> {
public:
  Ptr() : PtrBase() {};

  void init() {
    pointer.reset(new ccv_tld_info_t());
  }

  val toJS() {
    assert(pointer);
    return val(*pointer);
  }
};

// int ccv_read(const char *in, ccv_dense_matrix_t **x, int type)
int ccvjs_read(val source, Ptr<ccv_dense_matrix_t>& out, int type) {
  assert(type == CCV_IO_GRAY || type == CCV_IO_RGB_COLOR);
  return ccv_read_html(source, out, type);
}
int ccvjs_read(val source, Ptr<ccv_dense_matrix_t>& out) {
  return ccvjs_read(source, out, CCV_IO_GRAY);
}

// int ccv_write(ccv_dense_matrix_t *mat, char *out, int *len, int type, void *conf)
int ccvjs_write(const Ptr<ccv_dense_matrix_t>& mat, val out) {
  return ccv_write_html(mat, out);
}

// ccv_tld_t* ccv_tld_new(ccv_dense_matrix_t* a, ccv_rect_t box, ccv_tld_param_t params);
Ptr<ccv_tld_t> ccvjs_tld_new(const Ptr<ccv_dense_matrix_t>& a, ccv_rect_t box, ccv_tld_param_t params = ccv_tld_default_params) {
  return ccv_tld_new(a, box, params);
}

// ccv_comp_t ccv_tld_track_object(ccv_tld_t* tld, ccv_dense_matrix_t* a, ccv_dense_matrix_t* b, ccv_tld_info_t* info);
ccv_comp_t ccvjs_tld_track_object(const Ptr<ccv_tld_t>& tld, const Ptr<ccv_dense_matrix_t>& a, const Ptr<ccv_dense_matrix_t>& b, Ptr<ccv_tld_info_t>& info) {
  return ccv_tld_track_object(tld, a, b, info);
}

// ccv_array_t* ccv_swt_detect_words(ccv_dense_matrix_t* a, ccv_swt_param_t params);
Ptr<ccv_array_t, ccv_rect_t> ccvjs_swt_detect_words(const Ptr<ccv_dense_matrix_t>& a, ccv_swt_param_t params = ccv_swt_default_params) {
  return ccv_swt_detect_words(a, params);
}

// void ccv_sift(ccv_dense_matrix_t* a, ccv_array_t** keypoints, ccv_dense_matrix_t** desc, int type, ccv_sift_param_t params);
void ccvjs_sift(const Ptr<ccv_dense_matrix_t>& a, Ptr<ccv_array_t, ccv_keypoint_t>& keypoints, Ptr<ccv_dense_matrix_t>& desc, int type, ccv_sift_param_t params = ccv_sift_default_params) {
   ccv_sift(a, keypoints, desc, type, params);
}

// From ccv/bin/siftmatch.c
val ccvjs_sift_match(const Ptr<ccv_dense_matrix_t>& desc1, const Ptr<ccv_array_t, ccv_keypoint_t>& kp1, const Ptr<ccv_dense_matrix_t>& desc2, const Ptr<ccv_array_t, ccv_keypoint_t>& kp2) {
  double ratio = 0.36;

  ccv_array_t* image_keypoints = kp1;
  ccv_dense_matrix_t* image_desc = desc1;
  ccv_array_t* obj_keypoints = kp2;
  ccv_dense_matrix_t* obj_desc = desc2;

  int i, j, k;
  val matches = val::array();
  for (i = 0; i < obj_keypoints->rnum; i++) {
    float* odesc = obj_desc->data.f32 + i * 128;
    int minj = -1;
    double mind = 1e6, mind2 = 1e6;
    for (j = 0; j < image_keypoints->rnum; j++) {
      float* idesc = image_desc->data.f32 + j * 128;
      double d = 0;
      for (k = 0; k < 128; k++) {
        d += (odesc[k] - idesc[k]) * (odesc[k] - idesc[k]);
        if (d > mind2)
          break;
      }
      if (d < mind) {
        mind2 = mind;
        mind = d;
        minj = j;
      } else if (d < mind2) {
        mind2 = d;
      }
    }
    if (mind < mind2 * ratio) {
      ccv_keypoint_t* op = (ccv_keypoint_t*)ccv_array_get(obj_keypoints, i);
      ccv_keypoint_t* kp = (ccv_keypoint_t*)ccv_array_get(image_keypoints, minj);
      val pair = val::array();
      pair.call<void>("push", minj);
      pair.call<void>("push", i);
      matches.call<void>("push", pair);
    }
  }
  return matches;
}


#ifdef WITH_FILESYSTEM

// ccv_scd_classifier_cascade_t* ccv_scd_classifier_cascade_read(const char* filename);
Ptr<ccv_scd_classifier_cascade_t> ccvjs_scd_classifier_cascade_read(const std::string& filename) {
  return ccv_scd_classifier_cascade_read(filename.c_str());
}

// ccv_array_t* ccv_scd_detect_objects(ccv_dense_matrix_t* a, ccv_scd_classifier_cascade_t** cascades, int count, ccv_scd_param_t params);
Ptr<ccv_array_t, ccv_rect_t> ccvjs_scd_detect_objects(const Ptr<ccv_dense_matrix_t>& a, Ptr<ccv_scd_classifier_cascade_t>& cascades, int count, ccv_scd_param_t params = ccv_scd_default_params) {
  return ccv_scd_detect_objects(a, cascades, count, params);
}

// ccv_icf_classifier_cascade_t* ccv_icf_read_classifier_cascade(const char* filename);
Ptr<ccv_icf_classifier_cascade_t> ccvjs_icf_read_classifier_cascade(const std::string& filename) {
  return ccv_icf_read_classifier_cascade(filename.c_str());
}

// ccv_array_t* ccv_icf_detect_objects(ccv_dense_matrix_t* a, void* cascade, int count, ccv_icf_param_t params);
Ptr<ccv_array_t, ccv_comp_t> ccvjs_icf_detect_objects(const Ptr<ccv_dense_matrix_t>& a, Ptr<ccv_icf_classifier_cascade_t>& cascade, int count, ccv_icf_param_t params = ccv_icf_default_params) {
  return ccv_icf_detect_objects(a, (ccv_icf_classifier_cascade_t**)cascade, count, params);
}

// ccv_dpm_mixture_model_t* ccv_dpm_read_mixture_model(const char* directory);
Ptr<ccv_dpm_mixture_model_t> ccvjs_dpm_read_mixture_model(std::string directory) {
  return ccv_dpm_read_mixture_model(directory.c_str());
}

// ccv_array_t* ccv_dpm_detect_objects(ccv_dense_matrix_t* a, ccv_dpm_mixture_model_t** model, int count, ccv_dpm_param_t params);
Ptr<ccv_array_t, ccv_root_comp_t> ccvjs_dpm_detect_objects(const Ptr<ccv_dense_matrix_t>& a, Ptr<ccv_dpm_mixture_model_t>& cascades, int count, ccv_dpm_param_t params = ccv_dpm_default_params) {
  return ccv_dpm_detect_objects(a, cascades, count, params);
}

#endif // WITH_FILESYSTEM

// ccv_array_t* ccv_mser(ccv_dense_matrix_t* a, ccv_dense_matrix_t* h, ccv_dense_matrix_t** b, int type, ccv_mser_param_t params);
Ptr<ccv_array_t, ccv_mser_keypoint_t> ccvjs_mser(const Ptr<ccv_dense_matrix_t>& a, const Ptr<ccv_dense_matrix_t>& h, Ptr<ccv_dense_matrix_t>& b, int type, ccv_mser_param_t params = ccv_mser_default_params) {
  return ccv_mser(a, h, b, type, params);
}

// void ccv_canny(ccv_dense_matrix_t *a, ccv_dense_matrix_t **b, int type, int size, double low_thresh, double high_thresh)
void ccvjs_canny(const Ptr<ccv_dense_matrix_t>& a, Ptr<ccv_dense_matrix_t>& b, int type, int size, double low_thresh, double high_thresh) {
  ccv_canny(a, b, type, size, low_thresh, high_thresh);
}

// void ccv_close_outline(ccv_dense_matrix_t* a, ccv_dense_matrix_t** b, int type);
void ccvjs_close_outline(const Ptr<ccv_dense_matrix_t>& a, Ptr<ccv_dense_matrix_t>& b, int type) {
  ccv_close_outline(a, b, type);
}

// void ccv_flip(ccv_dense_matrix_t *a, ccv_dense_matrix_t **b, int btype, int type)
void ccvjs_flip(const Ptr<ccv_dense_matrix_t>& a, Ptr<ccv_dense_matrix_t>& b, int btype, int type) {
  ccv_flip(a, b, btype, type);
}

// void ccv_slice(ccv_matrix_t *a, ccv_matrix_t **b, int btype, int y, int x, int rows, int cols)
void ccvjs_slice(const Ptr<ccv_dense_matrix_t>& a, Ptr<ccv_dense_matrix_t>& b, int btype, int y, int x, int rows, int cols) {
  ccv_slice(a, (ccv_matrix_t**)(ccv_dense_matrix_t**)b, btype, y, x, rows, cols);
}
void ccvjs_slice(const Ptr<ccv_dense_matrix_t>& a, Ptr<ccv_dense_matrix_t>& b, int btype, ccv_rect_t rect) {
  ccv_slice(a, (ccv_matrix_t**)(ccv_dense_matrix_t**)b, btype, rect.y, rect.x, rect.height, rect.width);
}

// void ccv_blur(ccv_dense_matrix_t *a, ccv_dense_matrix_t **b, int type, double sigma)
void ccvjs_blur(const Ptr<ccv_dense_matrix_t>& a, Ptr<ccv_dense_matrix_t>& b, int type, double sigma) {
  ccv_blur(a, b, type, sigma);
}

//void ccv_sample_down(ccv_dense_matrix_t* a, ccv_dense_matrix_t** b, int type, int src_x, int src_y);
void ccvjs_sample_down(const Ptr<ccv_dense_matrix_t>& a, Ptr<ccv_dense_matrix_t>& b, int type, int src_x, int src_y) {
  ccv_sample_down(a, b, type, src_x, src_y);
}

// void ccv_optical_flow_lucas_kanade(ccv_dense_matrix_t *a, ccv_dense_matrix_t *b, ccv_array_t *point_a, ccv_array_t **point_b, ccv_size_t win_size, int level, double min_eigen)
void ccvjs_optical_flow_lucas_kanade(const Ptr<ccv_dense_matrix_t>& a, const Ptr<ccv_dense_matrix_t>& b, const Ptr<ccv_array_t, ccv_decimal_point_t>& point_a, Ptr<ccv_array_t, ccv_decimal_point_with_status_t>& point_b, ccv_size_t win_size, int level, double min_eigen) {
  ccv_optical_flow_lucas_kanade(a, b, point_a, point_b, win_size, level, min_eigen);
}
void ccvjs_optical_flow_lucas_kanade(const Ptr<ccv_dense_matrix_t>& a, const Ptr<ccv_dense_matrix_t>& b, const Ptr<ccv_array_t, ccv_decimal_point_t>& point_a, Ptr<ccv_array_t, ccv_decimal_point_with_status_t>& point_b, ccv_lucas_kanade_param_t params = ccv_lucas_kanade_default_params) {
  ccv_optical_flow_lucas_kanade(a, b, point_a, point_b, params.win_size, params.level, params.min_eigen);
}

template<typename T>
void register_ccv_array(const char* name) {
  class_<Ptr<ccv_array_t, T>>(name)
    .constructor()
    .function("init", &Ptr<ccv_array_t, T>::init)
    .function("get", &Ptr<ccv_array_t, T>::get)
    .function("push", &Ptr<ccv_array_t, T>::push)
    .function("size", &Ptr<ccv_array_t, T>::size)
    .function("toJS", &Ptr<ccv_array_t, T>::toJS);
}

template<typename T, std::size_t... I>
void register_array_elements(T& a, std::index_sequence<I...>) {
  (a.element(index<I>()), ...);
}
template<typename T, size_t N>
void register_array(const char* name) {
  value_array<std::array<T, N>> temp(name);
  register_array_elements(temp, std::make_index_sequence<N>());
}

EMSCRIPTEN_BINDINGS(ccv_js_module) {
  class_<Ptr<ccv_dense_matrix_t>>("ccv_dense_matrix_t")
    .constructor()
    .function("getRows", &Ptr<ccv_dense_matrix_t>::getRows)
    .function("getCols", &Ptr<ccv_dense_matrix_t>::getCols)
    .function("getStep", &Ptr<ccv_dense_matrix_t>::getStep)
    .function("getType", &Ptr<ccv_dense_matrix_t>::getType)
    .function("getDataType", &Ptr<ccv_dense_matrix_t>::getDataType)
    .function("getDataTypeSize", &Ptr<ccv_dense_matrix_t>::getDataTypeSize)
    .function("getChannel", &Ptr<ccv_dense_matrix_t>::getChannel)
    .function("getDataSize", &Ptr<ccv_dense_matrix_t>::getDataSize)
    .function("getData", &Ptr<ccv_dense_matrix_t>::getData);
  class_<Ptr<ccv_tld_t>>("ccv_tld_t")
    .constructor()
    .function("getTop", &Ptr<ccv_tld_t>::getTop)
    .function("getBox", &Ptr<ccv_tld_t>::getBox);
#ifdef WITH_FILESYSTEM
  class_<Ptr<ccv_scd_classifier_cascade_t>>("ccv_scd_classifier_cascade_t").constructor();
  class_<Ptr<ccv_icf_classifier_cascade_t>>("ccv_icf_classifier_cascade_t").constructor();
  class_<Ptr<ccv_dpm_mixture_model_t>>("ccv_dpm_mixture_model_t").constructor();
#endif
  class_<Ptr<ccv_tld_info_t>>("ccv_tld_info_t")
    .function("init", &Ptr<ccv_tld_info_t>::init)
    .function("toJS", &Ptr<ccv_tld_info_t>::toJS)
    .constructor();
  register_ccv_array<ccv_rect_t>("ccv_rect_array");
  register_ccv_array<ccv_comp_t>("ccv_comp_array");
  register_ccv_array<ccv_keypoint_t>("ccv_keypoint_array");
  register_ccv_array<ccv_root_comp_t>("ccv_root_comp_array");
  register_ccv_array<ccv_mser_keypoint_t>("ccv_mser_keypoint_array");
  register_ccv_array<ccv_decimal_point_t>("ccv_decimal_point_array");
  register_ccv_array<ccv_decimal_point_with_status_t>("ccv_decimal_point_with_status_array");

  // TODO: select_overload doesn't work for functions with default args
  function("ccv_read", select_overload<int(val, Ptr<ccv_dense_matrix_t>&, int)>(&ccvjs_read));
  function("ccv_read", select_overload<int(val, Ptr<ccv_dense_matrix_t>&)>(&ccvjs_read));
  function("ccv_write", &ccvjs_write);
  function("ccv_tld_new", &ccvjs_tld_new);
  function("ccv_tld_track_object", &ccvjs_tld_track_object);
  function("ccv_swt_detect_words", &ccvjs_swt_detect_words);
  function("ccv_sift", &ccvjs_sift);
  function("ccv_sift_match", &ccvjs_sift_match);
#ifdef WITH_FILESYSTEM
  function("ccv_scd_classifier_cascade_read", &ccvjs_scd_classifier_cascade_read);
  function("ccv_scd_detect_objects", &ccvjs_scd_detect_objects);
  function("ccv_icf_read_classifier_cascade", &ccvjs_icf_read_classifier_cascade);
  function("ccv_icf_detect_objects", &ccvjs_icf_detect_objects);
  function("ccv_dpm_read_mixture_model", &ccvjs_dpm_read_mixture_model);
  function("ccv_dpm_detect_objects", &ccvjs_dpm_detect_objects);
#endif
  function("ccv_mser", &ccvjs_mser);
  function("ccv_canny", &ccvjs_canny);
  function("ccv_close_outline", &ccvjs_close_outline);
  function("ccv_flip", &ccvjs_flip);
  function("ccv_slice", select_overload<void(const Ptr<ccv_dense_matrix_t>&, Ptr<ccv_dense_matrix_t>&, int, int, int, int, int)>(&ccvjs_slice));
  function("ccv_slice", select_overload<void(const Ptr<ccv_dense_matrix_t>&, Ptr<ccv_dense_matrix_t>&, int, ccv_rect_t)>(&ccvjs_slice));
  function("ccv_blur", &ccvjs_blur);
  function("ccv_sample_down", &ccvjs_sample_down);
  function("ccv_optical_flow_lucas_kanade", select_overload<void(const Ptr<ccv_dense_matrix_t>&, const Ptr<ccv_dense_matrix_t>&, const Ptr<ccv_array_t, ccv_decimal_point_t>&, Ptr<ccv_array_t, ccv_decimal_point_with_status_t>&, ccv_size_t, int, double)>(&ccvjs_optical_flow_lucas_kanade));
  function("ccv_optical_flow_lucas_kanade", select_overload<void(const Ptr<ccv_dense_matrix_t>&, const Ptr<ccv_dense_matrix_t>&, const Ptr<ccv_array_t, ccv_decimal_point_t>&, Ptr<ccv_array_t, ccv_decimal_point_with_status_t>&, ccv_lucas_kanade_param_t)>(&ccvjs_optical_flow_lucas_kanade));


#ifdef WITH_FILESYSTEM
  // Location of the trained models in the emscripten filesystem.
  // For example the build flag "--embed-file external/ccv/samples/face.sqlite3@/" will put that file into the root of the emscripten filesystem.
  std::string CCV_SCD_FACE_FILE = "/face.sqlite3";
  std::string CCV_ICF_PEDESTRIAN_FILE = "/pedestrian.icf";
  std::string CCV_DPM_PEDESTRIAN_FILE = "/pedestrian.m";
  std::string CCV_DPM_CAR_FILE = "/car.m";
  constant("CCV_SCD_FACE_FILE", CCV_SCD_FACE_FILE);
  constant("CCV_ICF_PEDESTRIAN_FILE", CCV_ICF_PEDESTRIAN_FILE);
  constant("CCV_DPM_PEDESTRIAN_FILE", CCV_DPM_PEDESTRIAN_FILE);
  constant("CCV_DPM_CAR_FILE", CCV_DPM_CAR_FILE);
#endif



  constant("CCV_C1", (int)CCV_C1);
  constant("CCV_C2", (int)CCV_C2);
  constant("CCV_C3", (int)CCV_C3);
  constant("CCV_C4", (int)CCV_C4);
  constant("CCV_8U ", (int)CCV_8U);
  constant("CCV_32S", (int)CCV_32S);
  constant("CCV_32F", (int)CCV_32F);
  constant("CCV_64S", (int)CCV_64S);
  constant("CCV_64F", (int)CCV_64F);
  constant("CCV_IO_RGB_COLOR", (int)CCV_IO_RGB_COLOR);
  constant("CCV_IO_GRAY", (int)CCV_IO_GRAY);
  constant("CCV_FLIP_X", (int)CCV_FLIP_X);
  constant("CCV_FLIP_Y", (int)CCV_FLIP_Y);
  constant("CCV_DARK_TO_BRIGHT", (int)CCV_DARK_TO_BRIGHT);
  constant("CCV_BRIGHT_TO_DARK", (int)CCV_BRIGHT_TO_DARK);



  constant("ccv_tld_default_params", ccv_tld_default_params);
  constant("ccv_swt_default_params", ccv_swt_default_params);
  constant("ccv_sift_default_params", ccv_sift_default_params);
#ifdef WITH_FILESYSTEM
  constant("ccv_scd_default_params", ccv_scd_default_params);
  constant("ccv_icf_default_params", ccv_icf_default_params);
  constant("ccv_dpm_default_params", ccv_dpm_default_params);
#endif
  constant("ccv_mser_default_params", ccv_mser_default_params);
  constant("ccv_lucas_kanade_default_params", ccv_lucas_kanade_default_params);




  value_object<ccv_rect_t>("ccv_rect_t")
    .field("x", &ccv_rect_t::x)
    .field("y", &ccv_rect_t::y)
    .field("width", &ccv_rect_t::width)
    .field("height", &ccv_rect_t::height);

  value_object<ccv_point_t>("ccv_point_t")
    .field("x", &ccv_point_t::x)
    .field("y", &ccv_point_t::y);

  value_object<ccv_size_t>("ccv_size_t")
    .field("width", &ccv_size_t::width)
    .field("height", &ccv_size_t::height);

  value_object<ccv_classification_t>("ccv_classification_t")
    .field("id", &ccv_classification_t::id)
    .field("confidence", &ccv_classification_t::confidence);

  value_object<ccv_comp_t>("ccv_comp_t")
    .field("rect", &ccv_comp_t::rect)
    .field("neighbors", &ccv_comp_t::neighbors)
    .field("classification", &ccv_comp_t::classification);

  value_object<ccv_tld_param_t>("ccv_tld_param_t")
    .field("win_size", &ccv_tld_param_t::win_size)
    .field("level", &ccv_tld_param_t::level)
    .field("min_forward_backward_error", &ccv_tld_param_t::min_forward_backward_error)
    .field("min_eigen", &ccv_tld_param_t::min_eigen)
    .field("min_win", &ccv_tld_param_t::min_win)
    .field("interval", &ccv_tld_param_t::interval)
    .field("shift", &ccv_tld_param_t::shift)
    .field("top_n", &ccv_tld_param_t::top_n)
    .field("rotation", &ccv_tld_param_t::rotation)
    .field("include_overlap", &ccv_tld_param_t::include_overlap)
    .field("exclude_overlap", &ccv_tld_param_t::exclude_overlap)
    .field("structs", &ccv_tld_param_t::structs)
    .field("features", &ccv_tld_param_t::features)
    .field("validate_set", &ccv_tld_param_t::validate_set)
    .field("nnc_same", &ccv_tld_param_t::nnc_same)
    .field("nnc_thres", &ccv_tld_param_t::nnc_thres)
    .field("nnc_verify", &ccv_tld_param_t::nnc_verify)
    .field("nnc_beyond", &ccv_tld_param_t::nnc_beyond)
    .field("nnc_collect", &ccv_tld_param_t::nnc_collect)
    .field("bad_patches", &ccv_tld_param_t::bad_patches)
    .field("new_deform", &ccv_tld_param_t::new_deform)
    .field("track_deform", &ccv_tld_param_t::track_deform)
    .field("new_deform_angle", &ccv_tld_param_t::new_deform_angle)
    .field("track_deform_angle", &ccv_tld_param_t::track_deform_angle)
    .field("new_deform_scale", &ccv_tld_param_t::new_deform_scale)
    .field("track_deform_scale", &ccv_tld_param_t::track_deform_scale)
    .field("new_deform_shift", &ccv_tld_param_t::new_deform_shift)
    .field("track_deform_shift", &ccv_tld_param_t::track_deform_shift);
  value_object<ccv_tld_info_t>("ccv_tld_info_t")
    .field("perform_track", &ccv_tld_info_t::perform_track)
    .field("perform_learn", &ccv_tld_info_t::perform_learn)
    .field("track_success", &ccv_tld_info_t::track_success)
    .field("ferns_detects", &ccv_tld_info_t::ferns_detects)
    .field("nnc_detects", &ccv_tld_info_t::nnc_detects)
    .field("clustered_detects", &ccv_tld_info_t::clustered_detects)
    .field("confident_matches", &ccv_tld_info_t::confident_matches)
    .field("close_matches", &ccv_tld_info_t::close_matches);



  register_array<double, 2>("array_double_2"); // For ccv_swt_param_t::same_word_thresh
  value_object<ccv_swt_param_t>("ccv_swt_param_t")
    .field("interval", &ccv_swt_param_t::interval)
    .field("min_neighbors", &ccv_swt_param_t::min_neighbors)
    .field("scale_invariant", &ccv_swt_param_t::scale_invariant)
    .field("direction", &ccv_swt_param_t::direction)
    .field("same_word_thresh", reinterpret_cast<std::array<double, 2> ccv_swt_param_t::*>(&ccv_swt_param_t::same_word_thresh)) // Emscripten doesn't like the type double[2], https://github.com/kripken/emscripten/pull/4510
    .field("size", &ccv_swt_param_t::size)
    .field("low_thresh", &ccv_swt_param_t::low_thresh)
    .field("high_thresh", &ccv_swt_param_t::high_thresh)
    .field("max_height", &ccv_swt_param_t::max_height)
    .field("min_height", &ccv_swt_param_t::min_height)
    .field("min_area", &ccv_swt_param_t::min_area)
    .field("letter_occlude_thresh", &ccv_swt_param_t::letter_occlude_thresh)
    .field("aspect_ratio", &ccv_swt_param_t::aspect_ratio)
    .field("std_ratio", &ccv_swt_param_t::std_ratio)
    .field("thickness_ratio", &ccv_swt_param_t::thickness_ratio)
    .field("height_ratio", &ccv_swt_param_t::height_ratio)
    .field("intensity_thresh", &ccv_swt_param_t::intensity_thresh)
    .field("distance_ratio", &ccv_swt_param_t::distance_ratio)
    .field("intersect_ratio", &ccv_swt_param_t::intersect_ratio)
    .field("elongate_ratio", &ccv_swt_param_t::elongate_ratio)
    .field("letter_thresh", &ccv_swt_param_t::letter_thresh)
    .field("breakdown", &ccv_swt_param_t::breakdown)
    .field("breakdown_ratio", &ccv_swt_param_t::breakdown_ratio);



  /*
  value_object<decltype(ccv_keypoint_t::affine)>("ccv_keypoint_t_affine")
    .field("a", &decltype(ccv_keypoint_t::affine)::a)
    .field("b", &decltype(ccv_keypoint_t::affine)::b)
    .field("c", &decltype(ccv_keypoint_t::affine)::c)
    .field("d", &decltype(ccv_keypoint_t::affine)::d);
  */
  value_object<decltype(ccv_keypoint_t::regular)>("ccv_keypoint_t_regular")
    .field("scale", &decltype(ccv_keypoint_t::regular)::scale)
    .field("angle", &decltype(ccv_keypoint_t::regular)::angle);
  value_object<ccv_keypoint_t>("ccv_keypoint_t")
      .field("x", &ccv_keypoint_t::x)
      .field("y", &ccv_keypoint_t::y)
      .field("octave", &ccv_keypoint_t::octave)
      .field("level", &ccv_keypoint_t::level)
      //.field("affine", &ccv_keypoint_t::affine)
      .field("regular", &ccv_keypoint_t::regular);
  value_object<ccv_sift_param_t>("ccv_sift_param_t")
    .field("up2x", &ccv_sift_param_t::up2x)
    .field("noctaves", &ccv_sift_param_t::noctaves)
    .field("nlevels", &ccv_sift_param_t::nlevels)
    .field("edge_threshold", &ccv_sift_param_t::edge_threshold)
    .field("peak_threshold", &ccv_sift_param_t::peak_threshold)
    .field("norm_threshold", &ccv_sift_param_t::norm_threshold);



#ifdef WITH_FILESYSTEM
  value_object<ccv_scd_param_t>("ccv_scd_param_t")
    .field("min_neighbors", &ccv_scd_param_t::min_neighbors)
    .field("step_through", &ccv_scd_param_t::step_through)
    .field("interval", &ccv_scd_param_t::interval)
    .field("size", &ccv_scd_param_t::size);

  value_object<ccv_icf_param_t>("ccv_icf_param_t")
    .field("min_neighbors", &ccv_icf_param_t::min_neighbors)
    .field("flags", &ccv_icf_param_t::flags)
    .field("step_through", &ccv_icf_param_t::step_through)
    .field("interval", &ccv_icf_param_t::interval)
    .field("threshold", &ccv_icf_param_t::threshold);

  register_array<ccv_comp_t, CCV_DPM_PART_MAX>("comp_array"); // For ccv_root_comp_t::part
  value_object<ccv_root_comp_t>("ccv_root_comp_t")
    .field("rect", &ccv_root_comp_t::rect)
    .field("neighbors", &ccv_root_comp_t::neighbors)
    .field("classification", &ccv_root_comp_t::classification)
    .field("pnum", &ccv_root_comp_t::pnum)
    .field("part", reinterpret_cast<std::array<ccv_comp_t, CCV_DPM_PART_MAX> ccv_root_comp_t::*>(&ccv_root_comp_t::part));
  value_object<ccv_dpm_param_t>("ccv_dpm_param_t")
    .field("interval", &ccv_dpm_param_t::interval)
    .field("min_neighbors", &ccv_dpm_param_t::min_neighbors)
    .field("flags", &ccv_dpm_param_t::flags)
    .field("threshold", &ccv_dpm_param_t::threshold);
#endif



  value_object<ccv_mser_param_t>("ccv_mser_param_t")
    .field("min_area", &ccv_mser_param_t::min_area)
    .field("max_area", &ccv_mser_param_t::max_area)
    .field("min_diversity", &ccv_mser_param_t::min_diversity)
    .field("area_threshold", &ccv_mser_param_t::area_threshold)
    .field("min_margin", &ccv_mser_param_t::min_margin)
    .field("max_evolution", &ccv_mser_param_t::max_evolution)
    .field("edge_blur_sigma", &ccv_mser_param_t::edge_blur_sigma)
    .field("delta", &ccv_mser_param_t::delta)
    .field("max_variance", &ccv_mser_param_t::max_variance)
    .field("direction", &ccv_mser_param_t::direction);
  value_object<ccv_mser_keypoint_t>("ccv_mser_keypoint_t")
    .field("keypoint", &ccv_mser_keypoint_t::keypoint)
    .field("m01", &ccv_mser_keypoint_t::m01)
    .field("m02", &ccv_mser_keypoint_t::m02)
    .field("m10", &ccv_mser_keypoint_t::m10)
    .field("m11", &ccv_mser_keypoint_t::m11)
    .field("m20", &ccv_mser_keypoint_t::m20)
    .field("rect", &ccv_mser_keypoint_t::rect)
    .field("size", &ccv_mser_keypoint_t::size);



  value_object<ccv_lucas_kanade_param_t>("ccv_lucas_kanade_param_t")
    .field("win_size", &ccv_lucas_kanade_param_t::win_size)
    .field("level", &ccv_lucas_kanade_param_t::level)
    .field("min_eigen", &ccv_lucas_kanade_param_t::min_eigen);
  value_object<ccv_decimal_point_t>("ccv_decimal_point_t")
    .field("x", &ccv_decimal_point_t::x)
    .field("y", &ccv_decimal_point_t::y);
  value_object<ccv_decimal_point_with_status_t>("ccv_decimal_point_with_status_t")
    .field("point", &ccv_decimal_point_with_status_t::point)
    .field("status", &ccv_decimal_point_with_status_t::status);
}
