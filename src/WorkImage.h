#pragma once

#include <iostream>
#include <algorithm>
#include <set>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include "opencv2/objdetect/objdetect.hpp"

using namespace cv;

class WorkImage
{

public:

    WorkImage( std::string path );
    void compute();
    void release();

    Mat image;
    int height,width,area,channel;
    Mat full_image;
    Mat gray_image;
    Mat threshold_image;
    bool ok;
    // resize factor : factor applied to resize the image to 500 px width, if applied (image too big)
    double factor;

};
