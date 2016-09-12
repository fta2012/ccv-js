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

// Reverse of _ccv_read_rgba_raw
// TODO: rewrite more generically and move in ccv/lib/io/_ccv_io_raw.c
void _ccv_write_rgba_raw(ccv_dense_matrix_t* x, unsigned char* data) {
  int rows = x->rows;
  int cols = x->cols;
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
    if (max == 1) { // binary image
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

int ccv_write_html(const val& source, ccv_dense_matrix_t* matrix) {
  int width = matrix->cols;
  int height = matrix->rows;
  unsigned char* data = (unsigned char*)malloc(4 * width * height); // TODO: this shouldn't be necessary
  _ccv_write_rgba_raw(matrix, data);
  val view = val(typed_memory_view(4 * width * height, data));
  val::module_property("writeImageData")(source, view, width, height);
  free(data);
  return 0;
}

int ccv_read_html(val v, ccv_dense_matrix_t** image, int type) {
  val imageData = val::module_property("readImageData")(v);
  std::string s = imageData["data"]["buffer"].as<std::string>(); // TODO: IE's imageData.data.buffer isn't a Uint8Array
  int width = imageData["width"].as<int>();
  int height = imageData["height"].as<int>();
  assert(type == CCV_IO_GRAY || type == CCV_IO_RGB_COLOR);
  ccv_read(s.c_str(), image, CCV_IO_RGBA_RAW | type, height, width, width * 4);
  return 0;
}



template<typename T>
ccv_array_t* ccv_array_concat_js_array(ccv_array_t* array, const val& jsArray) {
  int length = jsArray["length"].as<int>();
  for (int i = 0; i < length; i++) {
    T temp = jsArray[i].as<T>();
    ccv_array_push(array, &temp);
  }
  return array;
}

template<typename T>
ccv_array_t* ccv_array_from_js_array(const val& jsArray) {
  int length = jsArray["length"].as<int>();
  ccv_array_t* array = ccv_array_new(sizeof(T), length, 0);
  return ccv_array_concat_js_array<T>(array, jsArray);
}

template<typename T>
val ccv_array_to_js(ccv_array_t* array) {
  val jsArray = val::array();
  for (int i = 0; i < array->rnum; i++) {
    jsArray.call<void>("push", val(*(T*)ccv_array_get(array, i)));
  }
  return jsArray;
}


// TODO: Move this into ccv
void ccv_grayscale(ccv_dense_matrix_t* a, ccv_dense_matrix_t** b) {
  assert(CCV_GET_DATA_TYPE(a->type) == CCV_8U);
  assert(CCV_GET_CHANNEL(a->type) == CCV_C3);
  ccv_declare_derived_signature(sig, a->sig != 0, ccv_sign_with_literal("ccv_grayscale"), a->sig, CCV_EOF_SIGN);
  // Note: converts type to CCV_C1 in addition to grayscaling
  ccv_dense_matrix_t* db = *b = ccv_dense_matrix_renew(*b, a->rows, a->cols, CCV_ALL_DATA_TYPE | CCV_GET_CHANNEL(a->type), CCV_GET_DATA_TYPE(a->type) | CCV_C1, sig);
  ccv_object_return_if_cached(, db);
  int i, j;
  unsigned char* aptr = a->data.u8;
  unsigned char* bptr = db->data.u8;
  for (i = 0; i < a->rows; i++) {
    for (j = 0; j < a->cols; j++) {
      bptr[j] = (unsigned char)((aptr[3 * j] * 6969 + aptr[3 * j + 1] * 23434 + aptr[3 * j + 2] * 2365) >> 15);
    }
    aptr += a->step;
    bptr += db->step;
  }
}


const ccv_mser_param_t ccv_mser_default_params = {
  .min_area = 60,
  .max_area = 100000,//(int)(image->rows * image->cols * 0.3 + 0.5),
  .min_diversity = 0.2,
  .area_threshold = 1.01,
  .min_margin = 0.003,
  .max_evolution = 200,
  .edge_blur_sigma = sqrt(3.0),
  .delta = 5,
  .max_variance = 0.25,
  .direction = CCV_DARK_TO_BRIGHT,
};

typedef struct {
  ccv_size_t win_size; /**< The window size to compute optical flow. */
  int level; /**< Level of image pyramids */
  float min_eigen; /**< The minimal eigenvalue for a valid optical flow computation */
} ccv_lucas_kanade_param_t;
const ccv_lucas_kanade_param_t ccv_lucas_kanade_default_params = {
  .win_size = {
    .width = ccv_tld_default_params.win_size.width,
    .height = ccv_tld_default_params.win_size.height
  },
  .level = ccv_tld_default_params.level,
  .min_eigen = ccv_tld_default_params.min_eigen,
};


class CCVImage {
public:
  CCVImage(const CCVImage& other) = delete;
  CCVImage& operator=(const CCVImage& other) = delete;
  CCVImage(const CCVImage&& other) = delete;
  CCVImage& operator=(const CCVImage&& other) = delete;
  ~CCVImage() {}

  explicit CCVImage(const val& source, int type=0) {
    assert(!type || type == CCV_IO_GRAY || type == CCV_IO_RGB_COLOR);
    if (source.hasOwnProperty("$$")) { // TODO: check that source.$$.ptrType.name == 'CCVImage'
      const CCVImage& other = source.as<const CCVImage&>();
      assert(CCV_GET_DATA_TYPE(other.imagePtr->type) == CCV_8U);
      int channelType = CCV_GET_CHANNEL(other.imagePtr->type);
      if (!type) {
        type = channelType; // if not explicitly asked to convert, keep same type when copying
      }
      if ((channelType == CCV_C3 && type == CCV_IO_RGB_COLOR) ||
          (channelType == CCV_C1 && type == CCV_IO_GRAY)) {
        // Both have same IO type
        imagePtr = other.imagePtr;
      } else if (channelType == CCV_C3 && type == CCV_IO_GRAY) {
        // source is colored but want gray
        imagePtr = other.imagePtr;
        ccv_dense_matrix_t* out = nullptr;
        ccv_grayscale(imagePtr.get(), &out);
        imagePtr.reset(out, &ccv_matrix_free);
      } else {
        assert(false);
      }
    } else {
      if (!type) {
        type = CCV_IO_GRAY;
      }
      ccv_dense_matrix_t* temp = nullptr;
      ccv_read_html(source, &temp, type);
      imagePtr.reset(temp, &ccv_matrix_free);
    }
  }

  int getWidth() const {
    return imagePtr->cols;
  }

  int getHeight() const {
    return imagePtr->rows;
  }

  CCVImage* write(const val& v) {
    ccv_write_html(v, imagePtr.get());
    return this;
  }

  CCVImage* swtDetect(const ccv_swt_param_t& param, val callback) {
    ccv_array_t* words = ccv_swt_detect_words(imagePtr.get(), param);
    callback(ccv_array_to_js<ccv_rect_t>(words));
    ccv_array_free(words);
    return this;
  }

  CCVImage* siftMatch(const CCVImage& object_image, const ccv_sift_param_t& params, val callback) {
    // From ccv/bin/siftmatch.c
    ccv_dense_matrix_t* image = imagePtr.get();
    ccv_dense_matrix_t* object = object_image.imagePtr.get();
    ccv_array_t* obj_keypoints = 0;
    ccv_dense_matrix_t* obj_desc = 0;
    ccv_sift(object, &obj_keypoints, &obj_desc, 0, params);
    ccv_array_t* image_keypoints = 0;
    ccv_dense_matrix_t* image_desc = 0;
    ccv_sift(image, &image_keypoints, &image_desc, 0, params);
    int i, j, k;
    val matches = val::array();
    for (i = 0; i < obj_keypoints->rnum; i++)
    {
      float* odesc = obj_desc->data.f32 + i * 128;
      int minj = -1;
      double mind = 1e6, mind2 = 1e6;
      for (j = 0; j < image_keypoints->rnum; j++)
      {
        float* idesc = image_desc->data.f32 + j * 128;
        double d = 0;
        for (k = 0; k < 128; k++)
        {
          d += (odesc[k] - idesc[k]) * (odesc[k] - idesc[k]);
          if (d > mind2)
            break;
        }
        if (d < mind)
        {
          mind2 = mind;
          mind = d;
          minj = j;
        } else if (d < mind2) {
          mind2 = d;
        }
      }
      if (mind < mind2 * 0.36)
      {
        ccv_keypoint_t* op = (ccv_keypoint_t*)ccv_array_get(obj_keypoints, i);
        ccv_keypoint_t* kp = (ccv_keypoint_t*)ccv_array_get(image_keypoints, minj);
        val pair = val::array();
        pair.call<void>("push", minj);
        pair.call<void>("push", i);
        matches.call<void>("push", pair);
      }
    }
    callback(matches, ccv_array_to_js<ccv_keypoint_t>(image_keypoints), ccv_array_to_js<ccv_keypoint_t>(obj_keypoints));
    ccv_array_free(obj_keypoints);
    ccv_array_free(image_keypoints);
    ccv_matrix_free(obj_desc);
    ccv_matrix_free(image_desc);
    return this;
  }

#ifdef WITH_FILESYSTEM
  // You need to compile with "--embed-file external/ccv/samples/face.sqlite3@/" which will put the file at "/face.sqlite3" in emscripten's filesystem

  CCVImage* scdDetect(const ccv_scd_param_t& param, val callback) {
    ccv_scd_classifier_cascade_t* cascade = ccv_scd_classifier_cascade_read("/face.sqlite3");
    assert(cascade);
    ccv_array_t* seq = ccv_scd_detect_objects(imagePtr.get(), &cascade, 1, ccv_scd_default_params);
    callback(ccv_array_to_js<ccv_rect_t>(seq));
    ccv_array_free(seq);
    ccv_scd_classifier_cascade_free(cascade);
    return this;
  }

  CCVImage* icfDetect(const ccv_icf_param_t& param, val callback) {
    ccv_dense_matrix_t* image = imagePtr.get();
    ccv_icf_classifier_cascade_t* cascade = ccv_icf_read_classifier_cascade("/pedestrian.icf");
    ccv_array_t* seq = ccv_icf_detect_objects(image, &cascade, 1, ccv_icf_default_params);
    callback(ccv_array_to_js<ccv_comp_t>(seq));
    ccv_array_free(seq);
    ccv_icf_classifier_cascade_free(cascade);
    return this;
  }

  CCVImage* dpmDetect(const ccv_dpm_param_t& param, val callback, int model_type = 0/*0: pedestrian, 1: car*/) {
    ccv_dense_matrix_t* image = imagePtr.get();
    ccv_dpm_mixture_model_t* model;
    if (model_type) {
      model = ccv_dpm_read_mixture_model("/car.m");
    } else {
      model = ccv_dpm_read_mixture_model("/pedestrian.m");
    }
    ccv_array_t* seq = ccv_dpm_detect_objects(image, &model, 1, ccv_dpm_default_params);
    if (seq) {
      callback(ccv_array_to_js<ccv_root_comp_t>(seq));
      ccv_array_free(seq);
    } else {
      callback(val::array());
    }
    ccv_dpm_mixture_model_free(model);
    return this;
  }
#endif

  CCVImage* mser(const CCVImage& outline, const ccv_mser_param_t& params, val callback) {
    ccv_dense_matrix_t* yuv = imagePtr.get();
    // ccv_color_transform(image, &yuv, 0, CCV_RGB_TO_YUV);
    ccv_dense_matrix_t* mser = 0;
    ccv_array_t* mser_keypoint = ccv_mser(yuv, outline.imagePtr.get(), &mser, 0, params);
    callback(
      ccv_array_to_js<ccv_mser_keypoint_t>(mser_keypoint),
      val(typed_memory_view(4 * mser->cols * mser->rows, mser->data.u8)),
      mser->cols,
      mser->rows
    );
    ccv_array_free(mser_keypoint);
    ccv_matrix_free(mser);
    return this;
  }

  CCVImage* closeOutline() {
    ccv_dense_matrix_t* out = nullptr;
    ccv_close_outline(imagePtr.get(), &out, 0);
    imagePtr.reset(out, &ccv_matrix_free);
    return this;
  }

  CCVImage* canny(int size, double low_thresh, double high_thresh) {
    ccv_dense_matrix_t* canny = nullptr;
    ccv_canny(imagePtr.get(), &canny, 0, size, low_thresh, high_thresh);
    imagePtr.reset(canny, &ccv_matrix_free);
    return this;
  }

  CCVImage* flipX() {
    ccv_dense_matrix_t* out = nullptr;
    ccv_flip(imagePtr.get(), &out, 0, CCV_FLIP_X);
    imagePtr.reset(out, &ccv_matrix_free);
    return this;
  }

  CCVImage* slice(ccv_rect_t rect) {
    ccv_dense_matrix_t* out = nullptr;
    ccv_slice(imagePtr.get(), (ccv_matrix_t**)&out, 0, rect.y, rect.x, rect.height, rect.width);
    imagePtr.reset(out, &ccv_matrix_free);
    return this;
  }

  CCVImage* blur(double sigma) {
    ccv_dense_matrix_t* out = nullptr;
    ccv_blur(imagePtr.get(), &out, 0, sigma);
    imagePtr.reset(out, &ccv_matrix_free);
    return this;
  }


  std::shared_ptr<ccv_dense_matrix_t> imagePtr;
};



struct TLDDeleter {
  void operator()(ccv_tld_t* ptr) {
    ccv_tld_free(ptr);
  }
};
class TLDTracker {
public:
  TLDTracker(const TLDTracker& other) = delete;
  TLDTracker& operator=(const TLDTracker& other) = delete;
  TLDTracker(const TLDTracker&& other) = delete;
  TLDTracker& operator=(const TLDTracker&& other) = delete;
  ~TLDTracker() {}

  TLDTracker(const CCVImage& ccv_image, const ccv_rect_t& box, const ccv_tld_param_t& params = ccv_tld_default_params) {
    prevFrame = ccv_image.imagePtr;
    tld.reset(ccv_tld_new(prevFrame.get(), box, params));
  }

  void track(const CCVImage& ccv_image, val callback) {
    ccv_tld_info_t info;
    ccv_comp_t newbox = ccv_tld_track_object(tld.get(), prevFrame.get(), ccv_image.imagePtr.get(), &info);
    prevFrame = ccv_image.imagePtr;
    callback(info, tld->box, ccv_array_to_js<ccv_comp_t>(tld->top));
  }

private:
  std::shared_ptr<ccv_dense_matrix_t> prevFrame;
  std::unique_ptr<ccv_tld_t, TLDDeleter> tld;
};


struct CCVArrayDeleter {
  void operator()(ccv_array_t* ptr) {
    ccv_array_free(ptr);
  }
};
class LucasKanadeTracker {
public:
  LucasKanadeTracker(const LucasKanadeTracker& other) = delete;
  LucasKanadeTracker& operator=(const LucasKanadeTracker& other) = delete;
  LucasKanadeTracker(const LucasKanadeTracker&& other) = delete;
  LucasKanadeTracker& operator=(const LucasKanadeTracker&& other) = delete;
  ~LucasKanadeTracker() {}

  LucasKanadeTracker(const ccv_lucas_kanade_param_t& p = ccv_lucas_kanade_default_params) : param(p) {}
  void track(const CCVImage& ccv_image, val callback) {
    if (prevPoints && prevPoints->rnum) {
      if (!prevFrame) {
        prevFrame = ccv_image.imagePtr;
      }
      const std::shared_ptr<ccv_dense_matrix_t>& frame = ccv_image.imagePtr;
      ccv_array_t* pointsWithStatus = 0;
      ccv_optical_flow_lucas_kanade(prevFrame.get(), frame.get(), prevPoints.get(), &pointsWithStatus, param.win_size, param.level, param.min_eigen);
      callback(
        ccv_array_to_js<ccv_decimal_point_t>(prevPoints.get()),
        ccv_array_to_js<ccv_decimal_point_with_status_t>(pointsWithStatus)
      );

      // Filter out points with status == 0
      ccv_array_t* points = ccv_array_new(sizeof(ccv_decimal_point_t), 0, 0);
      int numPoints = pointsWithStatus->rnum;
      for (int i = 0; i < pointsWithStatus->rnum; i++) {
        ccv_decimal_point_with_status_t* p = (ccv_decimal_point_with_status_t*)ccv_array_get(pointsWithStatus, i);
        if (p->status) {
          ccv_array_push(points, &p->point);
        }
      }
      prevPoints.reset(points);
    } else {
      callback(val::array(), val::array());
    }
    prevFrame = ccv_image.imagePtr;
  }

  void addPoints(const val& additionalPoints) {
    if (prevPoints) {
      ccv_array_concat_js_array<ccv_decimal_point_t>(prevPoints.get(), additionalPoints);
    } else {
      ccv_array_t* points = ccv_array_from_js_array<ccv_decimal_point_t>(additionalPoints);
      prevPoints.reset(points);
    }
  }

private:
  std::shared_ptr<ccv_dense_matrix_t> prevFrame;
  std::unique_ptr<ccv_array_t, CCVArrayDeleter> prevPoints;
  ccv_lucas_kanade_param_t param;
};


int main() {
  ccv_enable_default_cache();

  EM_ASM({
    //console.log('ready', Module);
  });
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
  // TODO: Wanted method chaining for CCVImage so initially had each method return a reference.
  // But embind will then create a copy of that reference for returning to the JS layer.
  // https://github.com/kripken/emscripten/issues/3480
  //
  // Returning pointers is syntactically the same in JS and gets around the copying problem.
  class_<CCVImage>("CCVImage")
    .constructor<val>()
    .constructor<val, int>()
    .function("getWidth", &CCVImage::getWidth)
    .function("getHeight", &CCVImage::getHeight)
    .function("write", &CCVImage::write, allow_raw_pointers())
    .function("swtDetect", &CCVImage::swtDetect, allow_raw_pointers())
    .function("siftMatch", &CCVImage::siftMatch, allow_raw_pointers())
#ifdef WITH_FILESYSTEM
    .function("scdDetect", &CCVImage::scdDetect, allow_raw_pointers())
    .function("icfDetect", &CCVImage::icfDetect, allow_raw_pointers())
    .function("dpmDetect", &CCVImage::dpmDetect, allow_raw_pointers())
#endif
    .function("mser", &CCVImage::mser, allow_raw_pointers())
    .function("canny", &CCVImage::canny, allow_raw_pointers())
    .function("closeOutline", &CCVImage::closeOutline, allow_raw_pointers())
    .function("flipX", &CCVImage::flipX, allow_raw_pointers())
    .function("slice", &CCVImage::slice, allow_raw_pointers())
    .function("blur", &CCVImage::blur, allow_raw_pointers());

  class_<TLDTracker>("TLDTracker")
    .constructor<const CCVImage&, const ccv_rect_t&>()
    .constructor<const CCVImage&, const ccv_rect_t&, const ccv_tld_param_t&>()
    .function("track", &TLDTracker::track);

  class_<LucasKanadeTracker>("LucasKanadeTracker")
    .constructor()
    .constructor<const ccv_lucas_kanade_param_t&>()
    .function("track", &LucasKanadeTracker::track)
    .function("addPoints", &LucasKanadeTracker::addPoints);


  /*
  enum_<decltype(CCV_C1)>("enumChannels")
    .value("CCV_C1", CCV_C1)
    .value("CCV_C2", CCV_C2)
    .value("CCV_C3", CCV_C3)
    .value("CCV_C4", CCV_C4);

  enum_<decltype(CCV_8U)>("enumDataType")
    .value("CCV_8U ", CCV_8U)
    .value("CCV_32S", CCV_32S)
    .value("CCV_32F", CCV_32F)
    .value("CCV_64S", CCV_64S)
    .value("CCV_64F", CCV_64F);

  enum_<decltype(CCV_IO_GRAY)>("enumIO")
    .value("CCV_IO_RGB_COLOR", CCV_IO_RGB_COLOR)
    .value("CCV_IO_GRAY", CCV_IO_GRAY);
  */
  constant("CCV_IO_RGB_COLOR", (int)CCV_IO_RGB_COLOR);
  constant("CCV_IO_GRAY", (int)CCV_IO_GRAY);


  constant("ccv_tld_default_params", ccv_tld_default_params);
  constant("ccv_lucas_kanade_default_params", ccv_lucas_kanade_default_params);
  constant("ccv_swt_default_params", ccv_swt_default_params);
  constant("ccv_sift_default_params", ccv_sift_default_params);
  constant("ccv_scd_default_params", ccv_scd_default_params);
  constant("ccv_icf_default_params", ccv_icf_default_params);
  constant("ccv_dpm_default_params", ccv_dpm_default_params);
  constant("ccv_mser_default_params", ccv_mser_default_params);


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
