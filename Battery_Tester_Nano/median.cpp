
#include "median.h"
double currentMedianSample;
double medianBuffer[MAX_MEDIAN_SAMPLES];
double medianResult = -1;


  //addToFilter(pingDistance);
void addToFilter(int pD){
  currentMedianSample++;
    for (int a = 0 ; a < MAX_MEDIAN_SAMPLES; a++){
        if (medianBuffer[a] < pD){
          for (int b = MAX_MEDIAN_SAMPLES - 1; b > a; b--){
            medianBuffer[b] = medianBuffer[b - 1];
          }
          medianBuffer[a] = pD;
          break;
        }
    }
    if (currentMedianSample == MAX_MEDIAN_SAMPLES){
      medianResult = medianBuffer[int(MAX_MEDIAN_SAMPLES / 2.0)];
//      for (int a = 0; a < MAX_MEDIAN_SAMPLES; a++){
//      Serial.print(" "+String(medianBuffer[a]));
//      }
//      Serial.println();
//      Serial.println("D"+String(medianResult));
      resetFilter();
    }
}
void resetFilter(){
  for (int a =0 ; a < MAX_MEDIAN_SAMPLES; a++){
    medianBuffer[a]=-1;
  }
  currentMedianSample = 0;
}
double getMedian(){
  return medianResult;
}

