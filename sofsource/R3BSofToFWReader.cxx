#include "FairLogger.h"

#include "FairRootManager.h"
#include "R3BSofToFWMappedData.h"
#include "R3BSofToFWReader.h"

extern "C"
{
#include "ext_data_client.h"
#include "ext_h101_softofw.h"
}

#define NUM_SOFTOFW_DETECTORS 28
#define NUM_SOFTOFW_CHANNELS 2
#include <iostream>

using namespace std;

R3BSofToFWReader::R3BSofToFWReader(EXT_STR_h101_SOFTOFW* data, UInt_t offset)
    : R3BReader("R3BSofToFWReader")
    , fData(data)
    , fOffset(offset)
    , fOnline(kFALSE)
    , fArray(new TClonesArray("R3BSofToFWMappedData"))
{
}

R3BSofToFWReader::~R3BSofToFWReader()
{
    LOG(INFO) << "R3BSofToFWReader: Delete instance";
    if (fArray)
    {
        delete fArray;
    }
}

Bool_t R3BSofToFWReader::Init(ext_data_struct_info* a_struct_info)
{
    Int_t ok;
    LOG(INFO) << "R3BSofToFWReader::Init";
    EXT_STR_h101_SOFTOFW_ITEMS_INFO(ok, *a_struct_info, fOffset, EXT_STR_h101_SOFTOFW, 0);
    if (!ok)
    {
        perror("ext_data_struct_info_item");
        LOG(ERROR) << "R3BSofToFWReader::Failed to setup structure information.";
        return kFALSE;
    }

    // Register output array in tree
    if (!fOnline)
    {
        FairRootManager::Instance()->Register("SofToFWMappedData", "SofToFW", fArray, kTRUE);
    }
    else
    {
        FairRootManager::Instance()->Register("SofToFWMappedData", "SofToFW", fArray, kFALSE);
    }
    fArray->Clear();

    // clear struct_writer's output struct. Seems ucesb doesn't do that
    // for channels that are unknown to the current ucesb config.
    EXT_STR_h101_SOFTOFW_onion* data = (EXT_STR_h101_SOFTOFW_onion*)fData;
    for (int d = 0; d < NUM_SOFTOFW_DETECTORS; d++){
        data->SOFTOFW_P[d].TFM = 0;
        data->SOFTOFW_P[d].TCM = 0;
        data->SOFTOFW_P[d].E[0] = 0;
        data->SOFTOFW_P[d].E[1] = 0;
    }
    return kTRUE;
}

Bool_t R3BSofToFWReader::Read()
{
    // Convert plain raw data to multi-dimensional array
    EXT_STR_h101_SOFTOFW_onion* data = (EXT_STR_h101_SOFTOFW_onion*)fData;

    /*
      REMINDER OF THE DATA STRUCTURE

      typedef struct EXT_STR_h101_SOFTOFW_onion_t
      {
      // RAW //
      struct {
      uint32_t TFM;
      uint32_t TFMI[2 //TFM ];
      uint32_t TFME[2 // TFM ];
      uint32_t TF;
      uint32_t TFv[20 // TF ];
      uint32_t TCM;
      uint32_t TCMI[2 // TCM ];
      uint32_t TCME[2 // TCM ];
      uint32_t TC;
      uint32_t TCv[20 // TC ];
      uint32_t E[2];
      } SOFTOFW_P[28];
      
      } EXT_STR_h101_SOFTOFW_onion;
    */

    // loop over all detectors
    for (Int_t d = 0; d < NUM_SOFTOFW_DETECTORS; d++)
    {

        uint32_t NumberOfPMTsWithHits = data->SOFTOFW_P[d].TFM; // could also be data->SOFTOFW_P[d].TCM;
        uint32_t TotalNumberOfHits = data->SOFTOFW_P[d].TF;     // could also be data->SOFTOFW_P[d].TC
        Bool_t FLAG_P;

        FLAG_P = kFALSE;
        if ((NumberOfPMTsWithHits == TotalNumberOfHits) && (TotalNumberOfHits == 2))
            FLAG_P = kTRUE;

        // loop over channels with hits
        uint32_t curChannelStart = 0;
        for (Int_t pmmult = 0; pmmult < NumberOfPMTsWithHits; pmmult++)
        {
            uint32_t pmtval = data->SOFTOFW_P[d].TFMI[pmmult];
	    if (pmtval!=data->SOFTOFW_P[d].TCMI[pmmult]){
	      LOG(ERROR) << "R3BSofToFWReader::Reader() mismatch in PMt id for ToFW between TF / TC "
			 << "TF: PMT = " << data->SOFTOFW_P[d].TFMI[pmmult]
			 << "TC: PMT = " << data->SOFTOFW_P[d].TCMI[pmmult];
	    }
            uint32_t nextChannelStart = data->SOFTOFW_P[d].TFME[pmmult];
            // put the mapped items {det,pmt,finetime, coarsetime} one after the other in the fArray
            for (int hit = curChannelStart; hit < nextChannelStart; hit++)
            {
	      new ((*fArray)[fArray->GetEntriesFast()]) R3BSofToFWMappedData(d+1, // do not change into d !!!!!!! 
									     pmtval,
									     data->SOFTOFW_P[d].TCv[hit],
									     data->SOFTOFW_P[d].TFv[hit],
									     data->SOFTOFW_P[d].E[pmmult],
									     FLAG_P);
            }
            curChannelStart = nextChannelStart;
        }

    } // end of loop over the detectors
    return kTRUE;
}

void R3BSofToFWReader::Reset()
{
    // Reset the output array
    fArray->Clear();
}

ClassImp(R3BSofToFWReader)
