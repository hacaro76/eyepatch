#include "precomp.h"
#include "constants.h"
#include "TrainingSample.h"
#include "TrainingSet.h"
#include "Classifier.h"
#include "GestureClassifier.h"

GestureClassifier::GestureClassifier() :
	Classifier() {

    // set the default "friendly name" and type
    wcscpy(friendlyName, L"Gesture Filter");
    classifierType = GESTURE_FILTER;

    // append identifier to directory name
    wcscat(directoryName, FILE_GESTURE_SUFFIX);
}

GestureClassifier::GestureClassifier(LPCWSTR pathname) :
	Classifier(pathname) {

	USES_CONVERSION;

    WCHAR filename[MAX_PATH];
    wcscpy(filename, pathname);
    wcscat(filename, FILE_DATA_NAME);

	// load the templates from the data file
    FILE *datafile = fopen(W2A(filename), "rb");
	fread(&nTemplates, sizeof(int), 1, datafile);

	for(int i = 0; i < nTemplates; i++) {
		Template t(datafile);
		maxTemplateLength = max(maxTemplateLength,t.GetLength());
		char tname[MAX_PATH];
		sprintf(tname, "Gesture %d", i);
		rec.AddTemplate(tname, t);
    }
    fclose(datafile);

	// set the type
	classifierType = GESTURE_FILTER;

	UpdateTrajectoryImage();
}

GestureClassifier::~GestureClassifier() {

}

void GestureClassifier::StartTraining(TrainingSet *sampleSet) {
    if (isTrained) { // delete the old models
		rec.DeleteUserTemplates();
    }
    maxTemplateLength = 0;

    nTemplates = sampleSet->rangeSampleCount;

	// TODO: call into trainingset class to do this instead of accessing samplemap
	int tNum = 0;
    for (map<UINT, TrainingSample*>::iterator i = sampleSet->sampleMap.begin(); i != sampleSet->sampleMap.end(); i++) {
        TrainingSample *sample = (*i).second;
        if (sample->iGroupId == GROUPID_RANGESAMPLES) { // gesture (range) sample
			char tname[MAX_PATH];
			sprintf(tname, "Gesture %d", tNum);
			rec.AddTemplate(tname, sample->motionTrack);
            maxTemplateLength = max(maxTemplateLength, sample->motionTrack.size());
			tNum++;
		}
    }
    if (isOnDisk) { // this classifier has been saved so we'll update the files
        Save();        
    }

    // update member variables
	isTrained = true;

	// update demo image
	UpdateTrajectoryImage();
}

BOOL GestureClassifier::ContainsSufficientSamples(TrainingSet *sampleSet) {
    return (sampleSet->rangeSampleCount > 0);
}

void GestureClassifier::ClassifyFrame(IplImage *frame, IplImage* guessMask) {
    // not implemented: this class uses ClassifyTrack instead
    assert(false);
}    

void GestureClassifier::ClassifyTrack(MotionTrack mt, IplImage* guessMask) {
    IplImage *newMask = cvCloneImage(guessMask);
    cvZero(newMask);

    // don't start all the way at the beginning of the track if it's really long
    int startFrame = max(0, mt.size()-RHO_MAX*((double)maxTemplateLength));

    cvZero(applyImage);

	Result r = rec.Recognize(mt);

	if (r.m_score > threshold) {
		// draw a rectangle in the new mask image
        CvPoint topLeft = cvPoint(0, 0);
        CvPoint bottomRight = cvPoint(newMask->width, newMask->height);
        cvRectangle(newMask, topLeft, bottomRight, cvScalar(0xFF), CV_FILLED, 8); 

		// draw the recognized gesture in the apply image
		DrawTrack(applyImage, rec.m_templates[r.m_index].m_points, colorSwatch[r.m_index % COLOR_SWATCH_SIZE], 3);

		// print the name of the recognized gesture
		CvFont font;
		cvInitFont(&font,CV_FONT_HERSHEY_COMPLEX_SMALL, 0.55,0.6,0,1, CV_AA);
		cvPutText (applyImage,r.m_name.c_str(),cvPoint(7,20), &font, cvScalar(255,255,255));
	}

    // Combine old and new mask
    // TODO: support OR operation as well
    cvAnd(guessMask, newMask, guessMask);

    IplToBitmap(applyImage, applyBitmap);
	cvReleaseImage(&newMask);
}

void GestureClassifier::UpdateTrajectoryImage() {
	if (nTemplates < 1) return;

	int gridSize = (int) ceil(sqrt((double)nTemplates));
    int gridX = 0;
    int gridY = 0;
    int gridSampleW = FILTERIMAGE_WIDTH / gridSize;
    int gridSampleH = FILTERIMAGE_HEIGHT / gridSize;
    cvZero(filterImage);


	// font for printing name of the gesture
	CvFont font;
	cvInitFont(&font,CV_FONT_HERSHEY_COMPLEX_SMALL, 0.55,0.6,0,1, CV_AA);
	char gestnum[10];

	IplImage *gestureImg = cvCreateImage(cvSize(gridSampleW, gridSampleH), filterImage->depth, filterImage->nChannels);
	for (int i=0; i<nTemplates; i++) {
		cvSetImageROI(filterImage, cvRect(gridX*gridSampleW, gridY*gridSampleH, gridSampleW, gridSampleH));
		cvZero(gestureImg);
		DrawTrack(gestureImg, rec.m_templates[i].m_points, colorSwatch[i % COLOR_SWATCH_SIZE], 3);
		sprintf(gestnum, "%d", i);
		cvPutText(gestureImg, gestnum, cvPoint(7,16), &font, cvScalar(255,255,255));
		cvCopy(gestureImg, filterImage);
		cvResetImageROI(filterImage);
        gridX++;
        if (gridX >= gridSize) {
            gridX = 0;
            gridY++;
        }
	}
	cvReleaseImage(&gestureImg);

    // update demo image
    IplToBitmap(filterImage, filterBitmap);
}

void GestureClassifier::Save() {
    if (!isTrained) return;

	Classifier::Save();

    USES_CONVERSION;
    WCHAR filename[MAX_PATH];

    // save the template data
    wcscpy(filename,directoryName);
    wcscat(filename, FILE_DATA_NAME);
    FILE *datafile = fopen(W2A(filename), "wb");

	// write out the number of templates
	fwrite(&nTemplates, sizeof(int), 1, datafile); 

	// write out all the templates
	for(int i = 0; i < rec.m_templates.size(); i++) {
		rec.m_templates[i].WriteToFile(datafile);
    }
    fclose(datafile);
}