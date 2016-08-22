
#include "Utils.h"
#include "Output.h"
#include <fstream>
#include <sys/stat.h>
#include <glog/logging.h>
#include <gflags/gflags.h>

using std::cout;
using std::endl;

#define PI 3.14159265

// input
DEFINE_string(input_class_filter, "", "Filter entries on the specified class");
DEFINE_int32(limit, 0, "The number of input annotations to consider");

// Capture window dimensions
DEFINE_double(ratio, 1.0, "The ratio of capture window height/width");

//offsets
DEFINE_double(offset_x, 0.0, "Add an offset on the first axis of the rectangle, in percentage of the width of the rectangle. If zero, no offset");
DEFINE_double(offset_y, 0.0, "Add an offset on the second axis of the rectangle, in percentage of the width of the height. If zero, no offset");

DEFINE_double(factor, 1.0, "The scale factor to apply on annotation rectangle width and height");
DEFINE_double(factor_width, 1.0, "The scale factor to apply on annotation rectangle width");
DEFINE_double(factor_height, 1.0, "The scale factor to apply on annotation rectangle height");

// operations
// DEFINE_bool(redress, true, "Image is rotated to be parallel to the annotation window [Default : true]");
DEFINE_bool(merge, false, "If multiple rectangle per images, merge them");
DEFINE_bool(merge_line, false, "If multiple rectangle per images, merge rectangles that are roughly on the same line.");
DEFINE_bool(correct_ratio, false, "Correct the ratio of the annotated rectangles by augmenting its smallest dimension");
DEFINE_bool(add_borders, false, "Add borders to the window to fit the ratio");
DEFINE_bool(skip_rotation, false, "Skip rotation angle");
DEFINE_bool(full_image, false, "Will not extract the annotation");

// noise
DEFINE_double(noise_rotation, 0.0, "Add a rotation noise. If zero, no noise");
DEFINE_double(noise_translation, 0.0, "Add a translation noise. In %age of the dimensions. If zero, no noise");
DEFINE_double(noise_translation_offset, 0.0, "Defines an offset in the translation noise. In %age of the dimensions. If zero, no offset");
DEFINE_double(noise_zoomin, 1.0, "Add a noise in the scale factor. If 1.0, no zoomin noise");
DEFINE_double(noise_zoomout, 1.0, "Add a noise in the scale factor. If 1.0, no zoomout noise");
DEFINE_int32(samples, 1, "The number of noised samples to extract");
DEFINE_double(pepper_noise, 0.0, "Add pepper noise");
DEFINE_double(gaussian_noise, 0.0, "Add gaussian noise");

// output
DEFINE_double(resize_width, 0.0, "Resize width of capture window. If zero, no resize");
DEFINE_bool(gray, false, "Extract as a gray image");
DEFINE_string(backend, "directory", "The output format for storing the result. Possible values are : directory, lmdb, tesseract, opencv");
DEFINE_string(output_class, "", "Override the class by the specified class");
DEFINE_bool(output_by_label, true, "Output different labels in different output directories. For backend=directory only.");
DEFINE_bool(append, false, "Append results to an existing directory. For backend=directory only.");

// negatives
DEFINE_double(neg_width, 0.2, "The width of negative samples to extract, in pourcentage to the largest image dimension (width or height)");
DEFINE_int32(neg_per_pos, 0, "The number of negative samples per positives to extract");

int extract_image(Output * output, string  filename, Mat image, \
  RotatedRect r, string label, double ratio, \
  double factor, double factor_width, double factor_height, double offset_x, double offset_y,\
  bool skip_rotation, bool full_image, bool add_borders, int samples, \
  double noise_rotation, double noise_translation, double noise_translation_offset,\
  double noise_zoomin, double noise_zoomout, double resize_width, double gaussian_noise, double pepper_noise, RNG *rng ) {

    int nb_extrat = 0;
    int rotation = r.angle, res_w = r.size.width, res_h = r.size.height;
    int len = std::max(image.rows, image.cols);
    cv::Point2f pt(len/2., len/2.);

    float rotation_in_radian = ((float)rotation) * 3.14159265 / 180.0; //(rotation-180) % 180 + 180
    int res_x = r.center.x + offset_x * res_w * cos(rotation_in_radian)- offset_y * res_w * sin(rotation_in_radian);
    int res_y = r.center.y + offset_x * res_w * sin(rotation_in_radian)+ offset_y * res_w * cos(rotation_in_radian);

    //int res_x = r.center.x + offset_x * res_w * cos(rotation_in_radian) - offset_y * res_h * sin(rotation_in_radian);
    //int res_y = r.center.y + offset_x * res_w * sin(rotation_in_radian) + offset_y * res_h * cos(rotation_in_radian);
    // + Point2f( offset_x * res_w, offset_y * res_h )
    cout << "  Extracting labelled rectangle : [" << label << "," << res_x << "," << res_y << "," << res_w << "," << res_h << "," << rotation << "]" << endl;

    /// COMPUTE ROTATION
    int redress_rotation = 0;
    if(!skip_rotation)
      redress_rotation = rotation;

    int nb_sample = 0;
    while(nb_sample < samples) {
      nb_sample++;

      // noise rotation
      float delta_rotation = 0.0;
      if(noise_rotation!=0.0) {
        delta_rotation = - noise_rotation + 2.0 * static_cast <float> (rand()) /( static_cast <float> (RAND_MAX/ noise_rotation ));
      }

      // redress image...
      cv::Mat r = cv::getRotationMatrix2D(pt, redress_rotation + delta_rotation, 1.0);
      cv::Mat dst ;
      add_salt_pepper_noise(dst,0.3,0.3,rng);
      cv::warpAffine(image, dst, r, Size(len,len), INTER_LINEAR,cv::BORDER_CONSTANT);

      // ... and adapt annotation to new referentiel of the new image and factor
      RotatedRect extract_rrect ( change_ref ( cv::Point2f( res_x , res_y), pt.x, pt.y, redress_rotation + delta_rotation) + pt  , Size2f(res_w * factor * factor_width, res_h * factor * factor_height ), 0.0 );
      RotatedRect origin_rrect (extract_rrect.center, extract_rrect.size, - delta_rotation );

      // noise translation
      float delta_x = 0.0, delta_y = 0.0;
      if(noise_translation!=0.0) {
        float noise_translation_range = (noise_translation-noise_translation_offset) ;
        delta_x = - noise_translation_range * extract_rrect.size.width + 2.0 * static_cast <float> (rand()) /( static_cast <float> (RAND_MAX/ ( noise_translation_range   * extract_rrect.size.width) ));
        delta_x += sgn(delta_x) * noise_translation_offset * extract_rrect.size.width;
        delta_y = - noise_translation_range * extract_rrect.size.height + 2.0 * static_cast <float> (rand()) /( static_cast <float> (RAND_MAX/ ( noise_translation_range * extract_rrect.size.height) ));
        delta_y += sgn(delta_y) * noise_translation_offset * extract_rrect.size.height;
        extract_rrect.center = extract_rrect.center + Point2f( delta_x, delta_y);
      }

      // noise scale
      float delta_scale = 1.0;
      if(noise_zoomin!=1.0 || noise_zoomout!=1.0) {
        float scale_range = noise_zoomout - 1/noise_zoomin;
        delta_scale = 1/noise_zoomin + static_cast <float> (rand()) /( static_cast <float> (RAND_MAX/ scale_range ));
        extract_rrect.size = Size2f(extract_rrect.size.width * delta_scale, extract_rrect.size.height * delta_scale);
      }

      //extract annotation and adapt to new referentiel
      if(!full_image) {
        dst = extractRotatedRect(dst,extract_rrect);
        //adapt the referentiel
        extract_rrect.center = Point2f( dst.cols / 2.0 , dst.rows / 2.0 );
        origin_rrect.center = Point2f( dst.cols / 2.0 , dst.rows / 2.0 ) + Point2f( -delta_x, -delta_y );
      }

      //resize by adding border (fit ratio) or by stretching
      int left = 0, right = 0, up = 0, down = 0;
      if(resize_width!=0) {
        if(!add_borders){
          float w = dst.cols, h = dst.rows ;
          resize(dst, dst, Size(resize_width, resize_width * ratio), 0.0, 0.0,INTER_LINEAR);
          float factor_x = resize_width / w ;
          float factor_y = resize_width * ratio / h ;
          origin_rrect.center.x = origin_rrect.center.x * factor_x ;
          origin_rrect.center.y = origin_rrect.center.y * factor_y ;
          origin_rrect.size = Size(origin_rrect.size.width * factor_x,origin_rrect.size.height * factor_y);
        }
        else {
          float w = dst.cols ;
          dst = resizeContains(dst,resize_width,resize_width*ratio,left,right,up,down,false);
          float factor = dst.cols / w;
          origin_rrect.center.x = origin_rrect.center.x * factor + left ;
          origin_rrect.center.y = origin_rrect.center.y * factor + up ;
          origin_rrect.size = origin_rrect.size * factor;
        }
      }

      // add pepper noise or gaussian noise
      if(pepper_noise!=0.0)
        add_salt_pepper_noise(dst,pepper_noise,pepper_noise,rng);
      if(gaussian_noise!=0.0)
        add_gaussian_noise(dst,0,gaussian_noise, rng);

      //write rotated image
      output->write( dst, filename, nb_sample, label, origin_rrect, delta_x, delta_y, delta_rotation, delta_scale,left,right,up,down );
      nb_extrat ++;
      // outfile << (std::to_string(item) + ".jpg") << " 1 " << ((int)(p2.x - stoi(result[4])/2)) << " " << ((int)(p2.y - stoi(result[5])/2 )) << " " << stoi(result[4]) << " " << stoi(result[5]) << endl;

      // WorkImage pltimg( output_path);
      // // write image with rotated rect display for debugging
      // displayRotatedRectangle( pltimg.image , RotatedRect( Point2f(rrect.center.x * pltimg.factor, rrect.center.y * pltimg.factor), Size2f(rrect.size.width * pltimg.factor,rrect.size.height * pltimg.factor), rrect.angle ) );
      // output_path = string(argv[2]) + "/_" + std::to_string(i) + ".jpg" ;
      // imwrite( output_path , pltimg.image  );
      // pltimg.release();
      r.release();
      dst.release();
    }
  return nb_extrat;
}


RotatedRect compute_max_bounding_box(std::vector<RotatedRect> &input_rotatedrects) {
  // calcul des coordonnées de la bounding box max
  int x_min = INT_MAX , x_max = 0, y_min = INT_MAX, y_max = 0;
  int height = 0;
  int counts = 0;
  float slope = 0;

  for(int i = 0; i < input_rotatedrects.size(); i++ ) {

    Rect outputRect = input_rotatedrects[i].boundingRect();


    height += outputRect.br().y - outputRect.tl().y;

    if( outputRect.tl().x < x_min )
      x_min = outputRect.tl().x;
    if( outputRect.br().x > x_max )
      x_max = outputRect.br().x;
    if( outputRect.tl().y < y_min )
      y_min = outputRect.tl().y;
    if( outputRect.br().y > y_max )
      y_max = outputRect.br().y;

    int x1 = getCenterX(outputRect);
    int y1 = getCenterY(outputRect);
    if(i + 1 < input_rotatedrects.size() )
      for(int j = i + 1; j < input_rotatedrects.size(); j++ ) {
        int x2 = input_rotatedrects[j].center.x;
        int y2 = input_rotatedrects[j].center.y;
        slope += ((float)(y2 - y1)) / ((float)(x2 - x1));
        counts ++;
      }
  }

  // La max bounding box en Rect
  int width = x_max - x_min;
  Rect rr( x_min, y_min, width, y_max - y_min);
  // float fact_x = factor ;
  // float fact_y = factor;
  // // float fact_x = 1.3;
  // // float fact_y = 1.25;
  // Rect rr_a( x_min - ( fact_x - 1.0 ) * width / 2.0 , y_min - ( fact_y - 1.0 ) * (y_max - y_min) / 2.0 , width * fact_x, (y_max - y_min) * fact_y );

  // La max bounding box en RotatedRect
  int center_x = getCenterX(rr);
  int center_y = getCenterY(rr);
  height = ceil(((float)height) / ((float)input_rotatedrects.size()));
  slope = slope / counts;
  double orientation = atan(slope) * 180 / PI;
  return RotatedRect ( Point(center_x, center_y), Size(width, height), orientation);
  // RotatedRect rd_a( Point(center_x, center_y), Size(width * 1.1, height *1.2), orientation);
  // cout << "Rectangle : " << rr << endl;
  // outfile << input[cursor][0] << ",0," << rr_a.tl().x + rr_a.size().width/2.0 << "," << rr_a.tl().y + rr_a.size().height/2.0 << "," << rr_a.size().width << "," << rr_a.size().height << "," << orientation << endl;

}


void compute_merge_by_line(std::vector<RotatedRect> &input_rotatedrects, std::vector<string> &input_labels, std::vector<RotatedRect> &merged_rotatedrects, std::vector<string> &merged_labels) {

  int margin = 5;

  float height = 0.0;
  for( int i = 0; i < input_rotatedrects.size(); i ++ )
    height = std::max( (float)( input_rotatedrects[i].center.y + input_rotatedrects[i].size.height/2.0), height );

  std::vector<float> counts;
  std::vector<std::vector<RotatedRect> > ordered_rotatedrects;
  std::vector<std::vector<string> > ordered_labels;

  for( int y = 0 ; y < height - margin ; y ++) {
    int count = 0;
    std::vector<RotatedRect> line_rotatedrects;
    std::vector<string> line_labels;
    for(int i = 0; i < input_rotatedrects.size(); i++ )
      if( (input_rotatedrects[i].center.y >= y) && (input_rotatedrects[i].center.y <= y + margin) ) {
        count ++ ;
        line_rotatedrects.push_back(input_rotatedrects[i]);
        line_labels.push_back(input_labels[i]);
      }
    counts.push_back(count);
    ordered_rotatedrects.push_back( line_rotatedrects );
    ordered_labels.push_back(line_labels);
    // cout << "line " << y << endl;
    // for (int i = 0 ; i < line_labels.size(); i ++ )
    //   cout << line_labels[i] ;
    // cout << endl;
  }
  std::vector<int> maxY = Argmax(counts, 100);

  std::vector<int> maxYvalid ;

  for(int i = 0; i < maxY.size(); i ++) {


    int m = maxY[i]; // index de la ligne
    std::vector<string> l =  ordered_labels[m];
    std::vector<RotatedRect> o = ordered_rotatedrects[m];

    if(!l.size())
      break;

    bool next = false;
    for(int j = 0; j < maxYvalid.size() ; j ++ )
      if( (m + margin >= maxYvalid[j]  ) && (m - margin <= maxYvalid[j])  ) { next = true; break;}

    if(next)
        continue;

    maxYvalid.push_back( m );
    merged_rotatedrects.push_back( compute_max_bounding_box(o) );

    int x_previous = 0;
    string label = "";
    int www = 0;
    for(int k = 0; k < o.size(); k++) {
      int x_min = 1000000000;
      int i_min = 0;
      for(int j = 0; j < o.size() ; j ++ )
        if( o[j].center.x < x_min && o[j].center.x > x_previous) {
          x_min = o[j].center.x;
          i_min = j;
          www = o[j].size.width;
        }
      if( (x_previous != 0) && (x_min - x_previous > 0.9 * www) )
        label += " ";
      label += l[i_min];
       x_previous = x_min;
    }
    merged_labels.push_back(label);
  }

}


Point compute_negative(int width, int height, int cols, int rows, std::vector<RotatedRect> input_rotatedrects, float f) {
  int tries = 0;

  int hypothenuse = std::sqrt(  cols * cols + rows * rows );
  while( tries < 100) {
    int ss_x1 = rand() % (width - hypothenuse) + cols/2  ;
    int ss_y1 = rand() % (height  - hypothenuse) + rows/2;
    bool overlap = false;
    for(int i = 0; i < input_rotatedrects.size(); i ++) {
      if( input_rotatedrects[i].size.height * input_rotatedrects[i].size.height * f * f + input_rotatedrects[i].size.width * input_rotatedrects[i].size.width *f * f  > int( (ss_x1 - input_rotatedrects[i].center.x * f) * (ss_x1 - input_rotatedrects[i].center.x * f) + (ss_y1 - input_rotatedrects[i].center.y * f) * (ss_y1 - input_rotatedrects[i].center.y * f)))
        overlap = true;
    }
    if( !overlap )
      return Point(ss_x1, ss_y1);

    tries ++;
  }
  cout << "Compute negative failed." << endl;
  return Point(-1,-1);
}



int main( int argc, char** argv )
{
  #ifndef GFLAGS_GFLAGS_H_
    namespace gflags = google;
  #endif

  gflags::SetUsageMessage("\n"
        "\n"
        "Usage:\n"
        "    ./extractRect [FLAGS] input.csv output_dir"
        "\n");
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  const double& ratio = FLAGS_ratio;
  double& factor = FLAGS_factor;
  double& factor_width = FLAGS_factor_width;
  double& factor_height = FLAGS_factor_height;
  double& offset_x = FLAGS_offset_x;
  double& offset_y = FLAGS_offset_y;

  const bool& merge = FLAGS_merge;
  const bool& merge_line = FLAGS_merge_line;
  const bool& correct_ratio = FLAGS_correct_ratio;
  const double& pepper_noise = FLAGS_pepper_noise;
  const double& gaussian_noise = FLAGS_gaussian_noise;
  const double& noise_rotation = FLAGS_noise_rotation;
  const double& noise_translation = FLAGS_noise_translation;
  const double& noise_translation_offset = FLAGS_noise_translation_offset;
  const double& noise_zoomin = FLAGS_noise_zoomin;
  const double& noise_zoomout = FLAGS_noise_zoomout;
  double& resize_width = FLAGS_resize_width;
  const int& samples = FLAGS_samples;
  const double& neg_width = FLAGS_neg_width;
  const int& neg_per_pos = FLAGS_neg_per_pos;
  const bool& add_borders = FLAGS_add_borders;
  bool& gray = FLAGS_gray;
  const string& db_backend = FLAGS_backend;
  const string& input_class_filter = FLAGS_input_class_filter;
  const int& limit = FLAGS_limit;

  const string& output_class = FLAGS_output_class;
  const bool& output_by_label = FLAGS_output_by_label;
  const bool& append = FLAGS_append;
  const bool& skip_rotation = FLAGS_skip_rotation;
  bool& full_image = FLAGS_full_image;

  if (argc != 3) {
    gflags::ShowUsageWithFlagsRestrict(argv[0],
        "src/extractRect");
  } else {
    google::InitGoogleLogging(argv[0]);

    Output * output ;
    if(db_backend == "directory" ) {
      output = new Directory(argv[2],append,  output_by_label);
    } else if ( db_backend == "opencv" ) {
      full_image = true;
      gray = true;
      output = new OpnCV(argv[2]);
#ifdef CAFFE
    } else if ( db_backend == "lmdb" ) {
      output = new LMDB(argv[2]);
#endif
    } else if ( db_backend == "tesseract") {
      output = new Tessract(argv[2]);
    } else
      LOG(FATAL) << "Unknown db backend " << db_backend;

    string base_path = getbase(argv[1]);
    cout << "Base path: " << base_path << endl;
    cout << "Capture window ratio : " << ratio << endl;
    cout << "Capture window scale factor : " << factor << endl;
    if(gray)
      cout << "Extract in gray (1 channel)" << endl;
    if(skip_rotation)
      cout << "Rotation information in annotations will be skipped." << endl;
    if(add_borders)
      cout << "Add borders to fit ratio." << endl;
    cout << "Noise rotation : [-" << noise_rotation <<  "," << noise_rotation << "]"<< endl;
    cout << "Noise translation : [-" << noise_translation << "," << noise_translation << "]" << endl;
    cout << "Number of samples : " << samples << endl;


    std::vector<std::vector<std::string> > input;
    readCSV( argv[1] , input);

    // get rectangles for each unique input_path
    std::vector<std::string> input_path;
    std::vector<std::vector<string> > input_labels;
    std::vector<std::vector<RotatedRect> > input_rotatedrects;
    bool consider_rotation = true;
    group_by_image(input, consider_rotation, !merge && correct_ratio, ratio, input_path, input_labels, input_rotatedrects);

    RNG rng;
    int nb_inputs = 0;

    for(int cursor = 0; cursor < input_path.size(); cursor++) {

      if(limit!=0 && nb_inputs > limit) break;

      //load image
      string image_path = input_path[cursor];
      if( image_path.at(0) != '.' && image_path.at(0) != '/')
        image_path = base_path + image_path ;

      Mat image = imread(image_path, CV_LOAD_IMAGE_COLOR);
      if(! image.data )                              // Check for invalid input
      {
          std::cout <<  "Could not open or find the image " <<  base_path+input_path[cursor] << std::endl ;
      } else {
        cout << "Item " <<  ": " << input_path[cursor] << endl;

        float f = 1.0;
        int max_dim = max(image.rows, image.cols);
        if(max_dim>10000) {
          cout << "Image too big, resizing." << endl;
          f = 5000.0/((float)max_dim);
          resize(image,image,Size(),f,f,INTER_AREA);
        }

        if(gray)
          cvtColor(image,image,CV_BGR2GRAY);

        string filename = input_path[cursor];
        filename.erase(filename.end()-4,filename.end());
        filename = filename.substr(filename.find_last_of("/") + 1);

        // compute the positives
        int nb_positives = 0;
        if(!merge && !merge_line)
          for(int i = 0; i < input_rotatedrects[cursor].size(); i++){
            string label = input_labels[cursor][i];
            if( input_class_filter != "" && label != input_class_filter ) continue;
            if( output_class != "") label = output_class;

            RotatedRect r = input_rotatedrects[cursor][i];
            r.size.height = r.size.height * f;
            r.size.width = r.size.width * f;
            r.center.x = r.center.x * f;
            r.center.y = r.center.y * f;

            nb_positives += extract_image(output, filename + "_" + std::to_string(i), image, \
              r, label, ratio, \
              factor, factor_width, factor_height, offset_x, offset_y,\
              skip_rotation,full_image, add_borders,samples,\
              noise_rotation,noise_translation,noise_translation_offset, \
              noise_zoomin, noise_zoomout,resize_width,gaussian_noise,pepper_noise, &rng );
          }
        else {
          std::vector<string> merged_labels;
          std::vector<RotatedRect> merged_rotatedrects;
          if(merge_line) {
            cout << "Merge lines :" << endl;
            compute_merge_by_line(input_rotatedrects[cursor],input_labels[cursor], merged_rotatedrects, merged_labels);
            for (int i = 0; i < merged_labels.size() ; i++)
              cout << "   " <<  merged_labels[i] << endl;
          } else {
            merged_rotatedrects.push_back( compute_max_bounding_box(input_rotatedrects[cursor]) );
            merged_labels.push_back("0");
          }
          for(int i = 0 ; i < merged_rotatedrects.size(); i ++ ) {
            RotatedRect r = merged_rotatedrects[i];
            r.size.height = r.size.height * f;
            r.size.width = r.size.width * f;
            r.center.x = r.center.x * f;
            r.center.y = r.center.y * f;
            nb_positives += extract_image(output,filename + "_" + std::to_string(i),image, \
              r, merged_labels[i], ratio, \
              factor,factor_width, factor_height, offset_x, offset_y, \
              skip_rotation, full_image,add_borders, samples, \
              noise_rotation,noise_translation,noise_translation_offset, \
              noise_zoomin, noise_zoomout,resize_width,gaussian_noise,pepper_noise, &rng );
          }


        }
        nb_inputs += nb_positives;

        // compute the negatives
        int neg = 0;
        while( neg < neg_per_pos * nb_positives ) {
          int max_dim = std::max(image.cols, image.rows);
          // cout << "Width of negative extract : " << max_dim * neg_width << endl;
          Point neg_po = compute_negative(image.cols, image.rows, max_dim * neg_width , max_dim * neg_width * ratio, input_rotatedrects[cursor], f);
          if(neg_po.x!=-1) {
            neg += extract_image(output,filename + "_" + std::to_string(neg) , image, \
              RotatedRect(neg_po, Size2f(max_dim *neg_width, max_dim * neg_width * ratio), 0.0 ), "", ratio,\
              1.0,1.0,1.0,0.0,0.0,\
              skip_rotation, full_image, add_borders,1,\
              noise_rotation,0.0,0.0, \
              1.0,1.0,resize_width,gaussian_noise,pepper_noise, &rng );
          }
        }

        image.release();
      }
    }

    output->close();
  }
}
