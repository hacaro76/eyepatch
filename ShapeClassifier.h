#pragma once
#include "Classifier.h"

class ShapeClassifier : public Classifier {
public:
    ShapeClassifier();
    ~ShapeClassifier();

    BOOL ContainsSufficientSamples(TrainingSet*);
	void StartTraining(TrainingSet*);
	void ClassifyFrame(IplImage*, list<Rect>*);
    void Save();

private:
    CvMemStorage *templateStorage;
    CvSeq *templateContours;
};
