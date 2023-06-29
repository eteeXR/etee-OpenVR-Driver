#include "Encode/Thresholds.h"
/*
These are the thresholds used in the SeparateTriggerValue(...) function, inside of EteeEncodingManager.cpp. 
These thresholds define:
	- Split Thresholds: the percentage of activation needed to trigger the click and the 2nd analog range (i.e. squeezing)
	- Activation Thresholds for Touch: the percentage from the pull or value analog range needed to activate the touch bool
*/

// Split thresholds
float gripSplitThreshold = 0.65f;
float finger1SplitThreshold = 0.65f;
float finger2SplitThreshold = 0.65f;
float finger3SplitThreshold = 0.65f;
float finger4SplitThreshold = 0.65f;
float finger5SplitThreshold = 0.65f;
float thumbpadSplitThreshold = 0.65f;


// Activation threshold for touch
float gripTouchThreshold = 0.45f;
float finger1TouchThreshold = 0.45f;
float finger2TouchThreshold = 0.45f;
float finger3TouchThreshold = 0.45f;
float finger4TouchThreshold = 0.45f;
float finger5TouchThreshold = 0.45f;
float thumbpadTouchThreshold = 0.45f;