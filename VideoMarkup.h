#pragma once

typedef CWinTraits<WS_CHILD|WS_VISIBLE,0> CVideoMarkupTraits;

class CVideoMarkup: public CWindowImpl<CVideoMarkup, CWindow, CVideoMarkupTraits>
{
public:
    CVideoMarkup();
    ~CVideoMarkup();
    LRESULT OnPaint( UINT, WPARAM, LPARAM, BOOL& );
    LRESULT OnButtonDown( UINT, WPARAM, LPARAM, BOOL& );
    LRESULT OnTrack( UINT, WPARAM, LPARAM, BOOL& );
    LRESULT OnMouseMove( UINT, WPARAM, LPARAM, BOOL& );
    LRESULT OnButtonUp( UINT, WPARAM, LPARAM, BOOL& );
    LRESULT OnDestroy( UINT, WPARAM, LPARAM, BOOL& );
    LRESULT OnCreate(UINT, WPARAM, LPARAM, BOOL& );
    LRESULT OnCommand(UINT, WPARAM, LPARAM, BOOL& );
    LRESULT OnBeginDrag(int, LPNMHDR, BOOL&);
    LRESULT OnCustomDraw(int, LPNMHDR, BOOL&);

    void EnableControls(BOOL);
	void OpenVideoFile();
	void RecordVideoFile();
    void OpenSampleFile(char *filename);
    void ReplaceClassifier(Classifier *newClassifier);
    void EmptyTrash();

    static CWndClassInfo& GetWndClassInfo()
    {
        static CWndClassInfo wc =
        {
            { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, 
            StartWindowProc,
            0, 0, NULL, NULL, NULL, (HBRUSH)(WHITE_BRUSH), NULL, 
            FILTER_CREATE_CLASS, LoadIcon(_AtlBaseModule.GetResourceInstance(), MAKEINTRESOURCE(IDI_EYEPATCH)) },
            NULL, NULL, IDC_CROSS, TRUE, 0, FILTER_CREATE_CLASS
        };
        return wc;
    }

    BEGIN_MSG_MAP(CVideoMarkup)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
        MESSAGE_HANDLER(WM_LBUTTONDOWN, OnButtonDown)
        MESSAGE_HANDLER(WM_RBUTTONDOWN, OnButtonDown)
        MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
        MESSAGE_HANDLER(WM_HSCROLL, OnTrack)
        MESSAGE_HANDLER(WM_LBUTTONUP, OnButtonUp)
        MESSAGE_HANDLER(WM_RBUTTONUP, OnButtonUp)
        MESSAGE_HANDLER(WM_PAINT, OnPaint)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        MESSAGE_HANDLER(WM_COMMAND, OnCommand)
        NOTIFY_CODE_HANDLER(LVN_BEGINDRAG, OnBeginDrag)
        NOTIFY_CODE_HANDLER(NM_CUSTOMDRAW, OnCustomDraw)
	ALT_MSG_MAP(1)  // sample listview
    END_MSG_MAP()

private:
	HDC hdcmem, hdcmemExamples;
	HBITMAP hbm, hbmExamples;
    HRGN activeRgn;
    HCURSOR hTrashCursor, hDropCursor;

	Graphics *graphics, *graphicsExamples;
    PointF selectStart, selectCurrent;
    Pen posSelectPen, negSelectPen, guessPen;
    SolidBrush posBrush, negBrush, hoverBrush, grayBrush, ltgrayBrush;
    bool selectingRegion;
	bool scrubbingVideo;
    int currentGroupId;
    CRect m_videoRect, m_filterRect;

	CVideoLoader m_videoLoader;
	CVideoRecorder m_videoRecorder;
    TrainingSet sampleSet;
    CContainedWindow m_sampleListView;
    CFilterSelect m_filterSelect;
    CVideoControl m_videoControl;
    Classifier *classifier;

    // currently selected recognizer mode
    int recognizerMode;

    // drag and drop stuff
    HIMAGELIST hGroupHeaderImages;
    HIMAGELIST hDragImageList;
    bool dragHover;
    Rect hoverRect;
    bool draggingIcon;
    HIMAGELIST m_hImageList;

    // list of detected objects
    list <Rect> objGuesses;
    bool showGuesses;
};