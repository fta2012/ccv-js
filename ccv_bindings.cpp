#include <emscripten.h>
#include <emscripten/bind.h>
#include <array>
#include <string>
#include <vector>

extern "C" {
#include <ccv.h>
}

using namespace emscripten;

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
        data[4 * (i * width  + j) + 0] = mdata[i * step + c * j + 0];
        data[4 * (i * width  + j) + 1] = mdata[i * step + c * j + 1];
        data[4 * (i * width  + j) + 2] = mdata[i * step + c * j + 2];
        data[4 * (i * width  + j) + 3] = 255;
      }
    }
  } else {
    unsigned char max = *std::max_element(mdata, mdata + (height - 1) * step + width);
    if (max == 1) { // binary image
      for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
          data[4 * (i * width  + j) + 0] = mdata[i * step + j] ? 255 : 0;
          data[4 * (i * width  + j) + 1] = mdata[i * step + j] ? 255 : 0;
          data[4 * (i * width  + j) + 2] = mdata[i * step + j] ? 255 : 0;
          data[4 * (i * width  + j) + 3] = 255;
        }
      }
    } else { // grayscale image
      for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
          data[4 * (i * width  + j) + 0] = mdata[i * step + j];
          data[4 * (i * width  + j) + 1] = mdata[i * step + j];
          data[4 * (i * width  + j) + 2] = mdata[i * step + j];
          data[4 * (i * width  + j) + 3] = 255;
        }
      }
    }
  }
}

int ccv_write_html(const val& source, ccv_dense_matrix_t* matrix) { 
	int width = matrix->cols;
	int height = matrix->rows;
	unsigned char* data = (unsigned char*)malloc(4 * width * height);
	_ccv_write_rgba_raw(matrix, data);
	val view = val(typed_memory_view(4 * width * height, data));
  val::module_property("writeImageData")(source, view, width, height);
  free(data);
	return 0;
}

int ccv_read_html(val v, ccv_dense_matrix_t ** image, int type) {
  val imageData = val::module_property("readImageData")(v);
  std::string s = imageData["data"]["buffer"].as<std::string>(); // TODO: IE's imageData.data.buffer isn't a Uint8Array
  int width = imageData["width"].as<int>();
  int height = imageData["height"].as<int>();
  assert(type == CCV_IO_GRAY || type == CCV_IO_RGB_COLOR);
  ccv_read(s.c_str(), image, CCV_IO_RGBA_RAW | type, height, width, width * 4);
  return 0;
}

template<typename T>
class CCVArray {
public:
  CCVArray() {
		array = ccv_array_new(sizeof(T), 0, 0);
  }

  CCVArray(ccv_array_t * array) : array(array) {}

  int size() {
    return array->rnum;
  }

	void push(const T& x) {
		ccv_array_push(array, &x);
	}

  val get(int i) {
    return val(*(T*)ccv_array_get(array, i));
  }
  
  val toJS() {
    val jsArray = val::array();
    for (int i = 0; i < array->rnum; i++) {
      jsArray.call<void>("push", get(i));
    }
    return jsArray;
  }

private:
  ccv_array_t* array;
};

class CCVImage {
public:
  CCVImage(val source, int type=CCV_IO_GRAY) {
    ccv_read_html(source, &image, type);
  }

  ~CCVImage() {
    if (image) {
      ccv_matrix_free(image);
    }
  }

  CCVImage& write(const val& v) {
    ccv_write_html(v, image);
    return *this;
  }

  int getWidth() {
    return image->cols;
  }

  int getHeight() {
    return image->rows;
  }

  std::string getDataType() {
    switch(CCV_GET_DATA_TYPE(image->type)) {
      case CCV_8U: 
        return "CCV_8U";
      case CCV_32S:
        return "CCV_32S";
      case CCV_32F:
        return "CCV_32F";
      case CCV_64S:
        return "CCV_64S";
      case CCV_64F:
        return "CCV_64F";
      default:
        return "UNKNOWN";
    }
  }

  int getNumChannels() {
    return CCV_GET_CHANNEL(image->type);
  }

	CCVImage& canny(int size, double low_thresh, double high_thresh) {
    ccv_dense_matrix_t* canny;
    ccv_canny(image, &canny, 0, size, low_thresh, high_thresh);
    ccv_matrix_free(image);
    image = canny;
		return *this;
	}

  CCVImage& flip() {
		ccv_dense_matrix_t* out;
    ccv_flip(image, &out, 0, CCV_FLIP_X);
		ccv_matrix_free(image);
		image = out;
		return *this;
  }
  
  CCVImage& blur(double sigma) {
    ccv_blur(image, &image, 0, sigma);
		return *this;
  }

private:
  ccv_dense_matrix_t* image = nullptr;
};


class TLDTracker {
public:
  TLDTracker() {}

  ~TLDTracker() {
    if (tld) {
      ccv_tld_free(tld);
			tld = nullptr;
      ccv_matrix_free(prevFrame);
			prevFrame = nullptr;
    }
  }

  void init(val imageData, const ccv_rect_t& box, const ccv_tld_param_t& params) {
    if (tld) {
      ccv_tld_free(tld);
			tld = nullptr;
      ccv_matrix_free(prevFrame);
			prevFrame = nullptr;
    }
		ccv_read_html(imageData, &prevFrame, CCV_IO_GRAY);
    tld = ccv_tld_new(prevFrame, box, params);
  }

  void track(val imageData, val callback) {
    if (!tld) {
      //printf("No initial point for tracker yet\n");
      return;
    }
    ccv_dense_matrix_t* frame;
		ccv_read_html(imageData, &frame, CCV_IO_GRAY);
    ccv_tld_info_t info;
    ccv_comp_t newbox = ccv_tld_track_object(tld, prevFrame, frame, &info);
    ccv_matrix_free(prevFrame);
    prevFrame = frame;
    callback(info, tld->box, CCVArray<ccv_comp_t>(tld->top).toJS());
  }

private:
  ccv_dense_matrix_t* prevFrame = nullptr;
  ccv_tld_t* tld = nullptr;
};

int main() {
  ccv_enable_default_cache();

  EM_ASM({
    console.log('ready', Module);
  });
}


template<typename T>
class_<CCVArray<T>> registerCCVArray(const char* name) {
  return class_<CCVArray<T>>(name)
    .constructor()
    .function("get", &CCVArray<T>::get)
    .function("push", &CCVArray<T>::push)
    .function("size", &CCVArray<T>::size)
    .function("toJS", &CCVArray<T>::toJS);
}


EMSCRIPTEN_BINDINGS(ccv_js_module) {
  class_<CCVImage>("CCVImage")
    .constructor<val>()
    .constructor<val, int>()
    .function("getWidth", &CCVImage::getWidth)
    .function("getHeight", &CCVImage::getHeight)
    .function("getDataType", &CCVImage::getDataType)
    .function("getNumChannels", &CCVImage::getNumChannels)
    .function("write", &CCVImage::write)
    .function("canny", &CCVImage::canny)
    .function("flip", &CCVImage::flip)
    .function("blur", &CCVImage::blur);

  class_<TLDTracker>("TLDTracker")
    .constructor()
    .function("init", &TLDTracker::init)
    .function("track", &TLDTracker::track);

  registerCCVArray<ccv_comp_t>("CCVCompArray");

  constant("ccv_tld_default_params", ccv_tld_default_params);

  value_object<ccv_rect_t>("ccv_rect_t")
    .field("x", &ccv_rect_t::x)
    .field("y", &ccv_rect_t::y)
    .field("width", &ccv_rect_t::width)
    .field("height", &ccv_rect_t::height);
  
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

}
