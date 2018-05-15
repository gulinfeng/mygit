#ifndef _FRLIB_TYPES_H
#define _FRLIB_TYPES_H


#define IDLISTMAX 4					/*!< Maximum number of matched faces output */
#define FRINFO_STRING_SIZE 128				/*!< Length of string storing first/second name of person */

/*! Structure to store face ROI */
struct FRFace
{
	int 	x;					/*!< Face ROI top left x */
	int 	y;					/*!< Face ROI top left y */
	int 	width;					/*!< Face ROI width */
	int 	height;					/*!< Face ROI height */
};

/*! Structure to store FR output */
struct FRIDList
{
    struct FRFace	  pFace[IDLISTMAX];			 /*!< Face ROI */
    char 	  pFName[IDLISTMAX][FRINFO_STRING_SIZE]; /*!< Person's first name */
    char 	  pLName[IDLISTMAX][FRINFO_STRING_SIZE]; /*!< Person's last name */
    float         pConfidence[IDLISTMAX]; 		 /*!< confidences */
    unsigned long pRecordID  [IDLISTMAX]; 		 /*!< record ids, or ULONG_MAX in case of unknown */
    int           nResults;                     	 /*!< number of results */
};

/*! FR running mode - recognition or enrolling */
enum FRMode
{
	FRMODE_RECOGNIZE = 0,				/*!< Recognition mode */
	FRMODE_ENROLL = 1					/*!< Enrolling mode */
};

#endif //_FRLIB_TYPES_H