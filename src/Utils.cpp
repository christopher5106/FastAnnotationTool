#include "Utils.h"


std::string getexepath()
{
  char the_path[256];
  getcwd(the_path, 255);
  strcat(the_path, "/");
  return string(the_path);
}

std::string getAbsolutePath( string path) {
  char actualpath [PATH_MAX];
  char *ptr = realpath(path.c_str(), actualpath) ;
  return std::string(actualpath);
}

string getbase(string csv_filename) {
  std::string base_filename = string(csv_filename).substr(0,string(csv_filename).find_last_of("/\\") + 1);
  if(base_filename.length()==0)
    base_filename = getexepath();
  // cout << "exe path " << getexepath() << endl;
  return base_filename;
}

string getbasename(string csv_filename) {
  return csv_filename.substr(csv_filename.find_last_of("/\\") + 1, csv_filename.length());
}

// read image
Mat readI ( string path ) {
  Mat image = imread(path, CV_LOAD_IMAGE_COLOR);   // Read the file
  int height = image.rows, width = image.cols, area = height * width, channel = image.channels();
  double factor = image.rows / 500;
  if(factor > 1.5) {
    factor = 1 / factor;
    resize(image,image,Size(),factor,factor,INTER_AREA);
  }
  return image;
}


void readCSV( const char * csv_file, std::vector<std::vector<std::string> > & input) {
  std::ifstream csv_f(csv_file, std::ios::in | std::ios::binary);
  std::string str;

  while (std::getline(csv_f, str))
  {
    // Process str
    std::stringstream ss( str );
    std::vector<string> result;

    while( ss.good() )
    {
      std::string substr;
      getline( ss, substr, ',' );
      result.push_back( substr );
    }

    input.push_back( result );
  }

}

void group_by_image( std::vector<std::vector<std::string> > input, bool rotated, bool correct,float ratio, std::vector<std::string> & input_path, std::vector<std::vector<string> > &input_labels, std::vector<std::vector<RotatedRect> > &input_rotatedrects  ) {
  for(int i = 0 ; i < input.size(); i++) {
    // compute index
    int index = -1;
    for(int j = 0; j < input_path.size(); j++)
      if(  input_path[j] ==  input[i][0] )
        index = j;
    if( index == -1 ) {
      input_path.push_back( input[i][0] );
      std::vector<string> vec_int ;
      std::vector<RotatedRect> vec_rotatedrect;
      input_rotatedrects.push_back( vec_rotatedrect );
      input_labels.push_back( vec_int  );
      index = input_path.size() -1;
    }
    // cout << "index : " << index << endl;

    // take orientation into account or not
    int orient = 0;
    if( rotated ) orient = stoi(input[i][6]);
    // cout << "Orientation : " << orient << endl;
    Size2f s;
    if( correct  )
      s = correct_ratio(stoi(input[i][4]), stoi(input[i][5]), ratio);
    else
      s = Size2f(stoi(input[i][4]), stoi(input[i][5]));
    input_rotatedrects[index].push_back( RotatedRect ( Point2f(stoi(input[i][2]) , stoi(input[i][3]) ) , s, orient ) );

    input_labels[index].push_back( input[i][1] );
  }
}

void findRectangle( std::string image_path, std::string csvfile, vector<Rect> &outputRects) {
  std::ifstream file( csvfile );
  std::string str;
  while (std::getline(file, str))
  {
      std::stringstream ss( str );
      vector<string> result;

      while( ss.good() )
      {
        string substr;
        getline( ss, substr, ',' );
        result.push_back( substr );
      }
      if( result[0] == image_path )
        outputRects.push_back( Rect(stoi(result[2]) - stoi(result[4])/2.0 , stoi(result[3]) - stoi(result[5])/2.0, stoi(result[4]), stoi(result[5])) );
    }
}

// returns orientation between 0 and 180
int compute_orientation(Mat full_image, string output_path)
{
  Mat dst, cdst;
  Canny(full_image, dst, 50, 200, 3);

  cvtColor(dst, cdst, CV_GRAY2BGR);
  vector<Vec4i> lines;
  HoughLinesP(dst, lines, 1, CV_PI/180, 100, 50, 2 );
  int hist [180] = { };
  for( size_t i = 0; i < lines.size(); i++ )
  {
    Vec4i l = lines[i];
    line( cdst, Point(l[0], l[1]), Point(l[2], l[3]), Scalar(0,0,255), 3, CV_AA);
    int angle = (  int(round(atan2( l[3] - l[1] , l[2] - l[0] ) * 180/PI))  ) % 180  ;
    if(angle < 0)
      angle =  180 + angle;
    hist[angle] ++;
  }
  if(output_path!="")
    imwrite(output_path,cdst);
  int max_rot = 0;
  int max_rot_pond = 0;
  for( int i = 0; i < 180 ; i ++ )
    if( hist[i] > max_rot_pond) {
      max_rot_pond = hist[i];
      max_rot = i;
    }

  // int max_rot = distance(hist, max_element(hist, hist + 180))  ;
  //cout << "Max element : " << max_rot << endl;
  return max_rot;
}

Mat multiplyBy(Mat image, float factor) {
  Mat res (image.rows, image.cols, CV_32FC1);
  // for(int i = 0 ; i < 6; i ++)
  //     cout << "hey " << to_string(image.at<uchar>(0,i,0)) << endl;
  for(int i = 0 ; i < res.cols; i ++)
    for(int j = 0; j < res.rows ; j ++) {
      // cout << "hey " << to_string(image.at<uchar>(j,i)* factor) << endl;
      res.at<float>(j,i,0)  = image.at<uchar>(j,i,0) * factor;
    }

  return res;
};


void detectRectsAndContours(CascadeClassifier * cc, WorkImage image, vector<cv::Rect> & plateZone, vector<vector<cv::Point> > & all_contours) {
  //PLATE DETECTION
  cc->detectMultiScale(image.gray_image,plateZone,1.05,5);
  cout << "Nb plate zones : " << plateZone.size() << endl ;

  if(plateZone.size()==0)
    return;

  //LETTER DETECTION
  vector<Vec4i> hierarchy;
  findContours(image.threshold_image.clone(),all_contours,hierarchy,CV_RETR_LIST,CV_CHAIN_APPROX_NONE);
  cout << "Nb contours found : " << all_contours.size() << endl ;

  // Mat canny_output;
  // int thresh = 100;
  // int max_thresh = 255;
  // RNG rng(12345);
  // Canny( image.threshold_image, canny_output, thresh, thresh*2, 3 );
  // findContours( canny_output, all_contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, cv::Point(0, 0) );
  // imwrite(output_dir + "/" +path.substr(path.find_last_of("/\\") + 1) , threshold_image );
}


//process image
void extractRect(  Mat image, Rect & plateZone,vector<vector<cv::Point> > & all_contours, vector<vector<cv::Point> > & orderedContours, vector<cv::Rect> & orderedRects) {

  vector<vector<cv::Point> > contours_inside;
  vector<cv::Rect> boundRects_inside;
  for( int i = 0 ; i < all_contours.size() ; i ++) {
    // filter on small contours
    if( contourArea(all_contours[i]) > image.rows * image.cols / 1000000 ){
      cv::Rect bR = boundingRect(all_contours[i]);
      //filter countours in plate zone if platezone exists
      if( inside( bR, plateZone ) ) {
        contours_inside.push_back(all_contours[i]);
        boundRects_inside.push_back(bR);
      }
    }
  }
  cout << "      Nb big enough contours inside zone : " << contours_inside.size() << endl ;

  // delete contours inside other
  vector<vector<cv::Point> > contours;
  vector<cv::Rect> boundRects;
  for( int i = 0 ; i < contours_inside.size() ; i ++) {

    bool sup = true;
    for( int j = 0 ; j < contours_inside.size() ; j ++)
        if( boundRects_inside[j].contains(boundRects_inside[i].tl() ) && boundRects_inside[j].contains(boundRects_inside[i].br() ) ) {
          sup = false;
          break;
        }
    if(sup) {
      contours.push_back(contours_inside[i]);
      boundRects.push_back(boundRects_inside[i]);
      //getOrientation( contours_inside[i], image.image);
    }
  }
  cout << "      Nb big enough and deduplicated contours inside zone : " << contours.size() << endl ;
  if(contours.size() == 0) return;


  // distance de couleur entre les lettres
  vector<vector<cv::Point> > contours2;
  vector<cv::Rect> boundRects2;
  Mat mask = Mat::zeros(image.rows, image.cols, CV_8UC1);
  drawContours(mask, contours, -1, Scalar(255), CV_FILLED);
  //imshow("mask", mask);
  int channels[] = {0,1,2};
  float sranges[] = { 0, 256 };
  const float* ranges[] = { sranges, sranges, sranges };
  int histSize[] = { 25, 25, 25 };

  Mat b_hist;
  calcHist(&image,1,channels,mask,b_hist,3,histSize,ranges,true,false);
  normalize(b_hist, b_hist );

  for( int j = 0 ; j < contours.size() ; j ++ ) {
    Mat sub = image(boundRects[j]);
    Mat sub_mask = mask(boundRects[j]);
    // imshow("mask", sub_mask);
    Mat b_hist_j;
    calcHist(&sub,1,channels,sub_mask,b_hist_j,3,histSize,ranges,true,false);
    normalize(b_hist_j, b_hist_j );
    float distance = 0;
    for( int x = 0 ; x < histSize[0] ; x ++ )
      for( int y = 0 ; y < histSize[1] ; y ++ )
        for( int z = 0 ; z < histSize[2] ; z ++ )
        {
          distance += min( b_hist_j.at<float>(x, y, z), b_hist.at<float>(x, y, z));
        }
    //cout << distance << endl;
    if(distance > 1.7) //1.70
      {
        contours2.push_back(contours[j]);
        boundRects2.push_back(boundRects[j]);
        getOrientation( contours[j], image);
      }
  }

  //order RECTS
  int min_rect_abs = -1;
  for( int j = 0 ; j < boundRects2.size() ; j ++) {
    int argmin = 0;
    int current_max = 1000000000;
    for(int i = 0 ; i < boundRects2.size(); i ++ )
      if( (boundRects2[i].tl().x < current_max) && (boundRects2[i].tl().x > min_rect_abs) ) {
        argmin = i;
        current_max = boundRects2[argmin].tl().x;
      }
    orderedRects.push_back( boundRects2[argmin] );
    orderedContours.push_back( contours2[argmin] );
    min_rect_abs = boundRects2[argmin].tl().x;
  }

}

Mat resizeContains( Mat image, int cols, int rows, int & left, int & right, int & up, int & down, bool noise ) {

  //resize
  Mat resized_image;
  double normalization_factor_width = ((float)cols) / ((float) image.size().width);
  double normalization_factor_height = ((float)rows) / ((float)image.size().height);
  double normalization_factor = std::min(normalization_factor_width, normalization_factor_height);
  resize(image,resized_image,Size(), normalization_factor, normalization_factor,INTER_LINEAR);

  //makeborder
  RNG rng;
  Mat last_image;
  if(noise)
    add_salt_pepper_noise(last_image,0.3,0.3,&rng);

  left = round((cols - resized_image.size().width) / 2.0);
  right =  cols - resized_image.size().width - left;
  up = round((rows - resized_image.size().height) / 2.0);
  down = rows - resized_image.size().height - up;
  if(noise)
    copyMakeBorder(resized_image, last_image, up, down, left, right , BORDER_TRANSPARENT);
  else
    copyMakeBorder(resized_image, last_image, up, down, left, right , BORDER_CONSTANT, white);


  resized_image.release();
  return last_image;
}

// noise functions
void add_salt_pepper_noise(Mat &img, float pa, float pb, RNG *rng )
{
  Mat saltpepper_noise = Mat::zeros(img.rows, img.cols,CV_8U);
  randu(saltpepper_noise,0,255);

  Mat black , white;
  threshold(saltpepper_noise, black, 5.0, 255, cv::THRESH_BINARY_INV);
  threshold(saltpepper_noise, white, 250.0, 255, cv::THRESH_BINARY);

  img.setTo(255,white);
  img.setTo(0,black);
}

void add_gaussian_noise(Mat &srcArr,double mean,double sigma, RNG *rng)
{
    Mat NoiseArr = srcArr.clone();
    rng->fill(NoiseArr, RNG::NORMAL, mean,sigma);
    add(srcArr, NoiseArr, srcArr);
}

// display functions

void displayRects (Mat image, vector<cv::Rect> plateZone, Scalar color1) {
  for(int i = 0 ; i < plateZone.size(); i ++) {
    rectangle( image, plateZone[i].tl(), plateZone[i].br(), color1, 2, 8, 0 );
  }
}

void displayRectangle (Mat image, cv::Rect r, Scalar color1) {
  rectangle( image, r.tl(), r.br(), color1, 2, 8, 0 );
}

void displayRotatedRectangle (Mat image, RotatedRect rRect, Scalar color1 ) {
  cv::Point2f vertices[4];
  rRect.points(vertices);
  for (int i = 0; i < 4; i++)
    line(image, vertices[i], vertices[(i+1)%4], color1, 2, 8, 0 );
}

void displayCross (Mat image, Point2f p, Scalar color1) {
  line(image, p - Point2f(3.0,0.0), p + Point2f(3.0,0.0), color1, 1, 8, 0 );
  line(image, p - Point2f(0.0,3.0), p + Point2f(0.0,3.0), color1, 1, 8, 0 );
}

void displayText( Mat image, string text, cv::Point textOrg , double fontScale  ) {
  int fontFace = 0;

  int thickness = max( (int)(3 * fontScale), 1);
  int baseline=0;
  Size textSize = getTextSize(text, fontFace,fontScale, thickness, &baseline);
  baseline += thickness;
  rectangle(image, textOrg + Point(0, baseline + textSize.height / 2),
            textOrg + Point(textSize.width, - textSize.height / 2),
            Scalar(0,0,255),CV_FILLED);
  putText(image, text, textOrg + Point(0, textSize.height/2), fontFace, fontScale,Scalar::all(255), thickness, (int)(8.0 * fontScale));
}


//OCR
bool isLegal(char c)
{
  int len = sizeof(legal)/sizeof(char);

  for (int i = 0; i < len; i++)
    if (c == legal[i])
      return true;
  return false;
}

int char2Class(char c)
{
  int len = sizeof(legal)/sizeof(char);

  for (int i = 0; i < len; i++)
    if (c == legal[i])
      return i;
  return -1;
}

void most_probable_month(std::vector<float> output1, std::vector<float> output2, string &month, float & month_proba ) {

  month_proba = 0.0;
  char month_c [3] ;
  month_c[2] = '\0';
  // ensemble des chaines possibles
  for(int i = 0; i < 2 ; i ++) { // 0 ou 1
    int max_d = 10;
    int min_d = 1 ;
    if(i == 1) {max_d = 3; min_d = 0;};
    for(int j = min_d; j < max_d ; j ++ ) {
      float score = output1[i]+ output2[j];
      if( score > month_proba) {
        month_c[0] = driving_letters[i];
        month_c[1] = driving_letters[j];
        month = month_c;
        month_proba = score;
      }
    }
  }

}

void most_probable_year(std::vector<float> output1, std::vector<float> output2, std::vector<float> output3, std::vector<float> output4, string &year, float & year_proba) {

  year_proba = 0.0;
  char year_c [5];
  year_c[4] = '\0';
  for(int i = 0; i < 12; i ++) {// 190x, 191x, 192x, 193x, 194x, 195x, 196x ... 199x, 200x, 201x
    float prob ;
    if(i == 10) {
      prob = output1[2] + output2[0] + output3[0];
      year_c [0] = '2';
      year_c [1] = '0';
      year_c [2] = '0';
    } else if(i == 11) {
      prob = output1[2] + output2[0] + output3[1];
      year_c [0] = '2';
      year_c [1] = '0';
      year_c [2] = '1';
    } else {
      prob = output1[1] + output2[9] + output3[i];
      year_c [0] = '1';
      year_c [1] = '9';
      year_c [2] = driving_letters[i];
    }
    int max_d = 10;
    if(i == 11) max_d = 7;
    for(int j = 0; j < max_d ; j ++) {
      float score = prob + output4[j];
      if( score > year_proba) {
        year_c [3] = driving_letters[j];
        year = year_c ;
        year_proba = score;
      }
    }
  }
};


void compute_lines(std::vector<Point> letter_points, int margin, vector< vector<Point> > &ordered_lines, vector< float > &ordered_lines_y) {

  // int margin = 5;

  float height = 0.0;
  for( int i = 0; i < letter_points.size(); i ++ )
    height = std::max( (float)( letter_points[i].y), height );

  std::vector<float> counts;
  std::vector<std::vector<Point> > ordered_points;
  for( int y = 0 ; y < height - margin ; y ++) {
    int count = 0;
    std::vector<Point> line_points;
    for(int i = 0; i < letter_points.size(); i++ )
      if( (letter_points[i].y >= y) && (letter_points[i].y <= y + margin) ) {
        count ++ ;
        line_points.push_back(letter_points[i]);
      }
    counts.push_back(count);
    ordered_points.push_back( line_points );
  }

// cout << "ok " << endl;
  std::vector<int> maxY = Argmax(counts, 50000);
// cout << "max" <<endl;
  std::vector<std::vector<Point> > line_points;
  std::vector<int> maxYvalid ;
  for(int i = 0; i < maxY.size(); i ++) {
    int m = maxY[i]; // index de la ligne
    std::vector<Point> o = ordered_points[m];

    if(!o.size() || o.size()< 15) {
      cout << "       Less than 15 points in this line; skip." << endl;
      break;
    }

    bool next = false;
    for(int j = 0; j < maxYvalid.size() ; j ++ )
      if( (m + margin >= maxYvalid[j]  ) && (m - margin <= maxYvalid[j])  ) { next = true; break;}

    if(next)
        continue;

    maxYvalid.push_back( m );
    line_points.push_back( o );
  }

  float last_y = - 10.0;
  for(int i = 0; i < line_points.size(); i ++) {
    int min_y = 100000000;
    int arg_min = 0;
    for(int j = 0; j < line_points.size(); j ++) {
      if( line_points[j][0].y > last_y and line_points[j][0].y < min_y ) {
        min_y = line_points[j][0].y;
        arg_min = j;
      }
    }
    last_y = min_y ;
    ordered_lines.push_back( line_points[arg_min] );
    ordered_lines_y.push_back( min_y );
  }



}

void compute_lines_knn(std::vector<Point> letter_points, int nb_lines, vector< vector<Point> > &ordered_lines, vector< float > &ordered_lines_y){
  // calcul des lignes
  if( letter_points.size() > nb_lines ) {
    Mat mtx(letter_points.size(), 1, CV_32F), bestLabels, centers;
    for(int i = 0; i < letter_points.size(); i ++)
      mtx.at<float>(i,0) = letter_points[i].y;
    cv::kmeans(mtx,nb_lines,bestLabels, cv::TermCriteria(CV_TERMCRIT_ITER, 10, 1.0),3, cv::KMEANS_PP_CENTERS, centers);
    vector<float> line_y ;
    for(int i = 0 ; i < centers.rows ; i ++){
      //cout << "Center :" << centers.at<float>(i,0) << endl;
      line_y.push_back( centers.at<float>(i,0) );
    }

    // création des lignes de points

    vector<vector<Point> > lines;
    for (int i = 0; i < nb_lines ; i++) {
      vector<Point> line;
      lines.push_back( line );
    }
    for (int i = 0; i < bestLabels.rows; i++)
      lines[bestLabels.at<int>(i,0)].push_back( letter_points[i] );

    // order lines
    vector<int> order;
    float last_min = -10.0;
    for( int i = 0 ; i < centers.rows ; i ++) {
      // le minimum courant et son index
      float current_min = 100000000000.0;
      int current_argmin;
      // on cherche un minimum supérieur au dernier min
      for( int j = 0; j < centers.rows ; j ++) {
        float y_j = centers.at<float>(j,0);
        if( y_j < current_min && y_j > last_min ) {
          current_argmin = j;
          current_min = y_j;
        }
      }
      // on met à jour le dernier min
      last_min = current_min;

      order.push_back(current_argmin);
    }

    float last_y = - 10.0;
    for(int i = 0; i < order.size(); i ++) {
      int line_index = order[i];

      if(  centers.at<float>(line_index,0) > last_y +5.0 ) {
        //création d'une nouvelle ligne
        ordered_lines.push_back(lines[line_index]);
        last_y = centers.at<float>(line_index,0);
        ordered_lines_y.push_back( centers.at<float>(line_index,0) );

      } else {
        // on fait la moyenne pondérée des lignes comme y celle des deux lignes qui a le plus de points
        //ajout à la ligne précédente
        if( lines[line_index].size() > ordered_lines[ordered_lines.size()-1].size() )
          ordered_lines_y[ordered_lines.size()-1] = centers.at<float>(line_index,0);
        for(int j = 0; j < lines[line_index].size(); j ++)
          ordered_lines[ordered_lines.size()-1].push_back(lines[line_index][j]);
      }
    }
    mtx.release();
    bestLabels.release();
    centers.release();
  }
}


// rect functions

int getCenterX(cv::Rect r) {
  return r.tl().x + floor(((float)( r.br().x - r.tl().x ) ) / 2.0);
}

int getCenterY(cv::Rect r) {
  return  r.tl().y + floor(((float)( r.br().y - r.tl().y ) ) / 2.0);
}

bool inside(cv::Rect bR, cv::Rect plateZone) {
  int bx = bR.tl().x, by = bR.tl().y, bh = bR.size().height, bw = bR.size().width;
  //bR.area() > ((double) plateZone.area()) / 35
  return bR.size().height < plateZone.size().height && bR.size().height * 3 > plateZone.size().height && bR.size().width * 5 < plateZone.size().width && bR.size().width *25 > plateZone.size().width && plateZone.contains( cv::Point2f( bx + bw/2, by + bh/2) )   ;
}

// compute if a rectangle is inside an image
bool is_in_image(Rect r, Mat img) {
  return ( ( r.tl().x >= 0 ) && ( r.tl().y >= 0 ) && ( r.br().x < img.cols ) && ( r.br().y < img.rows ) );
}
bool is_in_image(RotatedRect r, Mat img) {
  return is_in_image( r.boundingRect(), img);
}

Point2f change_ref ( Point2f p, float center_x, float center_y, float orientation) {
    Point2f p_repositioned (p.x - center_x, p.y - center_y );
    double orientation_radian = orientation * 3.14159265 / 180.0 ;

    double hypothenuse = sqrt( p_repositioned.x * p_repositioned.x + p_repositioned.y * p_repositioned.y );
    double angle = atan2( p_repositioned.y , p_repositioned.x ) ;
    return Point2f( hypothenuse * cos( angle - orientation_radian ),  hypothenuse * sin( angle - orientation_radian ) );
}


// compute if point is in rotated rectangle
bool is_in( Point p, RotatedRect rr ) {
    Point2f p_new = change_ref ( p, rr.center.x, rr.center.y, rr.angle);
    //cout << p_repositioned << endl;

    //cout << " compare " << (new_y < rr.size.height / 2.0)  << " - " << new_y << " - " << rr.size.height / 2.0 << endl;
    //cout << "is in " << ( (new_x  > - rr.size.width / 2.0) && (new_x  < rr.size.width / 2.0) && (new_y > - rr.size.height / 2.0) && (new_y < rr.size.height / 2.0)  ) ;
    return ( (p_new.x  >= - rr.size.width / 2.0) && (p_new.x  <= rr.size.width / 2.0) && (p_new.y >= - rr.size.height / 2.0) && (p_new.y <= rr.size.height / 2.0)  );
}

// compute brute-force Intersection Over Union
float intersectionOverUnion( RotatedRect r1, RotatedRect r2) {
  Rect br1 = r1.boundingRect();
  Rect br2 = r2.boundingRect();
  Rect br ( Point( min(br1.tl().x , br2.tl().x ) , min(br1.tl().y , br2.tl().y )  ) , Point(max(br1.br().x , br2.br().x ) , max(br1.br().y , br2.br().y ) ) );
  //cout << "Area : " << br.area() << endl;
  int intersection = 0;
  for(int i = 0 ; i <= br.size().width; i ++ )
    for(int j = 0; j <= br.size().height ; j ++) {
      Point p ( br.tl().x + i , br.tl().y + j );
      if( is_in(p,r1) && is_in(p,r2) )
          intersection ++;
    }
    //cout << "BR : " << br2.tl() << " -> " << br2.br() << endl;
    //cout << "IOU " << ((float)intersection) << " - " << ((float) br.area()) << endl;
  return ((float)intersection) / ( (float) br.area());
}

// others
Mat createOne(vector<Mat> & images, int cols, int rows, int gap_size, int dim)
{
   cv::Mat result ( rows * dim + (rows+2) * gap_size, cols * dim + (cols+2) * gap_size, images[0].type(),white);
    size_t i = 0;
    int current_height = gap_size;
    int current_width = gap_size;
    for ( int y = 0; y < rows; y++ ) {
        for ( int x = 0; x < cols; x++ ) {
            if ( i >= images.size() )
                return result;
            // get the ROI in our result-image
            cv::Mat to(result,
                       cv::Range(current_height, current_height + dim),
                       cv::Range(current_width, current_width + dim));
            // copy the current image to the ROI
            images[i++].copyTo(to);
            current_width += dim + gap_size;
        }
        // next line - reset width and update height
        current_width = gap_size;
        current_height += dim + gap_size;
    }
    return result;
}

void standard_deviation(vector<int> data, double & mean, double & stdeviation,double & median)
{
    std::sort (data.begin(), data.end());
    mean=0.0;
    stdeviation=0.0;
    int i;
    for(i=0; i<data.size();++i)
        mean+=data[i];
    mean = mean / data.size();
    for(i=0; i< data.size();++i)
      stdeviation += (data[i]-mean)*(data[i]-mean);
    stdeviation = sqrt(stdeviation / data.size());
    median = round(data.size() / 2);
    return;
}


double getOrientation(vector<cv::Point> &pts, Mat &img)
{
    //Construct a buffer used by the pca analysis
    Mat data_pts = Mat(pts.size(), 2, CV_64FC1);
    for (int i = 0; i < data_pts.rows; ++i)
    {
        data_pts.at<double>(i, 0) = pts[i].x;
        data_pts.at<double>(i, 1) = pts[i].y;
    }

    //Perform PCA analysis
    PCA pca_analysis(data_pts, Mat(), CV_PCA_DATA_AS_ROW);

    //Store the position of the object
    cv::Point pos = cv::Point(pca_analysis.mean.at<double>(0, 0),
                      pca_analysis.mean.at<double>(0, 1));

    //Store the eigenvalues and eigenvectors
    vector<cv::Point2d> eigen_vecs(2);
    vector<double> eigen_val(2);
    for (int i = 0; i < 2; ++i)
    {
        eigen_vecs[i] = cv::Point2d(pca_analysis.eigenvectors.at<double>(i, 0),
                                pca_analysis.eigenvectors.at<double>(i, 1));

        eigen_val[i] = pca_analysis.eigenvalues.at<double>(0, i);
    }

    // Draw the principal components
    circle(img, pos, 3, CV_RGB(255, 0, 255), 2);
    if(  eigen_vecs[0].x * eigen_val[0] * eigen_vecs[0].x * eigen_val[0] + eigen_vecs[0].y * eigen_val[0] * eigen_vecs[0].y * eigen_val[0] > eigen_vecs[1].x * eigen_val[1] * eigen_vecs[1].x * eigen_val[1] + eigen_vecs[1].y * eigen_val[1] * eigen_vecs[1].y * eigen_val[1] ) {
      line(img, pos, pos + 0.2 * cv::Point(eigen_vecs[0].x * eigen_val[0], eigen_vecs[0].y * eigen_val[0]) , CV_RGB(255, 255, 0));
      return atan2(eigen_vecs[0].y, eigen_vecs[0].x) * 180 / 3.1417 - 90;
    } else {
      line(img, pos, pos + 0.2 * cv::Point(eigen_vecs[1].x * eigen_val[1], eigen_vecs[1].y * eigen_val[1]) , CV_RGB(0, 255, 255));
      return atan2(eigen_vecs[1].y, eigen_vecs[1].x) * 180 / 3.1417 - 90;
    }

}

void myGetQuadrangleSubPix(const Mat& src, Mat& dst,Mat& m )
{

    cv::Size win_size = dst.size();
    double matrix[6];
    cv::Mat M(2, 3, CV_64F, matrix);
    m.convertTo(M, CV_64F);
    double dx = (win_size.width - 1)*0.5;
    double dy = (win_size.height - 1)*0.5;
    matrix[2] -= matrix[0]*dx + matrix[1]*dy;
    matrix[5] -= matrix[3]*dx + matrix[4]*dy;

    // RNG rng;
    // add_salt_pepper_noise(dst,0.3,0.3,&rng);
    cv::warpAffine(src, dst, M, dst.size(),
        cv::INTER_LINEAR + cv::WARP_INVERSE_MAP,
        cv::BORDER_CONSTANT);
}

void getRotRectImg(cv::RotatedRect rr,Mat &img,Mat& dst)
{
    Mat m(2,3,CV_64FC1);
    float ang=rr.angle*CV_PI/180.0;
    m.at<double>(0,0)=cos(ang);
    m.at<double>(1,0)=sin(ang);
    m.at<double>(0,1)=-sin(ang);
    m.at<double>(1,1)=cos(ang);
    m.at<double>(0,2)=rr.center.x;
    m.at<double>(1,2)=rr.center.y;
    myGetQuadrangleSubPix(img,dst,m);
}

Mat extractRotatedRect( Mat src, RotatedRect rect ) {
  Mat dst(rect.size,CV_32FC3);
  getRotRectImg(rect,src,dst);
  return dst;
  // // matrices we'll use
  // Mat M, rotated, cropped;
  // // get angle and size from the bounding box
  // float angle = rect.angle;
  // Size rect_size = rect.size;
  // // thanks to http://felix.abecassis.me/2011/10/opencv-rotation-deskewing/
  // if (rect.angle < -45.) {
  //     angle += 90.0;
  //     std::swap(rect_size.width, rect_size.height);
  // }
  // // get the rotation matrix
  // M = getRotationMatrix2D(rect.center, angle, 1.0);
  // // perform the affine transformation
  // warpAffine(src, rotated, M, src.size(), INTER_CUBIC,cv::BORDER_CONSTANT);
  // // crop the resulting image
  // getRectSubPix(rotated, rect_size, rect.center, cropped);
  // return cropped;
}

void correct_ratio ( Rect & r, double ratio ) {
  float width = (float) r.size().width;
  float height = (float) r.size().height;

  float current_ratio = height / width;

  if (  current_ratio > ratio  ) {
    // augment width
    int new_width_delta = floor( ( height / ratio  - width )  / 2.0  );

    r.x -= new_width_delta;
    r.width += new_width_delta * 2;

  } else {
    // augment height
    int new_height_delta = floor( (width * ratio -height) / 2.0 );

    r.y -= new_height_delta ;
    r.height += new_height_delta * 2 ;


  }
};


Size2f correct_ratio ( float width, float height, double ratio ) {

  float current_ratio = height / width;

  if (  current_ratio > ratio  ) {
    // augment width
    int new_width = floor( height / ratio );

    return Size( new_width, height );
    // r.size.width = new_width;

  } else {
    // augment height
    int new_height = floor( width * ratio );

    return Size( width, new_height );
    // r.size.height = new_height;

  }
};



bool PairCompare(const std::pair<float, int>& lhs,
                        const std::pair<float, int>& rhs) {
  return lhs.first > rhs.first;
}

/* Return the indices of the top N values of vector v. */
std::vector<int> Argmax(const std::vector<float>& v, int N) {
  std::vector<std::pair<float, int> > pairs;
  for (size_t i = 0; i < v.size(); ++i)
    pairs.push_back(std::make_pair(v[i], i));

  unsigned long max_N = std::min((unsigned long) N, pairs.size());
  std::partial_sort(pairs.begin(), pairs.begin() + max_N, pairs.end(), PairCompare);

  std::vector<int> result;
  for (int i = 0; i < max_N; ++i)
    result.push_back(pairs[i].second);
  return result;
}
