#include <torch/torch.h>
#include <torch/script.h>

#include <torchvision/vision.h>
#include <torchvision/models/resnet.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/opencv.hpp>

#define kIMAGE_SIZE 224
#define kCHANNELS 3
#define kTOP_K 3

bool load_image(cv::Mat &image) {

  //image = cv::imread(file_name);  // CV_8UC3
  if (image.empty() || !image.data) {
    return false;
  }
  cv::cvtColor(image, image, cv::COLOR_BGR2RGB);
 
  int w = image.size().width, h = image.size().height;
  cv::Size scale((int)256*((float)w)/h, 256);
  cv::resize(image, image, scale);
  w = image.size().width, h = image.size().height;
	image = image(cv::Range(16,240), cv::Range(80, 304));
  image.convertTo(image, CV_32FC3, 1.0f / 255.0f);

  return true;
}

int recognition(cv::Mat & image) {

  static bool initialized = false;
  static torch::jit::script::Module module;
  if(!initialized) {
    try {
      std::cout << "Initialize ResNet50 model" << std::endl;
      module = torch::jit::load("./resnet50.pt");
    }
    catch (const c10::Error& e) {
      std::cerr << "error loading the model\n";
      return -1;
    }
    initialized = true;
  }

	if (load_image(image)) {

    auto input_tensor = torch::from_blob(
        image.data, {1, kIMAGE_SIZE, kIMAGE_SIZE, kCHANNELS});
    input_tensor = input_tensor.permute({0, 3, 1, 2});
    //input_tensor = input_tensor.permute({0, 1, 2, 3});
    input_tensor[0][0] = input_tensor[0][0].sub_(0.485).div_(0.229);
    input_tensor[0][1] = input_tensor[0][1].sub_(0.456).div_(0.224);
    input_tensor[0][2] = input_tensor[0][2].sub_(0.406).div_(0.225);

  
    torch::Tensor out_tensor = module.forward({input_tensor}).toTensor();
    auto results = out_tensor.sort(-1, true);
    auto softmaxs = std::get<0>(results)[0].softmax(0);
    auto indexs = std::get<1>(results)[0];
  
    std::cout << indexs[0].item<int>() << " " << softmaxs[0].item<double>() << std::endl;
    return indexs[0].item<int>();
		//for(int i = 0; i < 100; ++i)
		//	std::cout << softmaxs[i].item<double>() << ' ';
		//std::cout << '\n';
		//for(int i = 0; i < 100; ++i)
		//	std::cout << indexs[i].item<int>() << ' ';
		//std::cout << '\n';
	}

  return -1;
}

