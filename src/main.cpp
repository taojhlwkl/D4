/*
* Copyright (c) 2015 OpenALPR Technology, Inc.
* Open source Automated License Plate Recognition [http://www.openalpr.com]
*
* This file is part of OpenALPR.
*
* OpenALPR is free software: you can redistribute it and/or modify
* it under the terms of the GNU Affero General Public License
* version 3 as published by the Free Software Foundation
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Affero General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include <cstdio>
#include <sstream>
#include <iostream>
#include <iterator>
#include <algorithm>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include "tclap/CmdLine.h"

#include "support/filesystem.h"

#include "support/timing.h"
#include "support/platform.h"
#include "video/videobuffer.h"
#include "motiondetector.h"
#include "alpr.h"
#include "output.h"

using namespace alpr;

const std::string MAIN_WINDOW_NAME = "ALPR main window";

const bool SAVE_LAST_VIDEO_STILL = false;
const std::string LAST_VIDEO_STILL_LOCATION = "/tmp/laststill.jpg";
const std::string WEBCAM_PREFIX = "/dev/video";
MotionDetector motiondetector;
bool do_motiondetection = true;

/** Function Headers */
bool detectandshow(Alpr* alpr, cv::Mat frame, std::string region, bool writeJson);
bool is_supported_image(std::string image_file);

bool measureProcessingTime = false;
std::string templatePattern;

// This boolean is set to false when the user hits terminates (e.g., CTRL+C )
// so we can end infinite loops for things like video processing.
bool program_active = true;

#define TEST



int GetAllgpxFilepathFromfolder(const char*  Path, std::vector<std::string>& filenames);
void outputResult(const AlprResults& results);
std::vector<std::string> filenames;
std::string xlsxFile;
int index = 0;
int main(int argc, const char** argv)
{

	std::string configFile = "";
	bool outputJson = false;
	int seektoms = 0;
	bool detectRegion = false;
	std::string country;
	int topn;
	bool debug_mode = false;
	std::string dirpath = "";

	TCLAP::CmdLine cmd("OpenAlpr Command Line Utility", ' ', Alpr::getVersion());

	//TCLAP::UnlabeledMultiArg<std::string>  fileArg( "image_file", "Image containing license plates", true, "", "image_file_path"  );
	TCLAP::ValueArg<std::string> xlsxPath("x", "xlsxfile", "Path to the xlsx file.", true, "", "xlsxfile_file");

	//TCLAP::ValueArg<std::string> countryCodeArg("c","country","Country code to identify (either us for USA or eu for Europe).  Default=us",false, "us" ,"country_code");
	//TCLAP::ValueArg<int> seekToMsArg("","seek","Seek to the specified millisecond in a video file. Default=0",false, 0 ,"integer_ms");
	TCLAP::ValueArg<std::string> configFileArg("", "config", "Path to the openalpr.conf file", true, "", "config_file");
	TCLAP::ValueArg<std::string> templatePatternArg("p", "pattern", "Attempt to match the plate number against a plate pattern (e.g., md for Maryland, ca for California)", false, "", "pattern code");
	TCLAP::ValueArg<int> topNArg("n", "topn", "Max number of possible plate numbers to return.  Default=10", false, 10, "topN");

	//TCLAP::SwitchArg jsonSwitch("j","json","Output recognition results in JSON format.  Default=off", cmd, false);
	TCLAP::SwitchArg debugSwitch("", "debug", "Enable debug output.  Default=off", cmd, false);
	//TCLAP::SwitchArg detectRegionSwitch("d","detect_region","Attempt to detect the region of the plate image.  [Experimental]  Default=off", cmd, false);
	TCLAP::SwitchArg clockSwitch("", "clock", "Measure/print the total time to process image and all plates.  Default=off", cmd, false);
	//TCLAP::SwitchArg motiondetect("", "motion", "Use motion detection on video file or stream.  Default=off", cmd, false);
	TCLAP::ValueArg<std::string> imgDir("d", "imgdir", "Path to the image dir.", true, "", "imgDir_path");


	//TCLAP::ValueArg<std::string> oppatten("c", "country", "Country code to identify (either us for USA or eu for Europe).  Default=us", false, "us", "country_code");

	try
	{
		cmd.add(templatePatternArg);
		//cmd.add( seekToMsArg );
		cmd.add(topNArg);
		cmd.add(configFileArg);
		cmd.add(xlsxPath);
		cmd.add(imgDir);
		//cmd.add( fileArg );
		//cmd.add( countryCodeArg );

#ifndef TEST
		if (cmd.parse(argc, argv) == false)
		{
			// Error occurred while parsing.  Exit now.
			return 1;
		}
#endif
		//filenames = fileArg.getValue();

		//country = countryCodeArg.getValue();
		//   seektoms = seekToMsArg.getValue();
		//   outputJson = jsonSwitch.getValue();
		//detectRegion = detectRegionSwitch.getValue();
		//do_motiondetection = motiondetect.getValue();
		seektoms = 0;
		outputJson = 0;
		detectRegion = 0;
		do_motiondetection = 0;
		debug_mode = debugSwitch.getValue();
		configFile = configFileArg.getValue();
		templatePattern = templatePatternArg.getValue();
		topn = topNArg.getValue();
		measureProcessingTime = clockSwitch.getValue();
		xlsxFile = xlsxPath.getValue();
		dirpath = imgDir.getValue();
	}
	catch (TCLAP::ArgException &e)    // catch any exceptions
	{
		std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
		return 1;
	}
	country = (std::string)"ch";
	templatePattern = (std::string)"xx";
#ifdef TEST
	configFile = std::string("D:/project/D4/runtime_data/config/ch.conf");
	//debug_mode = true;
#endif
	//filenames.push_back((std::string)"D:/project/D4/Task3_车牌识别/功能评测图像库/省市简称变化子库/“贵”牌/贵A3885T.jpg");


	/****** read from fileDir ***********/
	//std::string dirpath = "D:/third_party/openalpr-2.3.0/Task3_车牌识别/功能评测图像库/车牌种类变化子库/小型汽车牌";
	//GetAllgpxFilepathFromfolder(dirpath.c_str(), filenames);

	/****** read from xlsx ***********/
#ifdef TEST 
	xlsxFile = "D:/project/D4/test.xlsx";
#endif
	/////////////////////////////
	//generate *.xlsx if not has. 
	////////////////////////////
#ifdef TEST
	//std::string dirpath = "D:/third_party/openalpr-2.3.0/Task3_车牌识别";
	//dirpath = "D:/project/D4/Task3_车牌识别/功能评测图像库/车牌种类变化子库/大型汽车后牌";
	dirpath = "D:/project/D4/Task3_车牌识别/功能评测图像库/省市简称变化子库";
#endif
	GetAllgpxFilepathFromfolder(dirpath.c_str(), filenames);
	auto iter = filenames.begin();
	for (; iter != filenames.end();)
	{
		if (!is_supported_image(*iter))
		{
			iter = filenames.erase(iter);
		}
		else
			iter++;
	}
	generateXlsxFlie(xlsxFile, filenames);
	filenames.clear();
	/////////////////////////

	/////////////////////////////
	//read files form *.xlsx  
	////////////////////////////
	readFilePathFromXlsx(xlsxFile, filenames);

	cv::Mat frame;

	Alpr alpr(country, configFile);
	alpr.setTopN(topn);

	if (debug_mode)
	{
		alpr.getConfig()->setDebug(true);
	}

	if (detectRegion)
		alpr.setDetectRegion(detectRegion);

	if (templatePattern.empty() == false)
		alpr.setDefaultRegion(templatePattern);

	if (alpr.isLoaded() == false)
	{
		std::cerr << "Error loading OpenALPR" << std::endl;
		return 1;
	}

	for (unsigned int i = 0; i < filenames.size(); i++)
	{
		std::string filename = filenames[i];

		if (filename == "stdin")
		{
			std::string filename;
			while (std::getline(std::cin, filename))
			{
				if (fileExists(filename.c_str()))
				{
					frame = cv::imread(filename);
					detectandshow(&alpr, frame, "", outputJson);
				}
				else
				{
					std::cerr << "Image file not found: " << filename << std::endl;
				}

			}
		}
		else if (is_supported_image(filename))
		{
			//q debug
			size_t start = filename.rfind("/");
			size_t end = filename.find(".");
			std::string file = filename.substr(start+1, end - start-1);
			Config* cin = alpr.getConfig();
			size_t start1 = xlsxFile.rfind("/");
			std::string xx(xlsxFile, 0, start1);
			cin->outputPath = xx +"/output/";
			//cin->outputPath = "D:/project/D4/output/";
			cin->fileName = file;
			cin->q_debug = 1;
			//q debug
			if (fileExists(filename.c_str()))
			{
				frame = cv::imread(filename);

				bool plate_found = detectandshow(&alpr, frame, "", outputJson);

				if (!plate_found && !outputJson)
				{
					std::cout << "No license plates found." << std::endl;
				}

			}
			else
			{
				std::cerr << "Image file not found: " << filename << std::endl;
			}
		}
		else if (DirectoryExists(filename.c_str()))
		{
			std::vector<std::string> files = getFilesInDir(filename.c_str());

			std::sort(files.begin(), files.end(), stringCompare);

			for (int i = 0; i < files.size(); i++)
			{
				if (is_supported_image(files[i]))
				{
					std::string fullpath = filename + "/" + files[i];
					std::cout << fullpath << std::endl;
					frame = cv::imread(fullpath.c_str());
					if (detectandshow(&alpr, frame, "", outputJson))
					{
						//while ((char) cv::waitKey(50) != 'c') { }
					}
					else
					{
						//cv::waitKey(50);
					}
				}
			}
		}
		else
		{
			std::cerr << "Unknown file type" << std::endl;
			return 1;
		}
	}

	return 0;
}

bool is_supported_image(std::string image_file)
{
	return (hasEndingInsensitive(image_file, ".png") || hasEndingInsensitive(image_file, ".jpg") ||
		hasEndingInsensitive(image_file, ".tif") || hasEndingInsensitive(image_file, ".bmp") ||
		hasEndingInsensitive(image_file, ".jpeg") || hasEndingInsensitive(image_file, ".gif"));
}


bool detectandshow(Alpr* alpr, cv::Mat frame, std::string region, bool writeJson)
{

	timespec startTime;
	getTimeMonotonic(&startTime);

	std::vector<AlprRegionOfInterest> regionsOfInterest;
	//先验的感兴趣区域
	if (do_motiondetection)
	{
		cv::Rect rectan = motiondetector.MotionDetect(&frame);
		if (rectan.width>0) regionsOfInterest.push_back(AlprRegionOfInterest(rectan.x, rectan.y, rectan.width, rectan.height));
	}
	else regionsOfInterest.push_back(AlprRegionOfInterest(0, 0, frame.cols, frame.rows));
	//所有流程--提取+识别得到resulit
	AlprResults results;
	if (regionsOfInterest.size()>0) results = alpr->recognize(frame.data, frame.elemSize(), frame.cols, frame.rows, regionsOfInterest);

	//输出result
	timespec endTime;
	getTimeMonotonic(&endTime);
	double totalProcessingTime = diffclock(startTime, endTime);
	if (measureProcessingTime)
		std::cout << "Total Time to process image: " << totalProcessingTime << "ms." << std::endl;


	if (writeJson)
	{
		std::cout << alpr->toJson(results) << std::endl;
	}
	else
	{
		//q output one result
		std::cout << filenames[index] << std::endl;
		if (results.plates.size()) {
			outputResult(results);
		}
		else {
			writeOneIndexToExcle(xlsxFile, index, "无", "无","无","无");
			index++;
		}

		for (int i = 0; i < results.plates.size(); i++)
		{
			std::cout << "plate" << i << ": " << results.plates[i].topNPlates.size() << " results";
			if (measureProcessingTime)
				std::cout << " -- Processing Time = " << results.plates[i].processing_time_ms << "ms.";
			std::cout << std::endl;

			std::cout << "      Background Color : " << results.plates[i].color << std::endl;

			if (results.plates[i].regionConfidence > 0)
				std::cout << "State ID: " << results.plates[i].region << " (" << results.plates[i].regionConfidence << "% confidence)" << std::endl;

			for (int k = 0; k < results.plates[i].topNPlates.size(); k++)
			{
				// Replace the multiline newline character with a dash
				std::string no_newline = results.plates[i].topNPlates[k].characters;
				std::replace(no_newline.begin(), no_newline.end(), '\n', '-');

				//char* symbol = no_newline.c_str();
				wchar_t *tempchar;
				char * resulttemp;
				//char* text;
				char* font1;
				//text = api.GetUTF8Text();

				tempchar = Utf_8ToUnicode(no_newline.c_str());
				resulttemp = UnicodeToAnsi(tempchar);
				no_newline = resulttemp;

				std::cout << "    - " << no_newline << "\t confidence: " << results.plates[i].topNPlates[k].overall_confidence;
				if (templatePattern.size() > 0 || results.plates[i].regionConfidence > 0)
					std::cout << "\t pattern_match: " << results.plates[i].topNPlates[k].matches_template;

				std::cout << std::endl;

			}
		}

	}



	return results.plates.size() > 0;
}

int GetAllgpxFilepathFromfolder(const char*  Path, std::vector<std::string>& filenames)
{
	char szFind[MAX_PATH];
	WIN32_FIND_DATA FindFileData;
	strcpy(szFind, Path);
	strcat(szFind, "\\*.*");
	HANDLE hFind = FindFirstFile(szFind, &FindFileData);
	if (INVALID_HANDLE_VALUE == hFind)
		return -1;

	do
	{
		if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (strcmp(FindFileData.cFileName, ".") != 0 && strcmp(FindFileData.cFileName, "..") != 0)
			{
				//发现子目录，递归之  
				char szFile[MAX_PATH] = { 0 };
				strcpy(szFile, Path);
				strcat(szFile, "/");
				strcat(szFile, FindFileData.cFileName);
				GetAllgpxFilepathFromfolder(szFile, filenames);
			}
		}
		else
		{
			//找到文件，处理之  
			std::string fileName = std::string(Path) + "/" + std::string(FindFileData.cFileName);
			filenames.push_back(fileName);
			//std::cout << Path << "/" << FindFileData.cFileName << std::endl;
		}
	} while (FindNextFile(hFind, &FindFileData));

	FindClose(hFind);

	return 0;
}


void outputResult(const AlprResults& results) {
	std::string color = "color";
	std::string plate = "ANSC是";
	int idx = 0;
	float Confidence = 0.0;

	//for (int i = 0; i < results.plates.size(); i++)
	//{
	// Confidence = std::max(Confidence, results.plates[i].topNPlates[0].overall_confidence);
	//}
	//assert(results.plates.size());
	//assert(results.plates[0].topNPlates.size());

	std::string match_one = "错误";
	std::string match_correct = "不存在正确结果";
	std::string filename = filenames[index];
	size_t start = filename.rfind("/");
	size_t end = filename.find(".");
	std::string file = filename.substr(start + 1, end - start - 1);
	//Verified By Double
	std::string no_newline="";
	std::string tem = "";
	int k = 0;
	int pattern_match;
	for (; k < results.plates[0].topNPlates.size(); k++)
	{
		pattern_match= results.plates[0].topNPlates[k].matches_template;
		if ( pattern_match)
		{
			if (no_newline == "")
			{
				tem = results.plates[0].topNPlates[k].characters;
				no_newline = tem;
				std::replace(tem.begin(), tem.end(), '\n', '-');
				wchar_t *tempchar;
				char * resulttemp;

				tempchar = Utf_8ToUnicode(tem.c_str());
				resulttemp = UnicodeToAnsi(tempchar);
				tem = resulttemp;
				if (tem == file)
				{
					match_one = "正确";
					match_correct = "存在正确结果";
					break;
				}
				else
				{
					match_one = "错误";
				}
			}
			else
			{
				tem = results.plates[0].topNPlates[k].characters;
				std::replace(tem.begin(), tem.end(), '\n', '-');
				wchar_t *tempchar;
				char * resulttemp;
				tempchar = Utf_8ToUnicode(tem.c_str());
				resulttemp = UnicodeToAnsi(tempchar);
				tem = resulttemp;
				if (tem == file)
				{
					match_correct = "存在正确结果";
					break;
				}
			}
		}
	}
	//Verified By Double
	std::replace(no_newline.begin(), no_newline.end(), '\n', '-');
	wchar_t *tempchar;
	char * resulttemp;

	tempchar = Utf_8ToUnicode(no_newline.c_str());
	resulttemp = UnicodeToAnsi(tempchar);
	no_newline = resulttemp;
	if (!pattern_match)
	{
		no_newline = "无";
	}
	plate = no_newline;
	color = results.plates[0].color;
	//writeOneIndexToExcle(xlsxFile, index, plate, color);
	writeOneIndexToExcle(xlsxFile, index, plate, color,match_one,match_correct);
	index++;
}