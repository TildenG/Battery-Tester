/*
Copyright 2017, Tilden Groves.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#include "median.h"
double currentMedianSample;
double medianBuffer[MAX_MEDIAN_SAMPLES];
double medianResult = -1;

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
