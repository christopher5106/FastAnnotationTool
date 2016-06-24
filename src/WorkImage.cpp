#include "WorkImage.h"

WorkImage::WorkImage (std::string path)
{
    full_image = imread(path, CV_LOAD_IMAGE_COLOR);   // Read the file
    if(! full_image.data )                              // Check for invalid input
    {
        std::cout <<  "Could not open or find the image " << path << std::endl ;
        ok = false;
        return ;
    } else ok = true;

    compute();
}

void WorkImage::compute()
{
  height = full_image.rows, width = full_image.cols, area = height * width, channel = full_image.channels();

  factor = full_image.rows / 500;

  if(factor > 1.5) {
    factor = 1 / factor;
    resize(full_image,image,Size(),factor,factor,INTER_AREA);
    height = image.rows, width = image.cols, area = height * width, channel = image.channels();
  } else {
    factor = 1.0;
    image = full_image;
  }

  cvtColor(image,gray_image,CV_BGR2GRAY);
  adaptiveThreshold(gray_image,threshold_image,255,ADAPTIVE_THRESH_MEAN_C,THRESH_BINARY,11,12);
  // threshold(gray_image, threshold_image, 100.0, 255, 0);
}

void WorkImage::release()
{
  image.release();
  gray_image.release();
  threshold_image.release();
  full_image.release();
}
