#include "precomp.h"

// Eyepatch includes
#include "TrainingSample.h"
#include "TrainingSet.h"
#include "Classifier.h"
#include "SiftClassifier.h"

SiftClassifier::SiftClassifier() :
	Classifier() {
    numSampleFeatures = 0;
    sampleCopy = NULL;
    sampleFeatures = NULL;
}

SiftClassifier::~SiftClassifier() {
    if (sampleCopy) cvReleaseImage(&sampleCopy);
    if (numSampleFeatures > 0) free(sampleFeatures);
}


void SiftClassifier::StartTraining(TrainingSet *sampleSet) {

    if (sampleCopy) cvReleaseImage(&sampleCopy);
    if (numSampleFeatures > 0) free(sampleFeatures);

    // TODO: call into trainingset class to do this instead of accessing samplemap
    for (map<UINT, TrainingSample*>::iterator i = sampleSet->sampleMap.begin(); i != sampleSet->sampleMap.end(); i++) {
        TrainingSample *sample = (*i).second;
        if (sample->iGroupId == 0) { // positive sample

            // store a copy of the sample image for later
            sampleCopy = cvCloneImage(sample->fullImageCopy);
            sampleWidth = sampleCopy->width;
            sampleHeight = sampleCopy->height;
            numSampleFeatures = sift_features(sample->fullImageCopy, &sampleFeatures);
            draw_features(sampleCopy, sampleFeatures, numSampleFeatures);
            cvResize(sampleCopy, filterImage);

            // for now just get features from first sample
            // TODO: collect feature sets from each sample?
            break;
		} else if (sample->iGroupId == 1) { // negative sample
        }
    }

    IplToBitmap(filterImage, filterBitmap);

	// update member variables
	nPosSamples = sampleSet->posSampleCount;
	nNegSamples = sampleSet->negSampleCount;
	isTrained = true;
}

void SiftClassifier::ClassifyFrame(IplImage *frame, list<Rect>* objList) {
    if (!isTrained) return;
    if(!frame) return;

    // clear current guesses
    objList->clear();

    // copy current frame and sample image for demo image
    IplImage *frameCopy = cvCloneImage(frame);
    IplImage *featureImage = cvCloneImage(sampleCopy);

    // get features in current frame
    struct feature *frameFeatures;
    int nFeatures = sift_features(frame, &frameFeatures);

    if (nFeatures > 0) {

        // we'll use these points to find the bounding rectangle of the matching features
        CvPoint ptMin, ptMax;
        ptMax.x = 0;            
        ptMax.y = 0;
        ptMin.x = frame->width;
        ptMin.y = frame->height;

        struct kd_root* kd_root = kdtree_build(frameFeatures, nFeatures);
        struct feature** nbrs;
        numFeatureMatches = 0;
        for(int i=0; i<numSampleFeatures; i++)
        {
            struct feature *feat = sampleFeatures + i;
            int k = kdtree_bbf_knn(kd_root, feat, 2, &nbrs, KDTREE_BBF_MAX_NN_CHKS);
            if( k == 2 ) {
                double d0 = descr_dist_sq(feat, nbrs[0]);
                double d1 = descr_dist_sq(feat, nbrs[1]);
                if(d0 < d1*NN_SQ_DIST_RATIO_THR) {
                    // the feature at ptSample in sample image corresponds to ptFrame in current frame
                    CvPoint ptSample = cvPoint(cvRound(feat->x), cvRound(feat->y) );
                    CvPoint ptFrame = cvPoint(cvRound(nbrs[0]->x), cvRound(nbrs[0]->y));
                    ptMin.x = min(ptMin.x, ptFrame.x);  ptMin.y = min(ptMin.y, ptFrame.y);
                    ptMax.x = max(ptMax.x, ptFrame.x);  ptMax.y = max(ptMax.y, ptFrame.y);

                    // draw feature in filter image
                    cvCircle(featureImage, ptSample, 2, colorSwatch[numFeatureMatches % COLOR_SWATCH_SIZE], 3, 8);

                    // draw feature in frame image
                    cvCircle(frameCopy, ptFrame, 2, colorSwatch[numFeatureMatches % COLOR_SWATCH_SIZE], 4, 8);

                    numFeatureMatches++;
                    sampleFeatures[i].fwd_match = nbrs[0];
                }
            }
            free( nbrs );
        }

        // As a starting point, compute the bounding box of matched features (in case we can't find transform)
        Rect objRect;
        objRect.X = ptMin.x;
        objRect.Y = ptMin.y;
        objRect.Width = ptMax.x - ptMin.x;
        objRect.Height = ptMax.y - ptMin.y;
        objList->push_back(objRect);

        if (numFeatureMatches >= SIFT_MIN_RANSAC_FEATURES) {
            // try to use RANSAC algorithm to find transformation
            CvMat* H = ransac_xform(sampleFeatures, numSampleFeatures, FEATURE_FWD_MATCH, lsq_homog, 4, 0.01, homog_xfer_err, 3.0, NULL, NULL );
            if (H != NULL) {
                IplImage* xformed;

                double pts[] = {0,0,sampleWidth,0,sampleWidth,sampleHeight,0,sampleHeight};
                CvMat foundRect = cvMat(1, 4, CV_64FC2, pts);
                cvPerspectiveTransform(&foundRect, &foundRect, H);

                cvLine(frameCopy, cvPoint(pts[0],pts[1]), cvPoint(pts[2],pts[3]), CV_RGB(255,255,255), 3);
                cvLine(frameCopy, cvPoint(pts[2],pts[3]), cvPoint(pts[4],pts[5]), CV_RGB(255,255,255), 3);
                cvLine(frameCopy, cvPoint(pts[4],pts[5]), cvPoint(pts[6],pts[7]), CV_RGB(255,255,255), 3);
                cvLine(frameCopy, cvPoint(pts[6],pts[7]), cvPoint(pts[0],pts[1]), CV_RGB(255,255,255), 3);
        
                objRect.X = min(min(pts[0],pts[2]),min(pts[4],pts[6]));
                objRect.Y = min(min(pts[1],pts[3]),min(pts[5],pts[7]));
                objRect.Width = max(max(pts[0],pts[2]),max(pts[4],pts[6])) - objRect.X;
                objRect.Height = max(max(pts[1],pts[3]),max(pts[5],pts[7])) - objRect.Y;

                cvReleaseMat( &H );
            }
        }

        // add object location guess to list
        objList->push_back(objRect);

        kdtree_release( &kd_root );
        free(frameFeatures);
    }

    cvResize(featureImage, filterImage);
    IplToBitmap(filterImage, filterBitmap);

    cvResize(frameCopy, applyImage);
    IplToBitmap(applyImage, applyBitmap);

    cvReleaseImage(&frameCopy);
    cvReleaseImage(&featureImage);

}
