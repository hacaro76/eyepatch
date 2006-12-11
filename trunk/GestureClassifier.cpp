#include "precomp.h"
#include "TrainingSample.h"
#include "TrainingSet.h"
#include "Classifier.h"
#include "GestureClassifier.h"

GestureClassifier::GestureClassifier() :
	Classifier() {
}

GestureClassifier::~GestureClassifier() {
}

void GestureClassifier::StartTraining(TrainingSet *sampleSet) {

    // TODO: call into trainingset class to do this instead of accessing samplemap
    for (map<UINT, TrainingSample*>::iterator i = sampleSet->sampleMap.begin(); i != sampleSet->sampleMap.end(); i++) {
        TrainingSample *sample = (*i).second;
        if (sample->iGroupId == 0) { // positive sample
		} else if (sample->iGroupId == 1) { // negative sample
        }
    }

    // update member variables
	nPosSamples = sampleSet->posSampleCount;
	nNegSamples = sampleSet->negSampleCount;
	isTrained = true;
}

void GestureClassifier::ClassifyFrame(IplImage *frame, list<Rect>* objList) {
    if (!isTrained) return;
    if(!frame) return;

    // clear the list of guesses
//    objList->clear();

//    cvResize(dst, applyImage);
//    IplToBitmap(applyImage, applyBitmap);
}
