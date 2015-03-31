
#include "ObjectTracker.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define GetRValue(rgb)   ((UBYTE8) (rgb)) 
#define GetGValue(rgb)   ((UBYTE8) (((ULONG_32) (rgb)) >> 8)) 
#define GetBValue(rgb)   ((UBYTE8) ((rgb) >> 16)) 

#define RGB(r, g ,b)  ((ULONG_32) (((UBYTE8) (r) | ((UBYTE8) (g) << 8)) | (((ULONG_32) (UBYTE8) (b)) << 16))) 

#define min(a, b)  (((a) < (b)) ? (a) : (b))

#define max(a, b)  (((a) > (b)) ? (a) : (b)) 


#define  MEANSHIFT_ITARATION_NO 5
#define  DISTANCE_ITARATION_NO 1
#define  ALPHA 1
#define  EDGE_DETECT_TRESHOLD  32



CObjectTracker::CObjectTracker(INT32 imW,INT32 imH,IMAGE_TYPE eImageType)
{
 
  m_nImageWidth = imW;
  m_nImageHeight = imH;
  m_eIMAGE_TYPE = eImageType;
  m_cSkipValue = 0;

  for (UBYTE8 i=0;i<MAX_OBJECT_TRACK_NUMBER;i++)
  {
	  m_sTrackingObjectTable[i].Status = false;
      for(SINT16 j=0;j<HISTOGRAM_LENGTH;j++)
		  m_sTrackingObjectTable[i].initHistogram[j] = 0;
  }

  m_nFrameCtr = 0;	
  m_uTotalTime = 0;
  m_nMaxEstimationTime = 0;
  m_cActiveObject = 0;

  switch (eImageType)
  {
   case MD_RGBA:
    m_cSkipValue = 4 ;
	break ;
   case MD_RGB:
	m_cSkipValue = 3 ;
   break ;
  };
};

CObjectTracker::~CObjectTracker()
{
 

}
//returns pixel values in format |0|B|G|R| wrt to (x.y)
ULONG_32 CObjectTracker::GetPixelValues(UBYTE8 *frame,SINT16 x,SINT16 y)
{
 ULONG_32 pixelValues = 0;

 pixelValues = *(frame+(y*m_nImageWidth+x)*m_cSkipValue+2)|//0BGR
               *(frame+(y*m_nImageWidth+x)*m_cSkipValue+1) << 8|
			   *(frame+(y*m_nImageWidth+x)*m_cSkipValue) << 16;


 return(pixelValues);

}
//set RGB components wrt to (x.y)
void CObjectTracker::SetPixelValues(UBYTE8 *frame,ULONG_32 pixelValues,SINT16 x,SINT16 y)
{
  *(frame+(y*m_nImageWidth+x)*m_cSkipValue+2) = UBYTE8(pixelValues & 0xFF);
  *(frame+(y*m_nImageWidth+x)*m_cSkipValue+1) = UBYTE8((pixelValues >> 8) & 0xFF);
  *(frame+(y*m_nImageWidth+x)*m_cSkipValue) = UBYTE8((pixelValues >> 16) & 0xFF); 
}
// returns box color 
ULONG_32 CObjectTracker::GetBoxColor()
{
 ULONG_32 pixelValues = 0;

 switch(m_cActiveObject)
 {
 case 0:
	 pixelValues = RGB(255,0,0);
	 break;
 case 1:
	 pixelValues = RGB(0,255,0);
	 break;
 case 2:
	 pixelValues = RGB(0,0,255);
	 break;
 case 3:
	 pixelValues = RGB(255,255,0);
	 break;
 case 4:
	 pixelValues = RGB(255,0,255);
	 break;
 case 5:
	 pixelValues = RGB(0,255,255);
	 break;
 case 6:
	 pixelValues = RGB(255,255,255);
	 break;
 case 7:
	 pixelValues = RGB(128,0,128);
	 break;
 case 8:
	 pixelValues = RGB(128,128,0);
	 break;
 case 9:
	 pixelValues = RGB(128,128,128);
	 break;
 case 10:
	 pixelValues = RGB(255,128,0);
	 break;
 case 11:
	 pixelValues = RGB(0,128,128);
	 break;
 case 12:
	 pixelValues = RGB(123,50,10);
	 break;
 case 13:
	 pixelValues = RGB(10,240,126);
	 break;
 case 14:
	 pixelValues = RGB(0,128,255);
	 break;
 case 15:
	 pixelValues = RGB(128,200,20);
	 break;
 default:
	 break;
 }

 return(pixelValues);


}
//initializes the object parameters 
void CObjectTracker::ObjectTrackerInitObjectParameters(SINT16 x,SINT16 y,SINT16 Width,SINT16 Height)
{

   m_cActiveObject = 0;

   m_sTrackingObjectTable[m_cActiveObject].X = x;
   m_sTrackingObjectTable[m_cActiveObject].Y = y;
   m_sTrackingObjectTable[m_cActiveObject].W = Width;
   m_sTrackingObjectTable[m_cActiveObject].H = Height;

   m_sTrackingObjectTable[m_cActiveObject].vectorX = 0;
   m_sTrackingObjectTable[m_cActiveObject].vectorY = 0;

  
   m_sTrackingObjectTable[m_cActiveObject].Status = true;
   m_sTrackingObjectTable[m_cActiveObject].assignedAnObject = false;
}

void CObjectTracker::ObjeckTrackerHandlerByUser(UBYTE8 *frame)
{
   m_cActiveObject = 0;

   if (m_sTrackingObjectTable[m_cActiveObject].Status)
   {
	   if (!m_sTrackingObjectTable[m_cActiveObject].assignedAnObject)
	   {
		   FindHistogram(frame,m_sTrackingObjectTable[m_cActiveObject].initHistogram);
           m_sTrackingObjectTable[m_cActiveObject].assignedAnObject = true;
	   }
	   else
	   {
		   FindNextLocation(frame);    

		   DrawObjectBox(frame);
	   }
   }

}
//Extracts the histogram of box
void CObjectTracker::FindHistogram(UBYTE8 *frame, FLOAT32 (*histogram))
{
  SINT16  i = 0;
  SINT16  x = 0;
  SINT16  y = 0;
  UBYTE8  E = 0;
  UBYTE8  qR = 0,qG = 0,qB = 0;
  ULONG_32 pixelValues = 0;
  UINT32 numberOfPixel = 0;


  
  for (i=0;i<HISTOGRAM_LENGTH;i++) 
	  histogram[i] = 0.0;

  for (y=max(m_sTrackingObjectTable[m_cActiveObject].Y-m_sTrackingObjectTable[m_cActiveObject].H/2,0);y<=min(m_sTrackingObjectTable[m_cActiveObject].Y+m_sTrackingObjectTable[m_cActiveObject].H/2,m_nImageHeight-1);y++)
	  for (x=max(m_sTrackingObjectTable[m_cActiveObject].X-m_sTrackingObjectTable[m_cActiveObject].W/2,0);x<=min(m_sTrackingObjectTable[m_cActiveObject].X+m_sTrackingObjectTable[m_cActiveObject].W/2,m_nImageWidth-1);x++)
	  {
		  E = CheckEdgeExistance(frame,x,y);

		  pixelValues = GetPixelValues(frame,x,y);

		  qR = GetRValue(pixelValues)/16;//quantize R component
          qG = GetGValue(pixelValues)/16;//quantize G component
          qB = GetBValue(pixelValues)/16;//quantize B component

		  histogram[4096*E+256*qR+16*qG+qB] += 1;

		  numberOfPixel++;

   }

  for (i=0;i<HISTOGRAM_LENGTH;i++) 
	  histogram[i] = histogram[i]/numberOfPixel;


}
 //Draw box around object
void CObjectTracker::DrawObjectBox(UBYTE8 *frame)
{
  SINT16 diff = 0;
  SINT16 sum  = 0;
  SINT16  x = 0;
  SINT16  y = 0;
  ULONG_32 pixelValues = 0;



  pixelValues = GetBoxColor();

  sum = (SINT16) min(m_sTrackingObjectTable[m_cActiveObject].X+m_sTrackingObjectTable[m_cActiveObject].W/2+1,(m_nImageWidth-1));
  diff = max(m_sTrackingObjectTable[m_cActiveObject].X-m_sTrackingObjectTable[m_cActiveObject].W/2,0);

  
  for (y=max(m_sTrackingObjectTable[m_cActiveObject].Y-m_sTrackingObjectTable[m_cActiveObject].H/2,0);y<=min(m_sTrackingObjectTable[m_cActiveObject].Y+m_sTrackingObjectTable[m_cActiveObject].H/2,(m_nImageHeight-1));y++)
  {
	  SetPixelValues(frame,pixelValues,diff,y);
	  SetPixelValues(frame,pixelValues,diff+1,y);

      SetPixelValues(frame,pixelValues,sum-1,y);
      SetPixelValues(frame,pixelValues,sum,y);
  }

  sum = (SINT16) min(m_sTrackingObjectTable[m_cActiveObject].Y+m_sTrackingObjectTable[m_cActiveObject].H/2+1,m_nImageHeight-1);
  diff = max(m_sTrackingObjectTable[m_cActiveObject].Y-m_sTrackingObjectTable[m_cActiveObject].H/2,0);

  for (x=max(m_sTrackingObjectTable[m_cActiveObject].X-m_sTrackingObjectTable[m_cActiveObject].W/2,0);x<=min(m_sTrackingObjectTable[m_cActiveObject].X+m_sTrackingObjectTable[m_cActiveObject].W/2,(m_nImageWidth-1));x++)
  {
	  SetPixelValues(frame,pixelValues,x,diff);
      SetPixelValues(frame,pixelValues,x,diff+1);

      SetPixelValues(frame,pixelValues,x,sum-1);
      SetPixelValues(frame,pixelValues,x,sum);
  }

}
// Computes weights and drives the new location of object in the next frame
void CObjectTracker::FindWightsAndCOM(UBYTE8 *frame, FLOAT32 (*histogram))
{
  SINT16  i = 0;
  SINT16  x = 0;
  SINT16  y = 0;
  UBYTE8  E = 0;
  FLOAT32  sumOfWeights = 0;
  SINT16  ptr  = 0;
  UBYTE8  qR = 0,qG = 0,qB = 0;
  FLOAT32   newX = 0.0;
  FLOAT32   newY = 0.0;
  ULONG_32 pixelValues = 0;


  FLOAT32 *weights = new FLOAT32[HISTOGRAM_LENGTH];

  for (i=0;i<HISTOGRAM_LENGTH;i++)
  {
	  if (histogram[i] >0.0 )
		  weights[i] = m_sTrackingObjectTable[m_cActiveObject].initHistogram[i]/histogram[i];
	  else
		  weights[i] = 0.0;
  }


  for (y=max(m_sTrackingObjectTable[m_cActiveObject].Y-m_sTrackingObjectTable[m_cActiveObject].H/2,0);y<=min(m_sTrackingObjectTable[m_cActiveObject].Y+m_sTrackingObjectTable[m_cActiveObject].H/2,m_nImageHeight-1);y++)
	  for (x=max(m_sTrackingObjectTable[m_cActiveObject].X-m_sTrackingObjectTable[m_cActiveObject].W/2,0);x<=min(m_sTrackingObjectTable[m_cActiveObject].X+m_sTrackingObjectTable[m_cActiveObject].W/2,m_nImageWidth-1);x++)
	  {
		  E = CheckEdgeExistance(frame,x,y);

		  pixelValues = GetPixelValues(frame,x,y);

		  qR = GetRValue(pixelValues)/16;
		  qG = GetGValue(pixelValues)/16;
		  qB = GetBValue(pixelValues)/16;

		  ptr = 4096*E+256*qR+16*qG+qB;

		  newX += (weights[ptr]*x);
		  newY += (weights[ptr]*y);

		  sumOfWeights += weights[ptr];
		}

   if (sumOfWeights>0)
   {
	   m_sTrackingObjectTable[m_cActiveObject].X = SINT16((newX/sumOfWeights) + 0.5);
	   m_sTrackingObjectTable[m_cActiveObject].Y = SINT16((newY/sumOfWeights) + 0.5);
   }

   delete[] weights, weights = 0;
}
// Returns the distance between two histograms.
FLOAT32 CObjectTracker::FindDistance(UBYTE8 (*histogram))
{
  SINT16  i = 0;
  FLOAT32 distance = 0;

  
  for(i=0;i<HISTOGRAM_LENGTH;i++)
	  distance += FLOAT32(sqrt(DOUBLE64(m_sTrackingObjectTable[m_cActiveObject].initHistogram[i]
                  *histogram[i])));

  return(distance);
}
//An alternative distance measurement 
FLOAT32 CObjectTracker::CompareHistogram(UBYTE8 (*histogram))
{
  SINT16  i	 = 0;
  FLOAT32 distance = 0.0;
  FLOAT32 difference = 0.0;

  
  for (i=0;i<HISTOGRAM_LENGTH;i++)
  {
	  difference = FLOAT32(m_sTrackingObjectTable[m_cActiveObject].initHistogram[i]
                         -histogram[i]);

	  if (difference>0)
		  distance += difference;
	  else
  		  distance -= difference;
  }
  return(distance);
}
// Returns the edge insformation of a pixel at (x,y)
UBYTE8 CObjectTracker::CheckEdgeExistance(UBYTE8 *frame,SINT16 _x,SINT16 _y)
{
  UBYTE8  E = 0;
  SINT16  GrayCenter = 0;
  SINT16  GrayLeft = 0;
  SINT16  GrayRight = 0;
  SINT16  GrayUp = 0;
  SINT16  GrayDown = 0;
  ULONG_32 pixelValues = 0;

  pixelValues = GetPixelValues(frame,_x,_y);

  GrayCenter = SINT16(3*GetRValue(pixelValues)+6*GetGValue(pixelValues)+GetGValue(pixelValues));

  if (_x>0)
  {
	  pixelValues = GetPixelValues(frame,_x-1,_y);
  
	  GrayLeft = SINT16(3*GetRValue(pixelValues)+6*GetGValue(pixelValues)+GetGValue(pixelValues));
  }

  if (_x < (m_nImageWidth-1))
  {
	  pixelValues = GetPixelValues(frame,_x+1,_y);
  
      GrayRight = SINT16(3*GetRValue(pixelValues)+6*GetGValue(pixelValues)+GetGValue(pixelValues));
  }

  if (_y>0)
  {
	  pixelValues = GetPixelValues(frame,_x,_y-1);
  
      GrayUp = SINT16(3*GetRValue(pixelValues)+6*GetGValue(pixelValues)+GetGValue(pixelValues));
  }

  if (_y<(m_nImageHeight-1))
  {
	  pixelValues = GetPixelValues(frame,_x,_y+1);

	  GrayDown = SINT16(3*GetRValue(pixelValues)+6*GetGValue(pixelValues)+GetGValue(pixelValues));
  }

  if (abs((GrayCenter-GrayLeft)/10)>EDGE_DETECT_TRESHOLD)
	  E = 1;

  if (abs((GrayCenter-GrayRight)/10)>EDGE_DETECT_TRESHOLD)
	  E = 1;

  if (abs((GrayCenter-GrayUp)/10)>EDGE_DETECT_TRESHOLD)
      E = 1;

  if (abs((GrayCenter-GrayDown)/10)>EDGE_DETECT_TRESHOLD)
      E = 1;

  return(E);
}
// Alpha blending: used to update initial histogram by the current histogram 
void CObjectTracker::UpdateInitialHistogram(UBYTE8 (*histogram))
{
  SINT16  i = 0;
  
  for (i=0; i<HISTOGRAM_LENGTH; i++)
	  m_sTrackingObjectTable[m_cActiveObject].initHistogram[i] = ALPHA*m_sTrackingObjectTable[m_cActiveObject].initHistogram[i]
                                                            +(1-ALPHA)*histogram[i];

}
// Mean-shift iteration 
void CObjectTracker::FindNextLocation(UBYTE8 *frame)
{
  UBYTE8  iteration = 0;

  FLOAT32 *currentHistogram = new FLOAT32[HISTOGRAM_LENGTH];

  for (iteration=0; iteration<MEANSHIFT_ITARATION_NO; iteration++)
  {
	  FindHistogram(frame,currentHistogram); //current frame histogram
   
      FindWightsAndCOM(frame,currentHistogram);//derive weights and new location
   
      //FindHistogram(frame,currentHistogram);   //uptade histogram
   
      //UpdateInitialHistogram(currentHistogram);//uptade initial histogram
  }

  delete[] currentHistogram, currentHistogram = 0;
}
 
