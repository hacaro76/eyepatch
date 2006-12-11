#include "../precomp.h"
#include "TrajectoryList.h"

typedef struct _DefBlobTrack {
    CvBlob      blob;
    CvBlobSeq*  pSeq;
    int         FrameBegin;
    int         FrameLast;
    int         Saved; /* flag */
} DefBlobTrack;

void AddTrackToList(DefBlobTrack* pTrack, vector<MotionTrack> *trackList) {
    if(pTrack == NULL) return;
    if (pTrack->pSeq == NULL) return;

    CvBlobSeq*  pS = pTrack->pSeq;
    CvBlob*     pB0 = pS?pS->GetBlob(0):NULL;
    double lastx = 0.0;
    double lasty = 0.0;

    MotionTrack mt;
    mt.clear();
    int nTrackPoints = pS->GetBlobNum();
    for(int i=0; i<nTrackPoints; i++) {
        CvBlob* pB = pS->GetBlob(i);
        MotionSample ms;
        ms.vx = CV_BLOB_X(pB)-lastx;
        lastx = CV_BLOB_X(pB);
        ms.vy = CV_BLOB_Y(pB)-lasty;
        lasty = CV_BLOB_Y(pB);
        ms.sizex = CV_BLOB_WX(pB);
        ms.sizey = CV_BLOB_WY(pB);
        mt.push_back(ms);
    }
    trackList->push_back(mt);
}

TrajectoryList::TrajectoryList(int BlobSizeNorm) :
m_TrackList(sizeof(DefBlobTrack)) {
    m_BlobSizeNorm = BlobSizeNorm;
    m_Frame = 0;
    m_pFileName = NULL;
};

TrajectoryList::~TrajectoryList() {
    int nBlobs = m_TrackList.GetBlobNum();
    for(int i=0; i<nBlobs; i++) {
        DefBlobTrack* pTrack = (DefBlobTrack*)m_TrackList.GetBlob(i);
        delete pTrack->pSeq;
        pTrack->pSeq = NULL;
    }
}

// copies the trajectories into the passed in list, and returns number of trajectories
// trajectories shorter than GESTURE_MIN_TRAJECTORY_LENGTH are discarded
int TrajectoryList::GetTracks(vector<MotionTrack> *trackList) {
    trackList->clear();
    int nTracks = m_TrackList.GetBlobNum();
    int nFullTracks = 0;
    for(int i=0; i<nTracks; i++) {
        DefBlobTrack* pTrack = (DefBlobTrack*)m_TrackList.GetBlob(i);
        if (pTrack->FrameLast - pTrack->FrameBegin > GESTURE_MIN_TRAJECTORY_LENGTH) {
            nFullTracks++;
            AddTrackToList(pTrack, trackList);
        }
    }
    return nFullTracks;
}

void TrajectoryList::SetFileName(char*) { }

void TrajectoryList::AddBlob(CvBlob* pBlob) {
    DefBlobTrack* pTrack = (DefBlobTrack*)m_TrackList.GetBlobByID(CV_BLOB_ID(pBlob));

    if(pTrack==NULL) { // create new track
        DefBlobTrack    Track;
        Track.blob = pBlob[0];
        Track.FrameBegin = m_Frame;
        Track.pSeq = new CvBlobSeq;
        m_TrackList.AddBlob((CvBlob*)&Track);
        pTrack = (DefBlobTrack*)m_TrackList.GetBlobByID(CV_BLOB_ID(pBlob));
    }
    assert(pTrack);
    pTrack->FrameLast = m_Frame;
    assert(pTrack->pSeq);
    pTrack->pSeq->AddBlob(pBlob);
}

void TrajectoryList::Process(IplImage *pImg, IplImage *pFG) {
    m_Frame++;
}

void TrajectoryList::Release() { }
