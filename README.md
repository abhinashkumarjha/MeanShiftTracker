# MeanShiftTracker
The implementation of Mean Shift Tracker
How to Use Tracking Code?

Mean-shift based tracking code was developed by using C++. Download the C++ header and source files ObjectTracker.h and ObjectTracker.cpp and include those files in your project. The following steps should be performed in order to use the tracking code.
1- To construct tracker object:


CObjectTracker *m_pObjectTracker = new CObjectTracker(INT32 imW,INT32 imH, 
                                                      IMAGE_TYPE eImageType);

imW and imH: Image width and height respectively.
eImageType : Describes how many bits are used per pixel such as 24 or 32. 
             The definition of IMAGE_TYPE can be found in header file.
2- To initialize object parameters in the first frame:


m_pObjectTracker->ObjectTrackerInitObjectParameters(SINT16 x,SINT16 y,
                                                     SINT16 Width,SINT16 Height);

(x,y)        : The center coordinates of object box.
Width, Height: The width and height of the object box.
3- To track the object in the next frames:


m_pObjectTracker->ObjeckTrackerHandlerByUser(UBYTE8 *frame);

frame: Frame buffer to be processed.
4- To delete tracker object:


delete m_pObjectTracker, m_pObjectTracker = 0;
