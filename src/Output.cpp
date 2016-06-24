#include "Output.h"

Output::Output(char * directory) {
  output_dir = directory;
};

Directory::Directory(char * directory, bool append, bool output_by_label): Output(directory) {
  if(!append) {
    CHECK_EQ(mkdir(output_dir, 0744), 0)
        << "mkdir " << output_dir << "failed";
  }
  outfile.open( string(output_dir) + "/results.csv", std::ios_base::app);

  output_by_label_ = output_by_label;
};

void Directory::write( cv::Mat dst, string filename, int nb_sample, string label, cv::RotatedRect rrect, float delta_x, float delta_y , float delta_rotation , float delta_scale, int left, int right, int up, int down ) {

  string label_dir = "";
  string label2 = "";
  string target_dir = string(output_dir);
  if( output_by_label_ ) {
    if( !label.length() )
      label2 = "_";
    else
      label2 = label ;

    target_dir += "/" + label2;
    label_dir = label2 + "/";
  } ;

  struct stat st;
  if(stat(target_dir.c_str(),&st) != 0)
    CHECK_EQ(mkdir( target_dir.c_str() , 0744), 0)
      << "mkdir " << target_dir << " failed";

  string out_filename  = filename + "_" + std::to_string(nb_sample) + ".jpg";
  string output_path = target_dir + "/" + out_filename;
  imwrite( output_path , dst  );

  std::cout << "    Output file path : " << label_dir << out_filename << std::endl;
  std::cout << "    New values : [" << label << "," << rrect.center.x << "," << rrect.center.y << "," << rrect.size.width << "," << rrect.size.height << "," << rrect.angle << "," << delta_x << "," << delta_y << "," << delta_rotation << "," << delta_scale << "]" << std::endl;

  //write rectangle position
  outfile << label_dir + out_filename << "," << label << "," << rrect.center.x << "," << rrect.center.y << "," << rrect.size.width << "," << rrect.size.height << "," << rrect.angle << "," << delta_x << "," << delta_y << "," << delta_rotation << "," << delta_scale << std::endl;
  outfile.flush();
};

void Directory::close() {
  outfile.close();
};


#ifdef CAFFE
LMDB::LMDB(char * directory) : Output(directory) {
  item_id = 0;
  LOG(INFO) << "Opening lmdb " << directory;
  CHECK_EQ(mkdir(directory, 0744), 0)
      << "mkdir " << directory << "failed";
  CHECK_EQ(mdb_env_create(&mdb_env), MDB_SUCCESS) << "mdb_env_create failed";
  CHECK_EQ(mdb_env_set_mapsize(mdb_env, 1099511627776), MDB_SUCCESS)  // 1TB
      << "mdb_env_set_mapsize failed";
  CHECK_EQ(mdb_env_open(mdb_env, directory, 0, 0664), MDB_SUCCESS)
      << "mdb_env_open failed";
  CHECK_EQ(mdb_txn_begin(mdb_env, NULL, 0, &mdb_txn), MDB_SUCCESS)
      << "mdb_txn_begin failed";
  CHECK_EQ(mdb_open(mdb_txn, NULL, 0, &mdb_dbi), MDB_SUCCESS)
      << "mdb_open failed. Does the lmdb already exist? ";
};

void LMDB::write( cv::Mat dst, string filename, int nb_sample, string label, cv::RotatedRect rrect, float delta_x, float delta_y , float delta_rotation , float delta_scale, int left, int right, int up, int down)
{

  // Storing to db
  char label_c;
  char* pixels = new char[dst.rows * dst.cols];
  const int kMaxKeyLength = 10;
  char key_cstr[kMaxKeyLength];
  string value;

  caffe::Datum datum;
  datum.set_channels(1);
  datum.set_height(dst.rows);
  datum.set_width(dst.cols);

  //copy
  for(int i = 0 ; i < dst.rows ; i++)
    for( int j = 0; j < dst.cols ; j++ )
      pixels[i * dst.cols + j] = dst.data[i * dst.cols + j];

  label_c = char2Class(label[0]);

  datum.set_data(pixels, dst.cols*dst.rows);
  datum.set_label(label_c);
  snprintf(key_cstr, kMaxKeyLength, "%08d", item_id);
  datum.SerializeToString(&value);
  string keystr(key_cstr);

  mdb_data.mv_size = value.size();
  mdb_data.mv_data = reinterpret_cast<void*>(&value[0]);
  mdb_key.mv_size = keystr.size();
  mdb_key.mv_data = reinterpret_cast<void*>(&keystr[0]);
  CHECK_EQ(mdb_put(mdb_txn, mdb_dbi, &mdb_key, &mdb_data, 0), MDB_SUCCESS)
      << "mdb_put failed";

  if (++item_id % 100 == 0) {
    // Commit txn
    CHECK_EQ(mdb_txn_commit(mdb_txn), MDB_SUCCESS)
        << "mdb_txn_commit failed";
    CHECK_EQ(mdb_txn_begin(mdb_env, NULL, 0, &mdb_txn), MDB_SUCCESS)
        << "mdb_txn_begin failed";
  }

};

void LMDB::close()
{
  // write the last batch
  CHECK_EQ(mdb_txn_commit(mdb_txn), MDB_SUCCESS) << "mdb_txn_commit failed";
  mdb_close(mdb_env, mdb_dbi);
  mdb_env_close(mdb_env);
};

#endif

Tessract::Tessract(char * directory) : Output(directory) {
  LOG(INFO) << "Creating directory " << directory;
  CHECK_EQ(mkdir(directory, 0744), 0)
      << "mkdir " << directory << " failed";
};

void Tessract::write( cv::Mat dst, string filename, int nb_sample, string label, cv::RotatedRect rrect, float delta_x, float delta_y , float delta_rotation , float delta_scale, int left, int right, int up, int down)
{
  my_images.push_back( dst  );
  letters.push_back( label );
  borders.push_back(Point(left,right));
};

void Tessract::close() {
  int dim = 28;
  int gap_size = 20;
  int nb_cols = 30;

  int nb_rows = std::ceil( ((double) my_images.size()) / nb_cols);
  Mat to_show =  createOne(my_images, nb_cols, nb_rows, gap_size, dim);
  imwrite(std::string(output_dir) + "/" + "lpfra.std.exp0" + ".tif" , to_show );

  outfile.open( std::string(output_dir) + "/" + "lpfra.std.exp0" + ".box", std::ios_base::app);
  int step = gap_size + dim;
  for(int j = 0 ; j < nb_rows ; j ++  )
    for(int i = 0 ; i < nb_cols ; i++ ){
      int ind = j * nb_cols + i;
      if( ind < letters.size() )
        outfile << letters[ind] << " " << i * step + gap_size + borders[i].x << " " << (nb_rows - j) * step + gap_size - dim << " " << (i+1) * step  - borders[i].y << " " << (nb_rows - j) * step + gap_size << " " << 0 << endl;

  }
  outfile.close();
};


OpnCV::OpnCV(char * directory) : Output(directory) {
  string dir_paths [5] = { string(directory), string(directory) + "/pos", string(directory) + "/neg", string(directory) + "/pos/img", string(directory) + "/neg/img" };
  for(int i = 0 ; i < 5 ; i ++) {
    const char * dir_path = dir_paths[i].c_str();
    LOG(INFO) << "Creating directory " << dir_path;
    CHECK_EQ(mkdir(dir_path, 0744), 0)
        << "mkdir " << directory << " failed";
  }
  outfile.open( std::string( directory ) + "/pos/info.dat", std::ios_base::app);
  item_id = 0;
};

void OpnCV::write( cv::Mat dst, string filename, int nb_sample, string label, cv::RotatedRect rrect, float delta_x, float delta_y , float delta_rotation , float delta_scale, int left, int right, int up, int down)
{
  // save a positive
  string output_path =  "img/" + std::to_string(item_id) + ".jpg" ;
  imwrite( string(output_dir) + "/pos/" + output_path , dst );
  outfile << output_path << " 1 " << (int)(rrect.center.x - (rrect.size.width/2)) << " " << (int)(rrect.center.y - ( rrect.size.height / 2)) << " " << (int)rrect.size.width << " " << (int)rrect.size.height << endl;
  outfile.flush();
  item_id ++;

};

void OpnCV::close() {
  outfile.close();
};
