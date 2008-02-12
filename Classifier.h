#pragma once
#include "ClassifierOutputData.h"

class Classifier
{
public:

	Classifier() {
	    isTrained = false;
        isOnDisk = false;
        classifierType = 0;
		threshold = 0.5;
        filterImage = cvCreateImage(cvSize(FILTERIMAGE_WIDTH, FILTERIMAGE_HEIGHT), IPL_DEPTH_8U, 3);
        applyImage = cvCreateImage(cvSize(FILTERIMAGE_WIDTH, FILTERIMAGE_HEIGHT), IPL_DEPTH_8U, 3);
		guessMask = cvCreateImage(cvSize(GUESSMASK_WIDTH, GUESSMASK_HEIGHT), IPL_DEPTH_8U, 1);
        filterBitmap = new Bitmap(FILTERIMAGE_WIDTH, FILTERIMAGE_HEIGHT, PixelFormat24bppRGB);
        applyBitmap = new Bitmap(FILTERIMAGE_WIDTH, FILTERIMAGE_HEIGHT, PixelFormat24bppRGB);

        // set the standard "friendly name"
        wcscpy(friendlyName, L"Generic Classifier");

        // create a directory to store this in
        WCHAR rootpath[MAX_PATH];
        SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, rootpath);
        int classifiernum = (int)time(0);
        wsprintf(directoryName, L"%s\\%s\\%s%d", rootpath, APP_CLASS, FILE_CLASSIFIER_PREFIX, classifiernum);

		// create list of available output variables
		variableNames.push_back("Mask");
		variableNames.push_back("Contours");

		// Initialize contour storage
		contourStorage = cvCreateMemStorage(0);
	}

	Classifier(LPCWSTR pathname) {
		USES_CONVERSION;

		isTrained = true;
        isOnDisk = true;
        classifierType = 0;
		threshold = 0.5;

		filterImage = cvCreateImage(cvSize(FILTERIMAGE_WIDTH, FILTERIMAGE_HEIGHT), IPL_DEPTH_8U, 3);
        applyImage = cvCreateImage(cvSize(FILTERIMAGE_WIDTH, FILTERIMAGE_HEIGHT), IPL_DEPTH_8U, 3);
		guessMask = cvCreateImage(cvSize(GUESSMASK_WIDTH, GUESSMASK_HEIGHT), IPL_DEPTH_8U, 1);
        filterBitmap = new Bitmap(FILTERIMAGE_WIDTH, FILTERIMAGE_HEIGHT, PixelFormat24bppRGB);
        applyBitmap = new Bitmap(FILTERIMAGE_WIDTH, FILTERIMAGE_HEIGHT, PixelFormat24bppRGB);

		// save the directory name for later
		wcscpy(directoryName, pathname);

		// load the "friendly name"
		WCHAR filename[MAX_PATH];
		wcscpy(filename, pathname);
		wcscat(filename, FILE_FRIENDLY_NAME);
		FILE *namefile = fopen(W2A(filename), "r");
		fgetws(friendlyName, MAX_PATH, namefile);
		fclose(namefile);

		// load the threshold
		wcscpy(filename, pathname);
		wcscat(filename, FILE_THRESHOLD_NAME);
		FILE *threshfile = fopen(W2A(filename), "r");
		fread(&threshold, sizeof(float), 1, threshfile);
		fclose(threshfile);

		// create list of available output variables
		variableNames.push_back("Mask");
		variableNames.push_back("Contours");

		// Initialize contour storage
		contourStorage = cvCreateMemStorage(0);
	}

	virtual ~Classifier() {
        cvReleaseImage(&filterImage);
        cvReleaseImage(&applyImage);
		cvReleaseImage(&guessMask);
		cvReleaseMemStorage(&contourStorage);
        delete filterBitmap;
        delete applyBitmap;
    }

	virtual void StartTraining(TrainingSet*) = 0;
    virtual BOOL ContainsSufficientSamples(TrainingSet*) = 0;
	virtual ClassifierOutputData ClassifyFrame(IplImage*) = 0;
	virtual void ResetRunningState() = 0;

	virtual void Save() {
		USES_CONVERSION;
		WCHAR filename[MAX_PATH];

		// make sure the directory exists 
	    SHCreateDirectory(NULL, directoryName);

		// save the "friendly name"
		wcscpy(filename,directoryName);
		wcscat(filename, FILE_FRIENDLY_NAME);
		FILE *namefile = fopen(W2A(filename), "w");
		fputws(friendlyName, namefile);
		fclose(namefile);

		// save the threshold
		wcscpy(filename,directoryName);
		wcscat(filename, FILE_THRESHOLD_NAME);
		FILE *threshfile  = fopen(W2A(filename), "w");
		fwrite(&threshold, sizeof(float), 1, threshfile);
		fclose(threshfile);

		isOnDisk = true;
	}

    void DeleteFromDisk() {
        if (!isOnDisk) return;
        DeleteDirectory(directoryName, true);
        isOnDisk = false;
    }

	CvSeq* GetMaskContours() {
        // reset the contour storage
        cvClearMemStorage(contourStorage);

        CvSeq* contours = NULL;
		cvFindContours(guessMask, contourStorage, &contours, sizeof(CvContour), CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, cvPoint(0,0));
		if (contours != NULL) {
	        contours = cvApproxPoly(contours, sizeof(CvContour), contourStorage, CV_POLY_APPROX_DP, 3, 1 );
		}
		return contours;
	}

    Bitmap* GetFilterImage() {
        return filterBitmap;
    }
    Bitmap* GetApplyImage() {
        return applyBitmap;
    }
    LPWSTR GetName() {
        return friendlyName;
    }
    void SetName(LPCWSTR newName) {
        wcscpy(friendlyName, newName);
    }

	bool isTrained;
    bool isOnDisk;
    int classifierType;
	float threshold;
	vector<string> variableNames;

protected:
    Bitmap *filterBitmap, *applyBitmap;
    IplImage *filterImage, *applyImage, *guessMask;
	CvMemStorage *contourStorage;

    WCHAR friendlyName[MAX_PATH];
    WCHAR directoryName[MAX_PATH];
};
