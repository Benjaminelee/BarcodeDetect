/**
Copyright 2017 Rafael Muñoz Salinas. All rights reserved.
Redistribution and use in source and binary forms, with or without modification, are
permitted provided that the following conditions are met:
   1. Redistributions of source code must retain the above copyright notice, this list of
      conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright notice, this list
      of conditions and the following disclaimer in the documentation and/or other materials
      provided with the distribution.
THIS SOFTWARE IS PROVIDED BY Rafael Muñoz Salinas ''AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL Rafael Muñoz Salinas OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
The views and conclusions contained in the software and documentation are those of the
authors and should not be interpreted as representing official policies, either expressed
or implied, of Rafael Muñoz Salinas.
*/
// Generate grid board with continuous ArUco markers (based on official aruco_print_marker.cpp)
#include "aruco.h"
#include <iostream>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <vector>
using namespace cv;
using namespace std;

// convinience command line parser (copy from official source)
class CmdLineParser
{
    int argc;
    char** argv;
public:
    CmdLineParser(int _argc, char** _argv)
          : argc(_argc)
          , argv(_argv)
    {
    }
    bool operator[](string param)
    {
        int idx = -1;
        for (int i = 0; i < argc && idx == -1; i++)
            if (string(argv[i]) == param)
                idx = i;
        return (idx != -1);
    }
    string operator()(string param, string defvalue = "-1")
    {
        int idx = -1;
        for (int i = 0; i < argc && idx == -1; i++)
            if (string(argv[i]) == param)
                idx = i;
        if (idx == -1)
            return defvalue;
        else
            return (argv[idx + 1]);
    }
};

int main(int argc, char** argv)
{
    try
    {
        CmdLineParser cml(argc, argv);
        if (argc < 2)
        {
            cerr << "Usage: outfile.(jpg|png|ppm|bmp) [options] \n"
                 << "\t-w <num>: columns count (required)\n"
                 << "\t-h <num>: rows count (required)\n"
                 << "\t-startid <num>: first marker id, default 0\n"
                 << "\t-gap <px>: pixel gap between markers, default 10\n"
                 << "\t[-e use enclosing corners]\n"
                 << "\t[-bs <size>:bit size in pixels. 75 by default ] \n"
                 << "\t[-d <dictionary>: ARUCO_MIP_36h12 default] \n"
                 << "\t[-border: adds the white border around each marker]"
                 << "\n\t [-center:  highlights marker center] "
                 << endl;
            auto dict_names = aruco::Dictionary::getDicTypes();
            cerr << "\t\tDictionaries: ";
            for (auto dict : dict_names)
                cerr << dict << " ";
            cerr << endl;
            cerr << "\t Instead of these, you can directly indicate the path to a custom dictionary file" << endl;
            return -1;
        }

        // 1. 解析网格核心参数
        int cols = stoi(cml("-w"));
        int rows = stoi(cml("-h"));
        int startId = stoi(cml("-startid", "0"));
        int gap = stoi(cml("-gap", "600"));
        int totalMarkers = rows * cols;

        // 2. 原版marker绘制参数
        int pixSize = stoi(cml("-bs", "75"));
        bool enclosingCorners = cml["-e"];
        bool waterMark = true;
        bool addBorder = cml["-border"];
        bool drawCenter = cml["-center"];

        // 3. 加载字典（默认ARUCO_MIP_36h12）
        aruco::Dictionary dic = aruco::Dictionary::load(cml("-d", "ARUCO_MIP_36h12"));

        // 4. 批量生成所有marker图像
        vector<Mat> markerList;
        for (int offset = 0; offset < totalMarkers; offset++)
        {
            int mid = startId + offset;
            Mat markerImg = dic.getMarkerImage_id(mid, pixSize, waterMark, enclosingCorners, addBorder, drawCenter);
            markerList.push_back(markerImg);
        }

        // 5. 计算画布总尺寸
        int singleW = markerList[0].cols;
        int singleH = markerList[0].rows;
        int borderMargin = singleW;
        int canvasW = cols * singleW + (cols - 1) * gap + 2 * borderMargin;
        int canvasH = rows * singleH + (rows - 1) * gap + 2 * borderMargin;
        Mat canvas(canvasH, canvasW, CV_8UC1, Scalar(255)); // 白色背景

        // 6. 将每个marker粘贴到画布对应网格位置
        for (int r = 0; r < rows; r++)
        {
            for (int c = 0; c < cols; c++)
            {
                int listIdx = r * cols + c;
                Mat& mk = markerList[listIdx];
                int x = borderMargin + c * (singleW + gap);
                int y = borderMargin + r * (singleH + gap);
                // ROI拷贝
                Mat roi = canvas(Rect(x, y, singleW, singleH));
                mk.copyTo(roi);
            }
        }

        // 7. 输出阵列图片
        imwrite(argv[1], canvas);
        cout << "Grid marker generated successfully!" << endl;
        cout << "Grid size: " << rows << " rows × " << cols << " cols" << endl;
        cout << "Marker ID range: " << startId << " ~ " << startId + totalMarkers - 1 << endl;
        cout << "Canvas border margin (per side): " << borderMargin << " pixels (equal to single marker size)" << endl;
        cout << "Saved to: " << argv[1] << endl;
    }
    catch (std::exception& ex)
    {
        cout << "Error: " << ex.what() << endl;
        return -1;
    }
    return 0;
}