//============================================================================
// Name : ComputerVision.cpp
// Author : Luke Hsiao, Clover Wu
// Version :
// Copyright : Copyright 2015 Team Vektor Krum
// Description : This program receives video data from the Soccer Field's
// overhead camera, processes the images, and outputs
// the (x,y) positions of all 4 robots, the ball, and all
// robot's orientations.
//============================================================================

#include <sstream>
#include <string>
#include <iostream>
#include <opencv2/highgui/highgui.hpp>
#include <opencv/cv.h>
#include "Ball.h"
#include "Robot.h"
#include "Object.h"

using namespace cv;

//initial min and max HSV filter values.
//these will be changed using trackbars
int H_MIN = 0;
int H_MAX = 256;
int S_MIN = 0;
int S_MAX = 256;
int V_MIN = 0;
int V_MAX = 256;

//default capture width and height
const int FRAME_WIDTH = 640;
const int FRAME_HEIGHT = 480;

//max number of objects to be detected in frame
const int MAX_NUM_OBJECTS=50;

//minimum and maximum object area
const int MIN_OBJECT_AREA = 40*40;
const int MAX_OBJECT_AREA = FRAME_HEIGHT*FRAME_WIDTH/1.5;

//names that will appear at the top of each window
const string windowName = "Original Image";
const string windowName1 = "HSV Image";
const string windowName2 = "Thresholded Image";
const string windowName3 = "After Morphological Operations";
const string trackbarWindowName = "Trackbars";

// This function is called whenever a trackbar changes
void on_trackbar( int, void* ) {

}
string intToString(int number) {
  std::stringstream ss;
  ss << number;
  return ss.str();
}
void createTrackbars() {
	//create window for trackbars
	namedWindow(trackbarWindowName,0);

	//create memory to store trackbar name on window
	char TrackbarName[50];
	sprintf( TrackbarName, "H_MIN", H_MIN);
	sprintf( TrackbarName, "H_MAX", H_MAX);
	sprintf( TrackbarName, "S_MIN", S_MIN);
	sprintf( TrackbarName, "S_MAX", S_MAX);
	sprintf( TrackbarName, "V_MIN", V_MIN);
	sprintf( TrackbarName, "V_MAX", V_MAX);

	//create trackbars and insert them into window
	//3 parameters are: the address of the variable that is changing when the trackbar is moved(eg.H_LOW),
	//the max value the trackbar can move (eg. H_HIGH), 
	//and the function that is called whenever the trackbar is moved(eg. on_trackbar)
	createTrackbar( "H_MIN", trackbarWindowName, &H_MIN, H_MAX, on_trackbar );
	createTrackbar( "H_MAX", trackbarWindowName, &H_MAX, H_MAX, on_trackbar );
	createTrackbar( "S_MIN", trackbarWindowName, &S_MIN, S_MAX, on_trackbar );
	createTrackbar( "S_MAX", trackbarWindowName, &S_MAX, S_MAX, on_trackbar );
	createTrackbar( "V_MIN", trackbarWindowName, &V_MIN, V_MAX, on_trackbar );
	createTrackbar( "V_MAX", trackbarWindowName, &V_MAX, V_MAX, on_trackbar );
}

// Places a small circle on the object
void drawObject(int x,int y,Mat &frame) {
	circle(frame,cv::Point(x,y),10,cv::Scalar(0,0,255));
	putText(frame,intToString(x)+ " , " + intToString(y),
	        Point(x,y+20),1,1,Scalar(0,255,0));
}

void drawBall(Ball soccerBall, Mat &frame) {
  int x = soccerBall.get_x_pos();
  int y = soccerBall.get_y_pos();
  circle(frame,cv::Point(x,y),10,cv::Scalar(0,0,255));
  putText(frame,"(" + intToString(x)+ "," + intToString(y) + ")",
          Point(x,y+20),1,1,Scalar(0,255,0));
  putText(frame, "Ball", Point(x+25,y+35),1,1,Scalar(0,255,0));
}

void drawRobot(Robot newRobot, Mat &frame) {
  int x = newRobot.get_x_pos();
  int y = newRobot.get_y_pos();
  int team = newRobot.getTeam();
  int angle = newRobot.getAngle();

  circle(frame,cv::Point(x,y),10,cv::Scalar(0,0,255));
  putText(frame,"(" + intToString(x)+ "," + intToString(y) + ")",
          Point(x,y+20),1,1,Scalar(0,255,0));
  putText(frame, "Robot", Point(x+20,y+35),1,1,Scalar(0,255,0));
  putText(frame, "Team " + team, Point(x+20,y+60),1,1,Scalar(0,255,0));
  putText(frame, "angle: " + angle, Point(x+20,y+75),1,1,Scalar(0,255,0));
}

// Draws all robot objects that are found
void drawAllRobots(vector<Robot> robots_to_draw, Mat &frame) {
  // Iterate through all found robots and draw them
  for (int i = 0; i < robots_to_draw.size(); i++) {
    drawRobot(robots_to_draw.at(i), frame);
  }
}

// This function reduces the noise of the image by eroding the image first
// then dialating the remaining image to produce cleaner objects
void morphOps(Mat &thresh) {
	//create structuring element that will be used to "dilate" and "erode" image.

	//the element chosen here is a 3px by 3px rectangle
	Mat erodeElement = getStructuringElement( MORPH_RECT,Size(3,3));
	//dilate with larger element so make sure object is nicely visible
	Mat dilateElement = getStructuringElement( MORPH_RECT,Size(8,8));

	erode(thresh,thresh,erodeElement);
	erode(thresh,thresh,erodeElement);

	dilate(thresh,thresh,dilateElement);
	dilate(thresh,thresh,dilateElement);
}

// Finds the contours (outlines) of the now filtered image and determine's its
// center by examining its moments.
void trackFilteredObject(Mat threshold, Mat HSV, Mat &cameraFeed) {

	Ball ball;
	vector<Robot> allRobots; //stores all robot locations

	Mat temp;
	threshold.copyTo(temp);

	//these two vectors needed for output of findContours
	vector< vector<Point> > contours;
	vector<Vec4i> hierarchy;

	//find contours of filtered image using openCV findContours function
	findContours(temp,contours,hierarchy,CV_RETR_CCOMP,CV_CHAIN_APPROX_SIMPLE );

	//use moments method to find our filtered object
	double refArea = 0;
	bool objectFound = false;
	if (hierarchy.size() > 0) {
		int numObjects = hierarchy.size();
		//if number of objects greater than MAX_NUM_OBJECTS we have a noisy filter
		if(numObjects<MAX_NUM_OBJECTS){
			for (int index = 0; index >= 0; index = hierarchy[index][0]) {

				Moments moment = moments((Mat)contours[index]);
				double area = moment.m00;

				//if the area is less than 20 px by 20px then it is probably just noise
				//if the area is the same as the 3/2 of the image size, probably just a bad filter
				//we only want the object with the largest area so we safe a reference area each
				//iteration and compare it to the area in the next iteration.
				if(area>MIN_OBJECT_AREA){
					ball.set_x_pos(moment.m10/area);
					ball.set_y_pos(moment.m01/area);
					objectFound = true;
				}
				else {
				  objectFound = false;
				}
			}
			//let user know you found an object
			if(objectFound ==true){
				//draw object location on screen
				drawBall(ball,cameraFeed);}

		}
		else {
		  putText(cameraFeed,"TOO MUCH NOISE! ADJUST FILTER",Point(0,50),1,2,Scalar(0,0,255),2);
		}
	}
}

int main(int argc, char* argv[]) {
	//if we would like to calibrate our filter values, set to true.
	bool calibrationMode = true;
	
	//Matrix to store each frame of the webcam feed
	Mat cameraFeed;
	Mat threshold;
	Mat HSV;

	if(calibrationMode){
		//create slider bars for HSV filtering
		createTrackbars();
	}
	//video capture object to acquire webcam feed
	VideoCapture capture;
	//open capture object at location zero (default location for webcam)
	capture.open(0);

	//set height and width of capture frame
	capture.set(CV_CAP_PROP_FRAME_WIDTH,FRAME_WIDTH);
	capture.set(CV_CAP_PROP_FRAME_HEIGHT,FRAME_HEIGHT);

	//start an infinite loop where webcam feed is copied to cameraFeed matrix
	//all of our operations will be performed within this loop
	while(1) {
		//store image to matrix
		capture.read(cameraFeed);
		//convert frame from BGR to HSV colorspace
		cvtColor(cameraFeed,HSV,COLOR_BGR2HSV);

		if(calibrationMode==true) {
      //if in calibration mode, we track objects based on the HSV slider values.
      cvtColor(cameraFeed,HSV,COLOR_BGR2HSV);
      inRange(HSV,Scalar(H_MIN,S_MIN,V_MIN),Scalar(H_MAX,S_MAX,V_MAX),threshold);

      // Erode, then dialate to get a cleaner image
      morphOps(threshold);

      imshow(windowName2,threshold);
      trackFilteredObject(threshold,HSV,cameraFeed);
		}
		else {
		  // When NOT in calibration mode, use actual hard-coded color values

		  Robot home1, home2;
		  Robot away1, away2;
		  Ball ball;

		  ball.setHSVmin(cv::Scalar(0,0,0));
		  ball.setHSVmin(Scalar(255,255,255));

      cvtColor(cameraFeed,HSV,COLOR_BGR2HSV);
      inRange(HSV,ball.getHSVmin(),ball.getHSVmax(),threshold);
      // Erode, then dialate to get a cleaner image
      morphOps(threshold);
      trackFilteredObject(threshold,HSV,cameraFeed);
		}



		imshow(windowName,cameraFeed);
		//imshow(windowName1,HSV);

		//delay 30ms so that screen can refresh.
		//image will not appear without this waitKey() command
		waitKey(30);
	}
	return 0;
}
