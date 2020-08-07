//------------------------------------------------------------------------------
// 
//     The Kinect for Windows APIs used here are preliminary and subject to change
// 
//------------------------------------------------------------------------------
#include "KinectSensor.h"
#include "KinectV2PluginStats.h"
#include "ImageUtils.h"
#include "KinectSensor.h"
#include "Windows/AllowWindowsPlatformTypes.h"

// face property text layout offset in X axis
static const float c_FaceTextLayoutOffsetX = -0.1f;

// face property text layout offset in Y axis
static const float c_FaceTextLayoutOffsetY = -0.125f;
#define BODY_WAIT_OBJECT WAIT_OBJECT_0
#define AUDIO_WAIT_OBJECT WAIT_OBJECT_0 + 1
#define COLOR_WAIT_OBJECT WAIT_OBJECT_0 + 2
#define INFRARED_WAIT_OBJECT WAIT_OBJECT_0 + 3
#define DEPTH_WAIT_OBJECT WAIT_OBJECT_0 + 4
#define BODYINDEX_WAIT_OBJECT WAIT_OBJECT_0 + 5
#define POINTER_ENTERED_WAIT_OBJECT WAIT_OBJECT_0 + 6
#define POINTER_EXITED_WAIT_OBJECT WAIT_OBJECT_0 + 7
#define POINTER_MOVED_WAIT_OBJECT WAIT_OBJECT_0 + 8


static const DWORD c_FaceFrameFeatures =
FaceFrameFeatures::FaceFrameFeatures_BoundingBoxInColorSpace
| FaceFrameFeatures::FaceFrameFeatures_PointsInColorSpace
| FaceFrameFeatures::FaceFrameFeatures_RotationOrientation
| FaceFrameFeatures::FaceFrameFeatures_Happy
| FaceFrameFeatures::FaceFrameFeatures_RightEyeClosed
| FaceFrameFeatures::FaceFrameFeatures_LeftEyeClosed
| FaceFrameFeatures::FaceFrameFeatures_MouthOpen
| FaceFrameFeatures::FaceFrameFeatures_MouthMoved
| FaceFrameFeatures::FaceFrameFeatures_LookingAway
| FaceFrameFeatures::FaceFrameFeatures_Glasses
| FaceFrameFeatures::FaceFrameFeatures_FaceEngagement;

static const uint32 BodyColorLUT[] = {
	0x0000FF00,
	0x00FF0000,
	0xFFFF4000,
	0x40FFFF00,
	0xFF40FF00,
	0xFF808000,
};

/*
FKinectSensor& FKinectSensor::Get(){

static FKinectSensor Kinect;

return Kinect;

}
*/
//#include "AllowWindowsPlatformTypes.h"

static uint32 ThreadNameWorkaround = 0;

uint32 FKinectSensor::Run()
{
	HRESULT hr = S_OK;
	DWORD timeout = 2000; // In

	HANDLE handles[] = { 		
 		(HANDLE)BodyEventHandle,		
  		(HANDLE)ColorEventHandle,		
		(HANDLE)InfraredEventHandle,
		(HANDLE)DepthEventHandle,		  		
		(HANDLE)BodyIndexEventHandle,
		(HANDLE)AudioBeamEventHandle,  		
		(HANDLE)PointerEnteredEventHandle,
		(HANDLE)PointerExitedEventHandle,
		(HANDLE)PointerMovedEventHandle };

	while (!bStop) 
	{
		SCOPE_CYCLE_COUNTER(STAT_KINECT_SENSOR_RunTime);

		DWORD result = WaitForMultipleObjects(!GIsEditor ? _countof(handles) : _countof(handles) - 3, handles, FALSE, timeout);

		//Body
		if (WAIT_OBJECT_0 == result)
		{
			TComPtr<IBodyFrameArrivedEventArgs> pArgs = nullptr;
			hr = m_pBodyFrameReader->GetFrameArrivedEventData(BodyEventHandle, &pArgs);
			if (SUCCEEDED(hr)) {
				ProcessBodyFrame(pArgs);
			}
			pArgs.Reset();

		}
		//Color
		else if (WAIT_OBJECT_0 + 1 == result)
		{
			TComPtr<IColorFrameArrivedEventArgs> pArgs = nullptr;
			hr = m_pColorFrameReader->GetFrameArrivedEventData(ColorEventHandle, &pArgs);
			if (SUCCEEDED(hr)) {
				ProcessColorFrame(pArgs);
			}
			pArgs.Reset();
		}
		//Infrared
		else if (WAIT_OBJECT_0 + 2 == result)
		{
			TComPtr<IInfraredFrameArrivedEventArgs> pArgs = nullptr;
			hr = m_pInfraredFrameReader->GetFrameArrivedEventData(InfraredEventHandle, &pArgs);
			if (SUCCEEDED(hr)) {
				ProcessInfraredFrame(pArgs);
			}
			pArgs.Reset();
		}
		//Depth
		else if (WAIT_OBJECT_0 + 3 == result)
		{
			TComPtr<IDepthFrameArrivedEventArgs> pArgs = nullptr;
			hr = m_pDepthFrameReader->GetFrameArrivedEventData(DepthEventHandle, &pArgs);
			if (SUCCEEDED(hr)) {
				ProcessDepthFrame(pArgs);
			}
			pArgs.Reset();
		}
		//BodyIndex
		else if (WAIT_OBJECT_0 + 4 == result)
		{
			TComPtr<IBodyIndexFrameArrivedEventArgs> pArgs = nullptr;
			hr = m_pBodyIndexFrameReader->GetFrameArrivedEventData(BodyIndexEventHandle, &pArgs);
			if (SUCCEEDED(hr)) {
				ProcessBodyIndex(pArgs);
			}
			pArgs.Reset();
		}
		//AudioBeam
		else if (WAIT_OBJECT_0 + 5 == result)
		{				
			IAudioBeamFrameArrivedEventArgs* pAudioBeamFrameArrivedEventArgs = NULL;
			IAudioBeamFrameReference* pAudioBeamFrameReference = NULL;
			IAudioBeamFrameList* pAudioBeamFrameList = NULL;
			IAudioBeamFrame* pAudioBeamFrame = NULL;
			UINT32 subFrameCount = 0;

			hr = m_pAudioBeamFrameReader->GetFrameArrivedEventData(AudioBeamEventHandle, &pAudioBeamFrameArrivedEventArgs);
			if (SUCCEEDED(hr))
			{
				hr = pAudioBeamFrameArrivedEventArgs->get_FrameReference(&pAudioBeamFrameReference);
			}
			if (SUCCEEDED(hr))
			{
				hr = pAudioBeamFrameReference->AcquireBeamFrames(&pAudioBeamFrameList);
			}
			if (SUCCEEDED(hr))
			{
				// Only one audio beam is currently supported
				hr = pAudioBeamFrameList->OpenAudioBeamFrame(0, &pAudioBeamFrame);
			}
			if (SUCCEEDED(hr))
			{
				hr = pAudioBeamFrame->get_SubFrameCount(&subFrameCount);
			}
			if (SUCCEEDED(hr) && subFrameCount > 0)
			{
				for (UINT32 i = 0; i < subFrameCount; i++)
				{
					// Process all subframes
					IAudioBeamSubFrame* pAudioBeamSubFrame = NULL;

					hr = pAudioBeamFrame->GetSubFrame(i, &pAudioBeamSubFrame);

					if (SUCCEEDED(hr))
					{
						float fBeamAngle = 0.f;
						float fBeamAngleConfidence = 0.0f;
						float* pAudioBuffer = NULL;
						UINT cbRead = 0;

						hr = pAudioBeamSubFrame->AccessUnderlyingBuffer(&cbRead, (BYTE **)&pAudioBuffer);
						if (SUCCEEDED(hr))
						{
							if (cbRead > 0)
							{
								pAudioBeamSubFrame->get_BeamAngle(&fBeamAngle);
								pAudioBeamSubFrame->get_BeamAngleConfidence(&fBeamAngleConfidence);
							}							
						}

						UINT32 count = 0;
						if (SUCCEEDED(hr))
						{
							hr = pAudioBeamSubFrame->get_AudioBodyCorrelationCount(&count);
						}

						UINT64 trackingId;
						if (count == 0)
						{
							//trackingId = (UINT64)-1;
							UE_LOG(LogTemp, Warning, TEXT("AudioBodyCorrelationCount = 0!!!"));
						}

						IAudioBodyCorrelation* pAudioBodyCorrelation = NULL;
						if (SUCCEEDED(hr))
						{
							hr = pAudioBeamSubFrame->GetAudioBodyCorrelation(0, &pAudioBodyCorrelation);
						}

						if (SUCCEEDED(hr))
						{
							pAudioBodyCorrelation->get_BodyTrackingId(&trackingId);
							FString strackingId = FString::Format(TEXT("{0}"), { trackingId });
							//UE_LOG(LogTemp, Warning, TEXT("BodyTrackingId is: %s"), strackingId);

							SetBodyTrackId(strackingId);
						}

						SAFE_RELEASE(pAudioBodyCorrelation);
					}
					SAFE_RELEASE(pAudioBeamSubFrame);
				}
			}
			SAFE_RELEASE(pAudioBeamFrame);
			SAFE_RELEASE(pAudioBeamFrameList);
			SAFE_RELEASE(pAudioBeamFrameReference);
			SAFE_RELEASE(pAudioBeamFrameArrivedEventArgs);
		}
		else if (WAIT_OBJECT_0 + 6 == result)
		{
/*
			TComPtr<IKinectPointerEventArgs> pArgs;
			m_pCoreWindow->GetPointerEnteredEventData(PointerEnteredEventHandle, &pArgs);

			pArgs.Reset();*/
		}
		else if (WAIT_OBJECT_0 + 7 == result)
		{	
		}
		else if (WAIT_OBJECT_0 + 8 == result)
		{
/*
			TComPtr<IKinectPointerEventArgs> pArgs;
			m_pCoreWindow->GetPointerMovedEventData(PointerMovedEventHandle, &pArgs);

			pArgs.Reset();*/
		}		

		FaceFrameResultList.Empty();
		//face
		for (int32 i = 0; i < BODY_COUNT; i++)
		{
			if (m_pFaceFrameReaders[i].IsValid())
			{
				TComPtr<IFaceFrameArrivedEventArgs> pArgs = nullptr;
				hr = m_pFaceFrameReaders[i]->GetFrameArrivedEventData(FaceEventHandle[i], &pArgs);

				if (SUCCEEDED(hr)) {
					this->ProcessFaces(pArgs, i);
				}
				pArgs.Reset();
			}
		}
	}

	return 0;
}

#include "Windows/HideWindowsPlatformTypes.h"

FKinectSensor::FKinectSensor()
	: m_pKinectSensor(nullptr),
	m_pBodyFrameReader(nullptr),
	m_pColorFrameReader(nullptr),
	m_pDepthFrameReader(nullptr),
	m_pInfraredFrameReader(nullptr),
	m_pAudioBeamFrameReader(nullptr),
	bStop(true),
	m_pColorFrameRGBX(nullptr),
	m_pInfraredFrameRGBX(nullptr),
	m_pDepthFrameRGBX(nullptr),
	m_pBodyIndexFrameRGBX(nullptr),
	pKinectThread(nullptr),
	m_usRawDepthBuffer(nullptr)
{
	for (int32 index = 0; index < BODY_COUNT; index++)
	{
		m_pFaceFrameSources[index] = nullptr;
		m_pFaceFrameReaders[index] = nullptr ;
	}

	m_pColorFrameRGBX = new RGBQUAD[cColorWidth * cColorHeight];
	m_pDepthFrameRGBX = new RGBQUAD[cInfraredWidth*cInfraredHeight];
	m_pInfraredFrameRGBX = new RGBQUAD[cInfraredWidth*cInfraredHeight];
	m_pBodyIndexFrameRGBX = new RGBQUAD[cInfraredWidth*cInfraredHeight];
	m_usRawDepthBuffer = new uint16[cInfraredWidth*cInfraredHeight];

	DepthSpacePointArray.AddZeroed(cColorWidth*cColorHeight);

}


FKinectSensor:: ~FKinectSensor()
{

	//SAFE_RELEASE(m_pKinectSensor);

	DepthSpacePointArray.Empty();

	if (m_pColorFrameRGBX)
	{
		delete[] m_pColorFrameRGBX;
		m_pColorFrameRGBX = nullptr;
	}
	if (m_pInfraredFrameRGBX){
		delete[] m_pInfraredFrameRGBX;
		m_pInfraredFrameRGBX = nullptr;
	}
	if (m_pDepthFrameRGBX){
		delete[] m_pDepthFrameRGBX;
		m_pDepthFrameRGBX = nullptr;
	}

	if (m_pBodyIndexFrameRGBX)
	{
		delete[] m_pBodyIndexFrameRGBX;
		m_pBodyIndexFrameRGBX = nullptr;
	}

	if (m_usRawDepthBuffer)
	{
		delete[] m_usRawDepthBuffer;
		m_usRawDepthBuffer = nullptr;
	}

	for (int i = 0; i < BODY_COUNT; i++)
	{
		SAFE_RELEASE(m_pFaceFrameSources[i]);
		SAFE_RELEASE(m_pFaceFrameReaders[i]);
	}
}

bool FKinectSensor::Init(){

	UE_LOG(LogTemp, Warning, TEXT("------------------ Initializing kinect"));
	HRESULT hr;

	hr = GetDefaultKinectSensor(&m_pKinectSensor);
	if (FAILED(hr))
	{
		return false;
	}

	if (!m_pKinectSensor || FAILED(hr))
	{
		return false;
	}

	hr = m_pKinectSensor->get_CoordinateMapper(&m_pCoordinateMapper);

	if (FAILED(hr))
	{
		return false;
	}

	TComPtr<IBodyFrameSource> pBodyFrameSource = nullptr;

	hr = m_pKinectSensor->Open();

	TComPtr<IBodyIndexFrameSource> pBodyIndexFrameSource = nullptr;

	if (SUCCEEDED(hr))
	{
		hr = m_pKinectSensor->get_BodyIndexFrameSource(&pBodyIndexFrameSource);
	}
	if (SUCCEEDED(hr)){

		hr = pBodyIndexFrameSource->OpenReader(&m_pBodyIndexFrameReader);
	}


	pBodyIndexFrameSource.Reset();
	if (SUCCEEDED(hr))
	{
		hr = m_pKinectSensor->get_BodyFrameSource(&pBodyFrameSource);
	}

	if (SUCCEEDED(hr))
	{
		hr = pBodyFrameSource->OpenReader(&m_pBodyFrameReader);
	}


	pBodyFrameSource.Reset();

	TComPtr<IColorFrameSource> pColorFrameSource = nullptr;

	hr = m_pKinectSensor->get_ColorFrameSource(&pColorFrameSource);

	if (SUCCEEDED(hr)){
		hr = pColorFrameSource->OpenReader(&m_pColorFrameReader);
	}

	pColorFrameSource.Reset();

	TComPtr<IInfraredFrameSource> pInfraredInfraredSource = nullptr;

	hr = m_pKinectSensor->get_InfraredFrameSource(&pInfraredInfraredSource);

	if (SUCCEEDED(hr)){

		hr = pInfraredInfraredSource->OpenReader(&m_pInfraredFrameReader);

	}
	pInfraredInfraredSource.Reset();

	TComPtr<IDepthFrameSource> pDepthFrameSource = nullptr;

	hr = m_pKinectSensor->get_DepthFrameSource(&pDepthFrameSource);

	if (SUCCEEDED(hr)){
		hr = pDepthFrameSource->OpenReader(&m_pDepthFrameReader);
	}

	pDepthFrameSource.Reset();

	//audio
	IAudioSource* pAudioSource = NULL;

	if (SUCCEEDED(hr)){
		hr = m_pKinectSensor->get_AudioSource(&pAudioSource);
	}

	if (SUCCEEDED(hr)){
		hr = pAudioSource->OpenReader(&m_pAudioBeamFrameReader);
	}
	//face
	if (SUCCEEDED(hr)) 
	{
		// create a face frame source + reader to track each body in the fov
		for (int32 i = 0; i < BODY_COUNT; i++)
		{
			if (SUCCEEDED(hr))
			{
				// create the face frame source by specifying the required face frame features
				hr = CreateFaceFrameSource(m_pKinectSensor, 0, c_FaceFrameFeatures, &m_pFaceFrameSources[i]);
			}
			if (SUCCEEDED(hr))
			{
				// open the corresponding reader
				hr = m_pFaceFrameSources[i]->OpenReader(&m_pFaceFrameReaders[i]);
			}
		}
	}

	if (!GIsEditor){

		hr = GetKinectCoreWindowForCurrentThread(&m_pCoreWindow);

		if (SUCCEEDED(hr)){
			m_pCoreWindow->SubscribePointerEntered(&PointerEnteredEventHandle);
			m_pCoreWindow->SubscribePointerExited(&PointerExitedEventHandle);
			m_pCoreWindow->SubscribePointerEntered(&PointerMovedEventHandle);
		}
	}

	if (SUCCEEDED(hr))
	{
		m_pColorFrameReader->SubscribeFrameArrived(&ColorEventHandle);

		m_pBodyFrameReader->SubscribeFrameArrived(&BodyEventHandle);

		m_pInfraredFrameReader->SubscribeFrameArrived(&InfraredEventHandle);

		m_pDepthFrameReader->SubscribeFrameArrived(&DepthEventHandle);

		m_pBodyIndexFrameReader->SubscribeFrameArrived(&BodyIndexEventHandle);

		m_pAudioBeamFrameReader->SubscribeFrameArrived(&AudioBeamEventHandle);

		for (int32 i = 0; i < BODY_COUNT; i++)
		{
			m_pFaceFrameReaders[i]->SubscribeFrameArrived(&FaceEventHandle[i]);
		}
		

		bStop = false;
		UE_LOG(LogTemp, Warning, TEXT("------------------ Initialized kinect"));
		return true;
	}
	UE_LOG(LogTemp, Warning, TEXT("------------------ Failed to init kinect?"));
	return false;
}

void FKinectSensor::Stop(){

	bStop = true;

}


void FKinectSensor::StartSensor()
{
	if (!pKinectThread)
	{
		UE_LOG(LogTemp, Warning, TEXT("------------------ Starting kinect thread"));

		pKinectThread = FRunnableThread::Create(this, *(FString::Printf(TEXT("FKinectThread%ul"), ThreadNameWorkaround++)), 0, EThreadPriority::TPri_AboveNormal);

	}
}


void FKinectSensor::ShutDownSensor()
{
	UE_LOG(LogTemp, Warning, TEXT("------------------ Shutting down kinect"));
	if (pKinectThread) 
	{
		pKinectThread->Kill(true);
		
		delete pKinectThread;

		pKinectThread = nullptr;
	}
}

void FKinectSensor::Exit()
{
	UE_LOG(LogTemp, Warning, TEXT("------------------ Exiting kinect"));
	if (m_pBodyFrameReader)
		m_pBodyFrameReader->UnsubscribeFrameArrived(BodyEventHandle);

	m_pBodyFrameReader.Reset();
	//SAFE_RELEASE(m_pBodyFrameReader);

	if (m_pColorFrameReader)
		m_pColorFrameReader->UnsubscribeFrameArrived(ColorEventHandle);

	m_pColorFrameReader.Reset();
	//SAFE_RELEASE(m_pColorFrameReader);

	if (m_pInfraredFrameReader)
		m_pInfraredFrameReader->UnsubscribeFrameArrived(InfraredEventHandle);

	m_pInfraredFrameReader.Reset();
	//SAFE_RELEASE(m_pInfraredFrameReader);

	if (m_pDepthFrameReader)
		m_pDepthFrameReader->UnsubscribeFrameArrived(DepthEventHandle);

	m_pDepthFrameReader.Reset();
	//SAFE_RELEASE(m_pDepthFrameReader);

	if (m_pBodyIndexFrameReader)
		m_pBodyIndexFrameReader->UnsubscribeFrameArrived(BodyIndexEventHandle);

	m_pBodyIndexFrameReader.Reset();
	//SAFE_RELEASE(m_pBodyIndexFrameReader);

	if (m_pAudioBeamFrameReader)
		m_pAudioBeamFrameReader->UnsubscribeFrameArrived(AudioBeamEventHandle);

	m_pAudioBeamFrameReader.Reset();
	//SAFE_RELEASE(m_pAudioBeamFrameReader);
	

	for (int32 i = 0; i < BODY_COUNT; i++ )
	{
		if (m_pFaceFrameReaders[i])
			m_pFaceFrameReaders[i]->UnsubscribeFrameArrived(FaceEventHandle[i]);

		m_pFaceFrameReaders[i].Reset();
	}

	// close the Kinect Sensor
	if (m_pKinectSensor)
	{
		m_pKinectSensor->Close();
	}

	m_pKinectSensor.Reset();

}

bool FKinectSensor::GetBodyFrame(FBodyFrame& OutBodyFrame){

	FScopeLock lock(&mBodyCriticalSection);
	if (m_bNewBodyFrame)
	{
		OutBodyFrame = BodyFrame;
		m_bNewBodyFrame = false;
		return true;
	}

	return false;

}

bool FKinectSensor::GetFaceFrameResult(TArray<FFaceFrameResult>& _FaceFrameResultList)
{
	FScopeLock lock(&mFaceCriticalSection);
	if (m_bFaceFrame)
	{
		_FaceFrameResultList = FaceFrameResultList;
		m_bFaceFrame = false;
		return true;
	}

	return false;
}

/**********************************************************************************************//**
 * Process the body frame described by pArgs.
 *
 * @author	Leon Rosengarten
 * @param [in,out]	pArgs	If non-null, the arguments.
 **************************************************************************************************/
#ifdef DEBUG
#pragma optimize("",off)
#endif // DEBUG

void FKinectSensor::ProcessBodyFrame(IBodyFrameArrivedEventArgs*pArgs)
{
	// data fetching loop
	SCOPE_CYCLE_COUNTER(STAT_KINECT_SENSOR_BodyProcessTime);

	IBodyFrameReference* pBodyFrameReferance = nullptr;

	HRESULT hr = pArgs->get_FrameReference(&pBodyFrameReferance);

	if (SUCCEEDED(hr)){
		bool processFrame = false;
		IBodyFrame* pBodyFrame = nullptr;
		if (SUCCEEDED(pBodyFrameReferance->AcquireFrame(&pBodyFrame))){
			INT64 nTime = 0;

			hr = pBodyFrame->get_RelativeTime(&nTime);

			IBody* ppBodies[BODY_COUNT] = { 0 };

			if (SUCCEEDED(hr))
			{

				hr = pBodyFrame->GetAndRefreshBodyData(_countof(ppBodies), ppBodies);
			}

			if (SUCCEEDED(hr))
			{

				Vector4 floorPlane;
				pBodyFrame->get_FloorClipPlane(&floorPlane);
				{
					FScopeLock lock(&mBodyCriticalSection);

					BodyFrame = FBodyFrame(ppBodies, floorPlane);
					m_bNewBodyFrame = true;

				}

				for (int i = 0; i < _countof(ppBodies); ++i)
				{

					SAFE_RELEASE(ppBodies[i]);
				}
			}
			SAFE_RELEASE(pBodyFrame);

		}
		SAFE_RELEASE(pBodyFrameReferance);
	}
}
#ifdef DEBUG
#pragma optimize("",on)
#endif // DEBUG

/**********************************************************************************************//**
 * Process the color frame described by pArgs.
 *
 * @author	Leon Rosengarten
 * @param [in,out]	pArgs	If non-null, the arguments.
 **************************************************************************************************/

void FKinectSensor::ProcessColorFrame(IColorFrameArrivedEventArgs*pArgs)
{
	SCOPE_CYCLE_COUNTER(STAT_KINECT_SENSOR_ColorProcessTime);

	TComPtr<IColorFrameReference> pColorFrameReferance = nullptr;

	HRESULT hr = pArgs->get_FrameReference(&pColorFrameReferance);

	if (SUCCEEDED(hr)){
		TComPtr<IColorFrame> pColorFrame = nullptr;
		if (SUCCEEDED(pColorFrameReferance->AcquireFrame(&pColorFrame))){
			RGBQUAD *pColorBuffer = NULL;
			pColorBuffer = m_pColorFrameRGBX;
			uint32 nColorBufferSize = cColorWidth * cColorHeight * sizeof(RGBQUAD);
			{
				FScopeLock lock(&mColorCriticalSection);
				hr = pColorFrame->CopyConvertedFrameDataToArray(nColorBufferSize, reinterpret_cast<BYTE*>(pColorBuffer), ColorImageFormat_Bgra);
				m_bNewColorFrame = true;
			}
		}
		pColorFrame.Reset();

	}
	pColorFrameReferance.Reset();

}

/**********************************************************************************************//**
 * Process the infrared frame described by pArgs.
 *
 * @author	Leon Rosengarten
 * @param [in,out]	pArgs	If non-null, the arguments.
 **************************************************************************************************/

void FKinectSensor::ProcessInfraredFrame(IInfraredFrameArrivedEventArgs*pArgs)
{

	SCOPE_CYCLE_COUNTER(STAT_KINECT_SENSOR_InfraredProcessTime);

	TComPtr<IInfraredFrameReference> pInfraredFrameReferance = nullptr;

	HRESULT hr = pArgs->get_FrameReference(&pInfraredFrameReferance);

	if (SUCCEEDED(hr)){
		
		TComPtr<IInfraredFrame> pInfraredFrame = nullptr;

		if (SUCCEEDED(pInfraredFrameReferance->AcquireFrame(&pInfraredFrame))){
			const uint32 nInfraredBufferSize = cInfraredWidth* cInfraredHeight;
			uint16 pInfraredBuffer[nInfraredBufferSize];
			//pInfraredBuffer = m_pColorFrameRGBX;
			{
				if (SUCCEEDED(pInfraredFrame->CopyFrameDataToArray(nInfraredBufferSize, pInfraredBuffer))){
					FScopeLock lock(&mInfraredCriticalSection);

					ConvertInfraredData(pInfraredBuffer, m_pInfraredFrameRGBX);

					m_bNewInfraredFrame = true;
				}

				//hr = pColorFrame->CopyConvertedFrameDataToArray(nColorBufferSize, reinterpret_cast<BYTE*>(pColorBuffer), ColorImageFormat_Bgra);
			}
		}
		pInfraredFrame.Reset();

	}
	pInfraredFrameReferance.Reset();
}

/**********************************************************************************************//**
 * Process the depth frame described by pArgs.
 *
 * @author	Leon Rosengarten
 * @param [in,out]	pArgs	If non-null, the arguments.
 **************************************************************************************************/

void FKinectSensor::ProcessDepthFrame(IDepthFrameArrivedEventArgs*pArgs)
{

	SCOPE_CYCLE_COUNTER(STAT_KINECT_SENSOR_DepthProcessTime);

	TComPtr<IDepthFrameReference> pDepthFrameReferance = nullptr;

	HRESULT hr = pArgs->get_FrameReference(&pDepthFrameReferance);

	if (SUCCEEDED(hr)){

		TComPtr<IDepthFrame> pDepthFrame = nullptr;

		if (SUCCEEDED(pDepthFrameReferance->AcquireFrame(&pDepthFrame))){

			USHORT nDepthMinReliableDistance = 0;
			USHORT nDepthMaxReliableDistance = 0;
			const uint32 nBufferSize = cInfraredWidth*cInfraredHeight;

			if (SUCCEEDED(pDepthFrame->get_DepthMaxReliableDistance(&nDepthMaxReliableDistance)) &&
				SUCCEEDED(pDepthFrame->get_DepthMinReliableDistance(&nDepthMinReliableDistance))
				){

				FScopeLock lock(&mDepthCriticalSection);
				if (SUCCEEDED(pDepthFrame->CopyFrameDataToArray(nBufferSize, m_usRawDepthBuffer))){
					ConvertDepthData(m_usRawDepthBuffer, m_pDepthFrameRGBX, nDepthMinReliableDistance, nDepthMaxReliableDistance);

					m_bNewDepthFrame = true;
				}
			}

		}


		pDepthFrame.Reset();
	}

	pDepthFrameReferance.Reset();
}

/**********************************************************************************************//**
* Updates the color texture described by pTexture.
*
* @author	Leon Rosengarten
* @param [in,out]	pTexture	If non-null, the texture.
**************************************************************************************************/

void FKinectSensor::UpdateColorTexture(UTexture2D*pTexture)
{
	SCOPE_CYCLE_COUNTER(STAT_KINECT_SENSOR_ColorUpdateTime);
	
	if (m_bNewColorFrame)
	{
		FScopeLock lock(&mColorCriticalSection);
		UpdateTexture(pTexture, m_pColorFrameRGBX, cColorWidth, cColorHeight);
		m_bNewColorFrame = false;
	}

	return;
}

/**********************************************************************************************//**
 * Updates the infrared texture described by pTexture.
 *
 * @author	Leon Rosengarten
 * @param [in,out]	pTexture	If non-null, the texture.
 **************************************************************************************************/

void FKinectSensor::UpdateInfraredTexture(UTexture2D*pTexture)
{
	SCOPE_CYCLE_COUNTER(STAT_KINECT_SENSOR_InfraredUpdateTime);
	
	if (m_bNewInfraredFrame)
	{
		FScopeLock lock(&mInfraredCriticalSection);
		UpdateTexture(pTexture, m_pInfraredFrameRGBX, cInfraredWidth, cInfraredHeight);
		m_bNewInfraredFrame = false;
	}

	return;
}
/**********************************************************************************************//**
 * Updates the depth frame texture described by pTexture.
 *
 * @author	Leon Rosengarten
 * @param [in,out]	pTexture If non-null, the texture.
 *
 **************************************************************************************************/

void FKinectSensor::UpdateDepthFrameTexture(UTexture2D*pTexture)
{
	SCOPE_CYCLE_COUNTER(STAT_KINECT_SENSOR_DepthUpdateTime);
	
	if (m_bNewDepthFrame)
	{
		FScopeLock lock(&mDepthCriticalSection);
		UpdateTexture(pTexture, m_pDepthFrameRGBX, cInfraredWidth, cInfraredHeight);
		m_bNewDepthFrame = false;
	}
}




/**********************************************************************************************//**
 * Convert infrared data.
 *
 * @author	Leon Rosengarten
 * @param [in,out]	pInfraredBuffer	If non-null, buffer for infrared data.
 * @param [in,out]	pInfraredRGBX  	If non-null, the infrared rgbx.
 **************************************************************************************************/

void FKinectSensor::ConvertInfraredData(uint16*pInfraredBuffer, RGBQUAD*pInfraredRGBX)
{
	if (pInfraredRGBX && pInfraredBuffer)
	{
		RGBQUAD* pRGBX = pInfraredRGBX;

		// end pixel is start + width*height - 1
		const uint16* pBufferEnd = pInfraredBuffer + (cInfraredWidth * cInfraredHeight);

		while (pInfraredBuffer < pBufferEnd)
		{
			USHORT ir = *pInfraredBuffer;

			// To convert to a byte, we're discarding the least-significant bits.
			BYTE intensity = static_cast<BYTE>(ir >> 8);

			pRGBX->rgbRed = intensity;
			pRGBX->rgbGreen = intensity;
			pRGBX->rgbBlue = intensity;

			++pRGBX;
			++pInfraredBuffer;
		}
	}
}


/**********************************************************************************************//**
 * Updates the texture.
 *
 * @author	Leon Rosengarten
 * @param [in,out]	pTexture	If non-null, the texture.
 * @param	pData				The data.
 * @param	SizeX				The size x coordinate.
 * @param	SizeY				The size y coordinate.
 **************************************************************************************************/

void FKinectSensor::UpdateTexture(UTexture2D*pTexture, const RGBQUAD*pData, uint32 SizeX, uint32 SizeY)
{

	if (pTexture && pData){

		UTexture2D* Texture = pTexture;

		const size_t Size = SizeX * SizeY* sizeof(RGBQUAD);

		uint8* Src = (uint8*)pData;

		uint8* Dest = (uint8*)Texture->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE);

		FMemory::Memcpy(Dest, Src, Size);

		Texture->PlatformData->Mips[0].BulkData.Unlock();

		Texture->UpdateResource();

	}

}


void FKinectSensor::MapColorFrameToDepthSpace(TArray<FVector2D> &DepthSpacePoints)
{
	if (DepthSpacePoints.Num() == cColorWidth*cColorHeight)
	{
		FScopeLock Lock(&mDepthCriticalSection);

		DepthSpacePoints = DepthSpacePointArray;

	}
}

/**********************************************************************************************//**
 * Convert depth data.
 *
 * @author	Leon Rosengarten
 * @param [in,out]	pDepthBuffer	If non-null, buffer for depth data.
 * @param [in,out]	pDepthRGBX  	If non-null, the depth rgbx.
 * @param	minDepth				The minimum depth.
 * @param	maxDepth				The maximum depth.
 **************************************************************************************************/

void FKinectSensor::ConvertDepthData(uint16*pDepthBuffer, RGBQUAD*pDepthRGBX, USHORT minDepth, USHORT maxDepth)
{
	if (pDepthRGBX && pDepthBuffer)
	{


		m_pCoordinateMapper->MapColorFrameToDepthSpace(cInfraredWidth*cInfraredHeight, m_usRawDepthBuffer,
													   cColorWidth*cColorHeight,
													   (DepthSpacePoint*)&DepthSpacePointArray[0]);

		RGBQUAD* pRGBX = pDepthRGBX;

		// end pixel is start + width*height - 1
		const UINT16* pBufferEnd = pDepthBuffer + (cInfraredWidth * cInfraredHeight);

		while (pDepthBuffer < pBufferEnd)
		{
			USHORT depth = *pDepthBuffer;

			// To convert to a byte, we're discarding the most-significant
			// rather than least-significant bits.
			// We're preserving detail, although the intensity will "wrap."
			// Values outside the reliable depth range are mapped to 0 (black).

			// Note: Using conditionals in this loop could degrade performance.
			// Consider using a lookup table instead when writing production code.
			BYTE intensity = static_cast<BYTE>((depth >= minDepth) && (depth <= maxDepth) ? (depth % 256) : 0);

			// Modified by Liam
			// R: Full depth value
			// G: 

			// Remap value
			// to_low + (value - from_low) * (to_high - to_low) / (from_high - from_low)

			BYTE mapped = 256 - static_cast<BYTE>(0 + (depth - minDepth) * (256 - 0) / (maxDepth - minDepth));

			pRGBX->rgbRed = mapped;
			pRGBX->rgbGreen = depth;
			pRGBX->rgbBlue = intensity;

			++pRGBX;
			++pDepthBuffer;
		}
	}
}

FVector2D FKinectSensor::BodyToScreen(const FVector& bodyPoint, int32 width, int32 height)
{
	// Calculate the body's position on the screen
	DepthSpacePoint depthPoint = { 0 };

	CameraSpacePoint tempBodyPoint;

	tempBodyPoint.X = bodyPoint.X;
	tempBodyPoint.Y = bodyPoint.Y;
	tempBodyPoint.Z = bodyPoint.Z;

	m_pCoordinateMapper->MapCameraPointToDepthSpace(tempBodyPoint, &depthPoint);

	float screenPointX = static_cast<float>(depthPoint.X * width) / cInfraredWidth;
	float screenPointY = static_cast<float>(depthPoint.Y * height) / cInfraredHeight;

	return FVector2D(screenPointX, screenPointY);
}


bool FKinectSensor::IsRunning()
{
	return !bStop;
}

void FKinectSensor::ProcessBodyIndex(IBodyIndexFrameArrivedEventArgs* pArgs)
{
	SCOPE_CYCLE_COUNTER(STAT_KINECT_SENSOR_BodyIndexProcessTime);
	const uint32 nBufferSize = cInfraredWidth*cInfraredHeight;
	TComPtr<IBodyIndexFrameReference> pBodyIndexFrameReference = nullptr;

	HRESULT hr = pArgs->get_FrameReference(&pBodyIndexFrameReference);

	if (SUCCEEDED(hr))
	{
		TComPtr<IBodyIndexFrame> pBodyIndexFrame = nullptr;
		hr = pBodyIndexFrameReference->AcquireFrame(&pBodyIndexFrame);
		if (SUCCEEDED(hr))
		{
			FScopeLock scopeLock(&mBodyIndexCriticalSection);
			TArray<uint8> BodyIndexBuffer;
			BodyIndexBuffer.AddZeroed(nBufferSize);

			pBodyIndexFrame->CopyFrameDataToArray(nBufferSize, &BodyIndexBuffer[0]);
			ConvertBodyIndexData(BodyIndexBuffer, m_pBodyIndexFrameRGBX);

			m_bNewBodyIndexFrame = true;

		}
		pBodyIndexFrame.Reset();

	}

	pBodyIndexFrameReference.Reset();

}

HRESULT FKinectSensor::UpdateBodyData(IBody** ppBodies)
{
	HRESULT hr = E_FAIL;

	if (m_pBodyFrameReader != nullptr)
	{
		IBodyFrame* pBodyFrame = nullptr;
		hr = m_pBodyFrameReader->AcquireLatestFrame(&pBodyFrame);
		if (SUCCEEDED(hr))
		{
			hr = pBodyFrame->GetAndRefreshBodyData(BODY_COUNT, ppBodies);
		}
		SAFE_RELEASE(pBodyFrame);
	}

	return hr;
}


HRESULT FKinectSensor::GetFaceTextPositionInColorSpace(IBody* pBody, D2D1_POINT_2F* pFaceTextLayout)
{
	HRESULT hr = E_FAIL;

	if (pBody != nullptr)
	{
		BOOLEAN bTracked = false;
		hr = pBody->get_IsTracked(&bTracked);

		if (SUCCEEDED(hr) && bTracked)
		{
			Joint joints[JointType_Count];
			hr = pBody->GetJoints(_countof(joints), joints);
			if (SUCCEEDED(hr))
			{
				CameraSpacePoint headJoint = joints[JointType_Head].Position;
				CameraSpacePoint textPoint =
				{
					headJoint.X + c_FaceTextLayoutOffsetX,
					headJoint.Y + c_FaceTextLayoutOffsetY,
					headJoint.Z
				};

				ColorSpacePoint colorPoint = { 0 };
				hr = m_pCoordinateMapper->MapCameraPointToColorSpace(textPoint, &colorPoint);

				if (SUCCEEDED(hr))
				{
					pFaceTextLayout->x = colorPoint.X;
					pFaceTextLayout->y = colorPoint.Y;
				}
			}
		}
	}

	return hr;
}

void FKinectSensor::ProcessFaces(IFaceFrameArrivedEventArgs* pArgs, int32 iFace)
{
	//监听Cpu使用情况
	SCOPE_CYCLE_COUNTER(STAT_KINECT_SENSOR_FaceProcessTime);

	IFaceFrameReference* pFaceFrameReferance = nullptr;	
	HRESULT hr = pArgs->get_FrameReference(&pFaceFrameReferance);

	if (SUCCEEDED(hr)){		
		IBody* ppBodies[BODY_COUNT] = { 0 };
		bool bHaveBodyData = SUCCEEDED(UpdateBodyData(ppBodies));

		// iterate through each face reader
		//for (int iFace = 0; iFace < BODY_COUNT; ++iFace)
		{
			// retrieve the latest face frame from this reader
			IFaceFrame* pFaceFrame = nullptr;
			hr = m_pFaceFrameReaders[iFace]->AcquireLatestFrame(&pFaceFrame);

			BOOLEAN bFaceTracked = false;
			if (SUCCEEDED(hr) && nullptr != pFaceFrame)
			{
				// check if a valid face is tracked in this face frame
				hr = pFaceFrame->get_IsTrackingIdValid(&bFaceTracked);
			}

			if (SUCCEEDED(hr))
			{
				if (bFaceTracked)
				{
					IFaceFrameResult* pFaceFrameResult = nullptr;
					RectI faceBox = { 0 };
					PointF facePoints[FacePointType::FacePointType_Count];
					Vector4 faceRotation;
					DetectionResult faceProperties[FaceProperty::FaceProperty_Count];
					D2D1_POINT_2F faceTextLayout;

					hr = pFaceFrame->get_FaceFrameResult(&pFaceFrameResult);

					// need to verify if pFaceFrameResult contains data before trying to access it
					if (SUCCEEDED(hr) && pFaceFrameResult != nullptr)
					{
						hr = pFaceFrameResult->get_FaceBoundingBoxInColorSpace(&faceBox);

						if (SUCCEEDED(hr))
						{
							hr = pFaceFrameResult->GetFacePointsInColorSpace(FacePointType::FacePointType_Count, facePoints);
						}

						if (SUCCEEDED(hr))
						{
							hr = pFaceFrameResult->get_FaceRotationQuaternion(&faceRotation);							
						}

						if (SUCCEEDED(hr))
						{
							hr = pFaceFrameResult->GetFaceProperties(FaceProperty::FaceProperty_Count, faceProperties);
						}

						if (SUCCEEDED(hr))
						{
							hr = GetFaceTextPositionInColorSpace(ppBodies[iFace], &faceTextLayout);
						}
					
						if(SUCCEEDED(hr))
						{
							FFacePoint myFacePoint;
							myFacePoint.X = facePoints->X;
							myFacePoint.X = facePoints->Y;

							UINT64 faceTId;
							hr = pFaceFrameResult->get_TrackingId(&faceTId);

							UE_LOG(LogTemp, Warning, TEXT("faceRotation.y is: %f"), faceRotation.y);

							{
								FScopeLock lock(&mFaceCriticalSection);
																							
								FFaceFrameResult FaceFrameResult = FFaceFrameResult();

								FaceFrameResult.faceRotation = FVector4(faceRotation.x, faceRotation.y, faceRotation.z, faceRotation.w);
								FaceFrameResult.facePoints = myFacePoint;

								FString sfaceTId = FString::Format(TEXT("{0}"), { faceTId });
								FaceFrameResult.TrackingId = sfaceTId;

								FaceFrameResultList.Add(FaceFrameResult);

								m_bFaceFrame = true;
							}
						}
					}

					SAFE_RELEASE(pFaceFrameResult);
				}
				else
				{
					// face tracking is not valid - attempt to fix the issue
					// a valid body is required to perform this step
					if (bHaveBodyData)
					{
						// check if the corresponding body is tracked 
						// if this is true then update the face frame source to track this body
						IBody* pBody = ppBodies[iFace];
						if (pBody != nullptr)
						{
							BOOLEAN bTracked = false;
							hr = pBody->get_IsTracked(&bTracked);

							UINT64 bodyTId;
							if (SUCCEEDED(hr) && bTracked)
							{
								// get the tracking ID of this body
								hr = pBody->get_TrackingId(&bodyTId);
								if (SUCCEEDED(hr))
								{
									// update the face frame source with the tracking ID
									m_pFaceFrameSources[iFace]->put_TrackingId(bodyTId);
								}
							}
						}
					}
				}
			}

			SAFE_RELEASE(pFaceFrame);
		}

		if (bHaveBodyData)
		{
			for (int i = 0; i < _countof(ppBodies); ++i)
			{
				SAFE_RELEASE(ppBodies[i]);
			}
		}
	}
}

void FKinectSensor::UpdateAudioFrame(IAudioBeamFrameArrivedEventArgs* pArgs)
{
	SCOPE_CYCLE_COUNTER(STAT_KINECT_SENSOR_AudioProcessTime);

	UINT32 subFrameCount = 0;
	IAudioBeamFrameList* pAudioBeamFrameList = NULL;
 	IAudioBeamFrame* pAudioBeamFrame = NULL;
	IAudioBeamFrameReference* pAudioBeamFrameReference = nullptr;

	HRESULT hr = pArgs->get_FrameReference(&pAudioBeamFrameReference);

	if (SUCCEEDED(hr))
	{
		hr = pAudioBeamFrameReference->AcquireBeamFrames(&pAudioBeamFrameList);
	}

	uint32 beamCount = 0;
	if (SUCCEEDED(hr))
	{
		hr = pAudioBeamFrameList->get_BeamCount(&beamCount);
	}
	
	for (uint32 index = 0; index < beamCount; index++)
	{
		if (SUCCEEDED(hr))
		{
			// Only one audio beam is currently supported
			hr = pAudioBeamFrameList->OpenAudioBeamFrame(index, &pAudioBeamFrame);
		}

		if (SUCCEEDED(hr))
		{
			hr = pAudioBeamFrame->get_SubFrameCount(&subFrameCount);
		}

		if (SUCCEEDED(hr) && subFrameCount > 0)
		{
			for (uint32 i = 0; i < subFrameCount; i++)
			{
				// Process all subframes
				IAudioBeamSubFrame* pAudioBeamSubFrame = NULL;

				hr = pAudioBeamFrame->GetSubFrame(i, &pAudioBeamSubFrame);

				float fBeamAngle = 0.f;
				if (SUCCEEDED(hr))
				{
					hr = pAudioBeamSubFrame->get_BeamAngle(&fBeamAngle);
					//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, *FString::Printf(TEXT("BeamAngle is: %.3f"), fBeamAngle), false, FVector2D{ 2, 2 });
				}
				

				UINT32 count = 0;		
				if (SUCCEEDED(hr))
				{
					hr = pAudioBeamSubFrame->get_AudioBodyCorrelationCount(&count);
				}
				
				UINT64 trackingId;
				if (count == 0)
				{
					trackingId = (UINT64) - 1;
					return;
				}

				IAudioBodyCorrelation* pAudioBodyCorrelation = NULL;
				if (SUCCEEDED(hr))
				{
					hr = pAudioBeamSubFrame->GetAudioBodyCorrelation(0, &pAudioBodyCorrelation);
				}

				if (SUCCEEDED(hr))
				{
					pAudioBodyCorrelation->get_BodyTrackingId(&trackingId);
					GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, *FString::Printf(TEXT("trackingId is: %d"), trackingId), false, FVector2D{ 2, 2 });
				}

				SAFE_RELEASE(pAudioBeamSubFrame);
			}
		}
	}
	SAFE_RELEASE(pAudioBeamFrame);
	SAFE_RELEASE(pAudioBeamFrameList);
}

void FKinectSensor::ConvertBodyIndexData(const TArray<uint8> BodyIndexBufferData, RGBQUAD* pBodyIndexRGBX)
{
	if (BodyIndexBufferData.Num() > 0 && pBodyIndexRGBX)
	{

		RGBQUAD* pRGBX = pBodyIndexRGBX;

		for (size_t i = 0; i < BodyIndexBufferData.Num(); i++)
		{

			if (BodyIndexBufferData[i] < 6)
			{

				uint32 LUTColor = BodyColorLUT[BodyIndexBufferData[i]];
				pRGBX->rgbBlue = (LUTColor & 0x0000ff00) >> 8;
				pRGBX->rgbGreen = (LUTColor & 0x00ff0000) >> 16;
				pRGBX->rgbRed = (LUTColor & 0xff000000) >> 24;
			
			}
			else
			{
				pRGBX->rgbBlue = pRGBX->rgbGreen = pRGBX->rgbRed = 0x00;
			}

			++pRGBX;
		}
	}
}


void FKinectSensor::UpdateBodyIndexTexture(UTexture2D* pTexture)
{
	SCOPE_CYCLE_COUNTER(STAT_KINECT_SENSOR_BodyIndexUpdateTime);
	
	if (m_bNewBodyIndexFrame)
	{
		FScopeLock lock(&mDepthCriticalSection);
		UpdateTexture(pTexture, m_pBodyIndexFrameRGBX, 512, 424);
		m_bNewBodyIndexFrame = false; 
	}
}
