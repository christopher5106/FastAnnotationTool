#pragma once
#include <string.h>
#include <fstream>
#include "WorkImage.h"
#include <algorithm>

using namespace std;
using std::string;
using std::cout;
using std::endl;

const Scalar white = Scalar(255,255,255);
const Scalar blue = Scalar(255,0,0);
const Scalar green = Scalar(0,255,0);
const Scalar red = Scalar(0,0,255);
const Scalar yellow = Scalar(0, 255, 255);

const char legal[] = {'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z','0','1','2','3','4','5','6','7','8','9'};

const char driving_letters[] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z'};

#define PI 3.14159265

#include <unistd.h>

std::string getexepath();
std::string getAbsolutePath( string path);
string getbase(string csv_filename);
string getbasename(string csv_filename);

// read image
Mat readI ( string path );
void readCSV( const char * csv_file, std::vector<std::vector<std::string> > & input) ;
// get rectangles for each unique input_path
void group_by_image( std::vector<std::vector<std::string> > input, bool rotated, bool correct,float ratio, std::vector<std::string> & input_path, std::vector<std::vector<string> > &input_labels, std::vector<std::vector<RotatedRect> > &input_rotatedrects  ) ;
void findRectangle( std::string image_path, std::string csvfile, vector<Rect> &outputRects) ;

// process image
int compute_orientation(Mat full_image, string output_path = "");
Mat multiplyBy(Mat image, float factor);
void detectRectsAndContours(CascadeClassifier * cc, WorkImage image, vector<cv::Rect> & plateZone, vector<vector<cv::Point> > & all_contours);
void extractRect(  Mat image, Rect & plateZone,vector<vector<cv::Point> > & all_contours, vector<vector<cv::Point> > & orderedContours, vector<cv::Rect> & orderedRects) ;
Mat resizeContains( Mat image, int cols, int rows, int & left, int & right, int & up, int & down, bool noise=false  );

// noise function
void add_salt_pepper_noise(Mat &srcArr, float pa, float pb , RNG *rng);
void add_gaussian_noise(Mat &srcArr,double mean,double sigma, RNG *rng);

//OCR
bool isLegal(char c);
int char2Class(char c);
void most_probable_month(std::vector<float> output1, std::vector<float> output2, string &month, float & month_proba ) ;
void most_probable_year(std::vector<float> output1, std::vector<float> output2, std::vector<float> output3, std::vector<float> output4, string &year, float & year_proba);
void compute_lines_knn(std::vector<Point> letter_points, int nb_lines, vector< vector<Point> > &ordered_lines, vector< float > &ordered_lines_y);
void compute_lines(std::vector<Point> letter_points, int nb_lines, vector< vector<Point> > &ordered_lines, vector< float > &ordered_lines_y) ;

//display functions
void displayRects(Mat image, vector<cv::Rect> plateZone, Scalar color1 = Scalar(110,239,23));
void displayText( Mat image, string text , cv::Point textOrg = cv::Point (10,50), double fontScale = 1 );
void displayRectangle (Mat image, cv::Rect r, Scalar color1 = Scalar(110,239,23));
void displayRotatedRectangle(Mat image, RotatedRect r, Scalar color1 = Scalar(110,239,23));
void displayCross (Mat image, Point2f p, Scalar color1 = Scalar(110,239,23));

//rect functions
int getCenterX(cv::Rect r);
int getCenterY(cv::Rect r);
bool inside(cv::Rect bR, cv::Rect plateZone);
bool is_in_image(Rect r, Mat img) ;
bool is_in_image(RotatedRect r, Mat img) ;
Point2f change_ref ( Point2f p, float center_x, float center_y, float orientation);
bool is_in( Point p, RotatedRect rr );
float intersectionOverUnion( RotatedRect r1, RotatedRect r2);
Mat extractRotatedRect( Mat src, RotatedRect rect );
void correct_ratio ( Rect & r, double ratio ) ;
Size2f correct_ratio ( float width, float height, double ratio ) ;

//others
Mat createOne(vector<Mat> & images, int cols, int rows, int min_gap_size, int dim);
void standard_deviation(vector<int>  data, double & mean, double & stdeviation,double & median);
double getOrientation(vector<cv::Point> &pts, Mat &img);

bool PairCompare(const std::pair<float, int>& lhs,const std::pair<float, int>& rhs) ;
std::vector<int> Argmax(const std::vector<float>& v, int N);

template <typename T> int sgn(T val) {
    return (T(0) < val) - (val < T(0));
}
