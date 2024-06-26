#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <ctime>

#include "IPM.h"
#include "ImageProcessor.h"

using namespace cv;
using namespace std;

vector<Point2f> readPointsFromFile(const string& filename) {
    vector<Point2f> points;
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error: Unable to open file " << filename << endl;
        exit(EXIT_FAILURE);
    }

    string line;
    while (getline(file, line)) {
        istringstream iss(line);
        float x, y;
        if (!(iss >> x >> y)) {
            cerr << "Error: Invalid format in file " << filename << endl;
            exit(EXIT_FAILURE);
        }
        points.push_back(Point2f(x, y));
    }

    file.close();
    return points;
}
std::vector<cv::Point2f> Points;

void on_mouse(int event, int x, int y, int flags, void* userdata) {
    cv::Mat* img = (cv::Mat*)userdata; // Cast userdata to cv::Mat pointer 

    if (event == cv::EVENT_LBUTTONDOWN) {
        std::vector<cv::Scalar> colors = {
            cv::Scalar(0, 0, 255),  // Màu đỏ
            cv::Scalar(0, 255, 0),  // Màu xanh lá
            cv::Scalar(255, 0, 0),  // Màu xanh dương
            cv::Scalar(0, 255, 255) // Màu vàng
        };

        cv::Scalar color = colors[Points.size() % colors.size()];
        Points.push_back(cv::Point2f(x, y));
        cv::circle(*img, cv::Point(x, y), 5, color, -1);
        cv::imshow("Image", *img);
    } else if (event == cv::EVENT_MOUSEMOVE) {
        cv::Mat temp = img->clone();

        std::string coordinates = "Coordinates: (" + std::to_string(x) + ", " + std::to_string(y) + ")";
        cv::putText(temp, coordinates, cv::Point(10, 20), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 255), 1);
        cv::imshow("Image", temp);
    }
}
void draw_points(cv::Mat& img, int& i) {
    cv::Mat tempImg = img.clone();
    cv::namedWindow("Image");
    cv::setMouseCallback("Image", on_mouse, &tempImg);

    auto keyboardEventHandler = [&](int key) {
        switch (key) {
            case 'r':
            case 'R':
                Points.clear();  
                tempImg = img.clone(); 
                break;
            case 32:  
                cv::destroyAllWindows(); 
                break;
            case 13:
                cv::destroyAllWindows(); 
                ++i;
            default:
                break;
        }
    };

    while (true) {
        int key = cv::waitKey(0);
        if (key != -1) {  
            keyboardEventHandler(key);
            if (key == 32 || key == 13)  
                break;
        }
    }
}


int main( int _argc, char** _argv )
{
    Mat inputImg, inputImgGray;
    Mat outputImg;
    
    string imageFolder = _argv[1];
    string origPointsFolder = _argv[2];
    string dstPointsFolder = _argv[3];
    int i = 1;
    while (i <= 6) {
        int temp = i;
        string imageFileName = imageFolder + "/" + to_string(i) + ".jpg";
        inputImg = imread(imageFileName);
        draw_points(inputImg,i);

        ImageProcessor processor;
        processor.findQuadrilateralFromPointsAndSave(Points,"Points/origin/" + to_string(i));
        Points.clear();

        cv::Mat blackImage(1024, 1280, CV_8UC3, cv::Scalar(0, 0, 0));
        draw_points(blackImage,i);
        processor.findQuadrilateralFromPointsAndSave(Points,"Points/destination/" + to_string(i));
        Points.clear();

        int width = inputImg.cols;
        int height = inputImg.rows;
        // The 4-points at the input image    
        vector<Point2f> origPoints = readPointsFromFile(origPointsFolder + "/" + to_string(i) + ".txt");
        if (origPoints.size() != 4) {
            cerr << "Error: Point file must contain exactly 4 points" << endl;
            continue;
        }

        vector<Point2f> dstPoints = readPointsFromFile(dstPointsFolder + "/" + to_string(i) + ".txt");
        if (dstPoints.size() != 4) {
            cerr << "Error: Point file must contain exactly 4 points" << endl;
            continue;
        }
        // IPM object
        IPM ipm( Size(width, height), Size(1280, 1024), origPoints, dstPoints );
        
        // Process
        clock_t begin = clock();
        ipm.applyHomography( inputImg, outputImg );     
        clock_t end = clock();
        double elapsed_secs = double(end - begin) / CLOCKS_PER_SEC;
        printf("Time taken for IPM transformation for image %d: %.2f (seconds)\n", i, elapsed_secs);
        draw_points(outputImg,i);
        if(temp == i)
            continue;
        string output_IPM = "IPM/" + to_string(i-1) + ".jpg";
        imwrite(output_IPM, outputImg);
        cout << "Output IPM saved as: " << output_IPM << endl;

        cv::Mat result_points = processor.drawPoints(inputImg,origPoints);
        string outputFile = "img_process/" + to_string(i-1) + ".jpg";
        imwrite(outputFile, result_points);
        cout << "Output image saved as: " << outputFile << endl;
    }
    ImageProcessor prc;
    std::string folderPath = "IPM";
    std::vector<cv::Mat> imgs = prc.readImagesFromFolder(folderPath);
    cv::Mat merge_img = prc.mergeImages(imgs);
    string merge_img_file = "img_process/result.jpg";
    imwrite(merge_img_file, merge_img);

    cv::Mat edges_image = prc.edge_detect(merge_img);
    string edges_image_file = "img_process/edge.jpg";
    imwrite(edges_image_file, edges_image);

    std::pair<std::vector<cv::Vec4i>, std::vector<cv::Vec4i>> lines = prc.detect_lines(edges_image);
    std::vector<cv::Vec4i> horizontal_lines = lines.first;
    std::vector<cv::Vec4i> vertical_lines = lines.second;
    cv::Mat gray_image;
    cv::cvtColor(merge_img, gray_image, cv::COLOR_BGR2GRAY);
    cv::Mat line_image = cv::Mat::zeros(gray_image.size(), CV_8UC1);
    line_image = prc.visualize_lines(line_image,horizontal_lines);
    line_image = prc.visualize_lines(line_image,vertical_lines);
    string lines_image_file = "img_process/line.jpg";
    imwrite(lines_image_file, line_image);

    cv::Mat box_image = cv::Mat::zeros(gray_image.size(), CV_8UC1);
    string box_image_file = "img_process/box.jpg";
    // box_image = prc.draw_parking_spaces(box_image,vertical_lines,'v');
    // box_image = prc.draw_parking_spaces(box_image,horizontal_lines,'h');
    
    std::ofstream outfile("img_process/parking_space.txt", std::ios::trunc);
    box_image = prc.draw_parking_spaces(box_image,vertical_lines,'v',"img_process/parking_space.txt");
    box_image = prc.draw_parking_spaces(box_image,horizontal_lines,'h',"img_process/parking_space.txt");
    imwrite(box_image_file, box_image);
    cv::Mat box_result;
    box_result = prc.draw_parking_spaces(merge_img,vertical_lines,'v');
    box_result = prc.draw_parking_spaces(box_result,horizontal_lines,'h');
    string box_result_file = "img_process/box_result.jpg";
    imwrite(box_result_file, box_result);
    prc.print_parking_spaces("img_process/parking_space.txt");
    return 0;    
}  



