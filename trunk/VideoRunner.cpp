#include "precomp.h"
#include "constants.h"
#include "TrainingSample.h"
#include "TrainingSet.h"
#include "Classifier.h"
#include "OutputSink.h"
#include "MotionClassifier.h"
#include "GestureClassifier.h"
#include "VideoRunner.h"

CVideoRunner::CVideoRunner(CWindow *caller) {
    videoCapture = NULL;
    copyFrame = NULL;
    outputAccImage = NULL;
    motionHistory = NULL;
    bmpInput = NULL;
    bmpOutput = NULL;
	bmpMotion = NULL;
	bmpGesture = NULL;
    processingVideo = false;
	runningLive = true;
    nFrames = 0;
	framesAvailable = 0;
    parent = caller;

    trackingMotion = 0;
    trackingBlobs = 0;

	m_hMutex = NULL;
    m_hThread = NULL;

    CreateBlobTracker();
}

CVideoRunner::~CVideoRunner(void) {
    StopProcessing();
	if (m_hMutex) CloseHandle(m_hMutex);
}

void CVideoRunner::CreateBlobTracker() {
        // Set number of foreground training frames
    param.FGTrainFrames = GESTURE_NUM_FGTRAINING_FRAMES;

    // Create FG Detection module
    param.pFG = cvCreateFGDetectorBase(CV_BG_MODEL_FGD, NULL);

    // Create Blob Entrance Detection module
    param.pBD = cvCreateBlobDetectorCC();
    param.pBD->SetParam("MinDistToBorder",1.0);
    param.pBD->SetParam("Latency",5);
    param.pBD->SetParam("HMin",0.08);
    param.pBD->SetParam("WMin",0.08);

    // Create blob tracker module
    param.pBT = cvCreateBlobTrackerCCMSPF();

    // create blob trajectory generation module (currently not needed)
    param.pBTGen = (CvBlobTrackGen*) &trajectories;

    // create blob trajectory post processing module
    param.pBTPP = cvCreateModuleBlobTrackPostProcKalman();

    // create blob trajectory analysis module (currently not needed)
    param.UsePPData = 0;
    param.pBTA = NULL;

    // create a pipeline using these components
    pTracker = cvCreateBlobTrackerAuto1(&param);
}

void CVideoRunner::DestroyBlobTracker() {
    
    // Release all the modules
    if(param.pBT)cvReleaseBlobTracker(&param.pBT);
    if(param.pBD)cvReleaseBlobDetector(&param.pBD);
    if(param.pBTGen)cvReleaseBlobTrackGen(&param.pBTGen);
    if(param.pBTA)cvReleaseBlobTrackAnalysis(&param.pBTA);
    if(param.pFG)cvReleaseFGDetector(&param.pFG);
    if(pTracker)cvReleaseBlobTrackerAuto(&pTracker);
}

void CVideoRunner::ProcessFrame() {
    USES_CONVERSION;
	if (currentFrame == NULL) return;

    WaitForSingleObject(m_hMutex,INFINITE);

    // load frame and flip if needed
	if (currentFrame->origin  == IPL_ORIGIN_TL) {
		cvCopy(currentFrame,copyFrame);
	} else {
		cvFlip(currentFrame,copyFrame);
	}

    if (trackingMotion) { // we have a motion filter in the chain, so compute motion
        ProcessMotionFrame();
    }

    if (trackingBlobs) { // we have a gesture filter in the chain, so grab trajectories
        ProcessBlobFrame();
    }

    // some outputs run on the original (unfiltered) frame
	// we apply these before applying any of the filters
    for (list<OutputSink*>::iterator j=activeOutputs.begin(); j!=activeOutputs.end(); j++) {
        (*j)->ProcessInput(copyFrame);
    }


    // First black out the output frame
    cvZero(outputFrame);
    int nFiltersInChain = activeClassifiers.size();
    int nCurrentFilter = 0;

    // apply filter chain to frame
    for (list<Classifier*>::iterator i=activeClassifiers.begin(); i!=activeClassifiers.end(); i++) {

        // start with a full mask (all on)
        // TODO: support AND operation as well
        cvSet(guessMask, cvScalar(0xFF));

        if ((*i)->classifierType == MOTION_FILTER) {
            ((MotionClassifier*)(*i))->ClassifyMotion(motionHistory, nFrames, guessMask);
        } else if ((*i)->classifierType == GESTURE_FILTER) {
            vector<MotionTrack> trackList;
            trajectories.GetCurrentTracks(&trackList);
            if (trackList.size() == 0) {
                cvZero(guessMask);
            } else {
//				for (int trackNum=0; trackNum < trackList.size(); trackNum++) {
				// BLAH -- only checking first active track
				for (int trackNum=0; trackNum< 1; trackNum++) {
	                ((GestureClassifier*)(*i))->ClassifyTrack(trackList[trackNum], guessMask);
				}
            }
        } else {
            (*i)->ClassifyFrame(copyFrame, guessMask);
        } 

        // Copy the masked output of this filter to accumulator frame
        cvZero(outputAccImage);
        cvCopy(copyFrame, outputAccImage, guessMask);

        // Find contours in mask image and trace in accumulator frame
        CvSeq* contours = NULL;
        cvFindContours(guessMask, contourStorage, &contours, sizeof(CvContour), CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, cvPoint(0,0));
        if (contours != NULL) {
            contours = cvApproxPoly(contours, sizeof(CvContour), contourStorage, CV_POLY_APPROX_DP, 3, 1 );
            cvDrawContours(outputAccImage, contours, colorSwatch[nCurrentFilter%COLOR_SWATCH_SIZE], CV_RGB(0,0,0), 1, 2, CV_AA);
        }
        nCurrentFilter++;

        // Add masked accumulator frame to output frame
        cvAddWeighted(outputAccImage, (1.0/nFiltersInChain), outputFrame, 1.0, 0, outputFrame);

        // apply output chain to filtered frame
        for (list<OutputSink*>::iterator j=activeOutputs.begin(); j!=activeOutputs.end(); j++) {
            (*j)->ProcessOutput(copyFrame, guessMask, contours, W2A((*i)->GetName()));
        }

        // reset the contour storage
        cvClearMemStorage(contourStorage);
    }

    // convert to bitmap
    IplToBitmap(copyFrame, bmpInput);
    IplToBitmap(outputFrame, bmpOutput);

    // invalidate parent rectangle for redraw
    CRect videoRect(FILTERLIBRARY_WIDTH, 0, WINDOW_X, WINDOW_Y);
    if (parent->IsWindow()) {
        parent->InvalidateRect(&videoRect, FALSE);
    }

    // update frame count and release the mutex
    nFrames++;
    ReleaseMutex(m_hMutex);

    // Grab next frame (do this AFTER releasing mutex)
	if (runningLive || (nFrames < framesAvailable)) {
		currentFrame = cvQueryFrame(videoCapture);
	} else {	// we are all out of recorded video frames
		parent->SendMessage(WM_COMMAND, IDC_RUNRECORDED, 0);
	}
}

void CVideoRunner::ProcessMotionFrame() {
    // convert frame to grayscale for motion history computation
    cvCvtColor(copyFrame, motionBuf[last], CV_BGR2GRAY);
    int idx1 = last;
    int idx2 = (last + 1) % MOTION_NUM_IMAGES;
    last = idx2;

    // get difference between frames
    IplImage* silh = motionBuf[idx2];
    cvAbsDiff(motionBuf[idx1], motionBuf[idx2], silh);

    // threshold difference image and use it to update motion history image
    cvThreshold(silh, silh, MOTION_DIFF_THRESHOLD, 1, CV_THRESH_BINARY); 
    cvUpdateMotionHistory(silh, motionHistory, nFrames, MOTION_MHI_DURATION); // update MHI

    // convert MHI to blue 8U image
    IplImage *mask = cvCreateImage(cvSize(motionHistory->width, motionHistory->height), IPL_DEPTH_8U, 1);
    IplImage *dst = cvCreateImage(cvSize(motionHistory->width, motionHistory->height), IPL_DEPTH_8U, 3);
    cvCvtScale(motionHistory, mask, 255./MOTION_MHI_DURATION,(MOTION_MHI_DURATION-nFrames)*255./MOTION_MHI_DURATION);
    cvZero(dst);
    cvCvtPlaneToPix( mask, 0, 0, 0, dst );

    IplToBitmap(dst, bmpMotion);
	cvReleaseImage(&mask);
	cvReleaseImage(&dst);
}

void CVideoRunner::ProcessBlobFrame() {
    // Process the new frame with the blob tracker
    // the second parameter is an optional mask
    pTracker->Process(copyFrame, NULL);

    // we don't need to keep the old trajectories around
    trajectories.DeleteOldTracks();

	// remainder of this function is just to create the blob status image
    // make a color copy of the grayscale foreground mask
    IplImage* fgImage = pTracker->GetFGMask();
    IplImage *fgImageCopy = cvCloneImage(copyFrame);
    IplImage *maskedCopyFrame = cvCloneImage(copyFrame);
    cvCvtColor(fgImage, fgImageCopy, CV_GRAY2BGR );

    // mask the current frame with the foreground mask
    cvAnd(copyFrame, fgImageCopy, maskedCopyFrame);

    // overlay masked image on original image
    cvAddWeighted(copyFrame, 0.3, maskedCopyFrame, 0.7, 0.0, maskedCopyFrame);

    int nBlobs = pTracker->GetBlobNum();
    if (nBlobs>0) { // some blobs were detected in the current frame
        for(int i=0; i<nBlobs; i++) {
            CvSize  TextSize;
            CvBlob* pB = pTracker->GetBlob(i-1);
            CvPoint center = cvPoint(cvRound(pB->x*256),cvRound(pB->y*256));
            CvSize  size = cvSize(MAX(1,cvRound(CV_BLOB_RX(pB)*256)), MAX(1,cvRound(CV_BLOB_RY(pB)*256)));

            cvEllipse(maskedCopyFrame, center, size, 0, 0, 360, CV_RGB(0,255,0), 1, CV_AA, 8);

        }
    }
    IplToBitmap(maskedCopyFrame, bmpGesture);
    cvReleaseImage(&maskedCopyFrame);
    cvReleaseImage(&fgImageCopy);
}

DWORD WINAPI CVideoRunner::ThreadCallback(CVideoRunner* instance) {
    while (1) {
        if (instance->processingVideo && (instance->currentFrame != NULL)) {
	        instance->ProcessFrame();
        } else {
	        return 1L;
        }
    }
    return 1L;
}

void CVideoRunner::StartProcessing(bool isLive) {
	CvCapture *vc = NULL;
	runningLive = isLive;
	if (isLive) {    // Attempt to access the camera and get dimensions
	    vc = cvCreateCameraCapture(0);
		if (vc == NULL) {
			MessageBox(GetActiveWindow(),
				L"Sorry, I'm unable to connect to a camera.  Please make sure that your camera is plugged in and its drivers are installed.", 
				L"Error Accessing Camera", MB_OK | MB_ICONERROR);
			return;
		}
	} else {	// Attempt to load a recorded video
		if (!LoadRecordedVideo(parent->m_hWnd, &vc)) {
			// user didn't select a file
			return;
		}
		if (vc == NULL) { // user selected a file but we couldn't load it
			MessageBox(GetActiveWindow(), 
				L"Sorry, I'm unable to load this video file.\nIt may be in a format I can't recognize.", 
				L"Error Loading Video", MB_OK | MB_ICONERROR);
			return;
		}
	}

    // get video capture properties
	videoCapture = vc;
    currentFrame = cvQueryFrame(videoCapture);
    videoX = cvGetCaptureProperty(videoCapture, CV_CAP_PROP_FRAME_WIDTH);
    videoY = cvGetCaptureProperty(videoCapture, CV_CAP_PROP_FRAME_HEIGHT);
    fps = cvGetCaptureProperty(videoCapture, CV_CAP_PROP_FPS);
	nFrames = 1;
	if (!runningLive) {
		framesAvailable = cvGetCaptureProperty(videoCapture, CV_CAP_PROP_FRAME_COUNT);
	}

	// create images to store a copy of the current frame input and output, and an accumulator for filter data
    copyFrame = cvCreateImage(cvSize(videoX,videoY),IPL_DEPTH_8U,3);
    outputFrame = cvCreateImage(cvSize(videoX,videoY),IPL_DEPTH_8U,3);
    outputAccImage =  cvCreateImage(cvSize(videoX,videoY),IPL_DEPTH_8U,3);

    // create image to store motion history
    motionHistory = cvCreateImage(cvSize(videoX,videoY), IPL_DEPTH_32F, 1);
    cvZero(motionHistory);

    // allocate contour storage
    contourStorage = cvCreateMemStorage(0);

    // Allocate an image history ring buffer
    memset(motionBuf, 0, MOTION_NUM_IMAGES*sizeof(IplImage*));
    for(int i = 0; i < MOTION_NUM_IMAGES; i++) {
        motionBuf[i] = cvCreateImage(cvSize(copyFrame->width,copyFrame->height), IPL_DEPTH_8U, 1);
        cvZero(motionBuf[i]);
    }
    last = 0;

    // create a mask to store the results of the processing
    guessMask = cvCreateImage(cvSize(videoX,videoY),IPL_DEPTH_8U,1);

    // Create bitmaps to display video input and output
    bmpInput = new Bitmap(videoX, videoY, PixelFormat24bppRGB);
    bmpOutput = new Bitmap(videoX, videoY, PixelFormat24bppRGB);

	// Create smaller bitmaps for motion and gesture status images
    bmpMotion = new Bitmap(videoX, videoY, PixelFormat24bppRGB);
    bmpGesture = new Bitmap(videoX, videoY, PixelFormat24bppRGB);

    processingVideo = true;

    // Start processing thread
	m_hMutex = CreateMutex(NULL,FALSE,NULL);
	m_hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadCallback, (LPVOID)this, 0, &threadID);
}

void CVideoRunner::StopProcessing() {
    if (!processingVideo) return;

    WaitForSingleObject(m_hMutex,INFINITE);

	// End processing thread
	TerminateThread(m_hThread, 0);
    processingVideo = false;
	currentFrame = NULL;

    cvReleaseCapture(&videoCapture);
    cvReleaseImage(&copyFrame);
    cvReleaseImage(&outputFrame);
    cvReleaseImage(&outputAccImage);
    cvReleaseImage(&motionHistory);
    for(int i = 0; i < MOTION_NUM_IMAGES; i++) {
        cvReleaseImage(&motionBuf[i]);
    }
    cvReleaseImage(&guessMask);
    cvReleaseMemStorage(&contourStorage);

    delete bmpInput;
    delete bmpOutput;
	delete bmpMotion;
	delete bmpGesture;
    ReleaseMutex(m_hMutex);
}

void CVideoRunner::AddActiveFilter(Classifier *c) {
    WaitForSingleObject(m_hMutex,INFINITE);
    if (c->classifierType == MOTION_FILTER) {
        trackingMotion++;
    } else if (c->classifierType == GESTURE_FILTER) {
        trackingBlobs++;
    }
    activeClassifiers.push_back(c);
    ReleaseMutex(m_hMutex);
}

void CVideoRunner::ClearActiveFilters() {
    WaitForSingleObject(m_hMutex,INFINITE);
    trackingMotion = 0;
    trackingBlobs = 0;
    activeClassifiers.clear();
    ReleaseMutex(m_hMutex);
}

bool CVideoRunner::AddActiveOutput(OutputSink *o) {	// returns true if the output was added; false if it was already active
	bool alreadyAdded = false;
    WaitForSingleObject(m_hMutex,INFINITE);
    for (list<OutputSink*>::iterator j=activeOutputs.begin(); j!=activeOutputs.end(); j++) {
		if ((*j) == o) {
			alreadyAdded = true;
		}
    }
	if (!alreadyAdded) {
		activeOutputs.push_back(o);
		o->StartRunning();
	}
    ReleaseMutex(m_hMutex);
	return !alreadyAdded;
}

void CVideoRunner::ClearActiveOutputs() {
    WaitForSingleObject(m_hMutex,INFINITE);
    for (list<OutputSink*>::iterator j=activeOutputs.begin(); j!=activeOutputs.end(); j++) {
        (*j)->StopRunning();
    }
    activeOutputs.clear();
    ReleaseMutex(m_hMutex);
}


BOOL CVideoRunner::LoadRecordedVideo(HWND hwndOwner, CvCapture** capture) {
    WCHAR szFileName[MAX_PATH] = L"";
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn); // SEE NOTE BELOW
    ofn.hwndOwner = hwndOwner;
    ofn.lpstrFilter = L"Video Files\0*.avi;*.mpg;*.mp4;*.wmv;*.flv;*.mpeg;*.m2v;*.mpv;*.mov;*.qt;*.vob;*.rm\0";
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = L"avi";

    if(!GetOpenFileName(&ofn)) {
	    return FALSE;
    }

    USES_CONVERSION;
    // Attempt to load the video file and get dimensions
    *capture = cvCreateFileCapture(W2A(szFileName));
	return TRUE;
}
