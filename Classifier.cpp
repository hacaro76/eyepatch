#include "precomp.h"
#include "Classifier.h"
#include "constants.h"

CClassifierDialog::CClassifierDialog(Classifier* c) { 
	parent = c;
}

CClassifierDialog::~CClassifierDialog() {
}

LRESULT CClassifierDialog::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled) {
	USES_CONVERSION;
	CenterWindow();
	WCHAR label[MAX_PATH];
	wsprintf(label, L"Select which data should be output from the \"%s\" classifier:", parent->GetName());
	GetDlgItem(IDC_TOP_LABEL).SetWindowText(label);

	int nVariables = parent->outputData.NumVariables();
	for (int i=0; i<nVariables; i++) {
		string varName = parent->outputData.GetNameOfIndex(i);
		bool varState = parent->outputData.GetStateOfIndex(i);
		CWindow checkbox;
		checkbox.Create(L"BUTTON", this->m_hWnd, CRect(10,50+i*25,300,50+i*25+25),
			A2W(varName.c_str()), WS_CHILD | BS_AUTOCHECKBOX );
		Button_SetCheck(checkbox, varState);
		checkbox.ShowWindow(true);
		checkbox.UpdateWindow();
	}
	return TRUE;    // let the system set the focus
}

LRESULT CClassifierDialog::OnButtonClicked(UINT uMsg, WPARAM wParam, HWND hwndButton, BOOL& bHandled) {
	if (LOWORD(wParam) == IDOK) {	// the clicked button was the dismiss dialog button
		EndDialog(IDOK);
		return FALSE;
	}

	WCHAR buttonName[MAX_PATH];
	::Button_GetText(hwndButton, buttonName, MAX_PATH);
	if (Button_GetCheck(hwndButton) == BST_CHECKED) {	// activate corresponding variable
		parent->ActivateVariable(buttonName, TRUE);
	} else {	// deactivate corresponding variable
		parent->ActivateVariable(buttonName, FALSE);
	}
	return TRUE;
}

LRESULT CClassifierDialog::OnClose(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled) {
	EndDialog(IDOK);
	return 0;
}

LRESULT CClassifierDialog::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled) {
	return 0;
}



Classifier::Classifier() :
	m_ClassifierDialog(this) {

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

	// Initialize contour storage
	contourStorage = cvCreateMemStorage(0);

	// Create the default variables (all classifiers have these)
	outputData.AddVariable("Mask", guessMask);
	outputData.AddVariable("Contours", (CvSeq*)NULL);
}

Classifier::Classifier(LPCWSTR pathname) :
	m_ClassifierDialog(this) {

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

	// Initialize contour storage
	contourStorage = cvCreateMemStorage(0);

	// Create the default variables (all classifiers have these)
	outputData.AddVariable("Mask", guessMask);
	outputData.AddVariable("Contours", (CvSeq*)NULL);
}

Classifier::~Classifier() {
    cvReleaseImage(&filterImage);
    cvReleaseImage(&applyImage);
	cvReleaseImage(&guessMask);
	cvReleaseMemStorage(&contourStorage);
    delete filterBitmap;
    delete applyBitmap;
}


void Classifier::Save() {
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

void Classifier::Configure() {
	m_ClassifierDialog.DoModal();
}

void Classifier::DeleteFromDisk() {
    if (!isOnDisk) return;
    DeleteDirectory(directoryName, true);
    isOnDisk = false;
}

CvSeq* Classifier::GetMaskContours() {
    // reset the contour storage
    cvClearMemStorage(contourStorage);

    CvSeq* contours = NULL;
	cvFindContours(guessMask, contourStorage, &contours, sizeof(CvContour), CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, cvPoint(0,0));
	if (contours != NULL) {
        contours = cvApproxPoly(contours, sizeof(CvContour), contourStorage, CV_POLY_APPROX_DP, 3, 1 );
	}
	return contours;
}

Bitmap* Classifier::GetFilterImage() {
    return filterBitmap;
}
Bitmap* Classifier::GetApplyImage() {
    return applyBitmap;
}
LPWSTR Classifier::GetName() {
    return friendlyName;
}
void Classifier::SetName(LPCWSTR newName) {
    wcscpy(friendlyName, newName);
}

void Classifier::ActivateVariable(LPCWSTR varName, bool state) {
	USES_CONVERSION;
	string name = W2A(varName);
	outputData.SetVariableState(name, state);
}