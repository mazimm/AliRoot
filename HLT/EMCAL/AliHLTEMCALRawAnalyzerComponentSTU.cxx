/**************************************************************************
 * This file is property of and copyright by the ALICE HLT Project        *
 * All rights reserved.                                                   *
 *                                                                        *
 * Primary Authors: Markus Fasel                                          *
 *                                                                        *
 * Permission to use, copy, modify and distribute this software and its   *
 * documentation strictly for non-commercial purposes is hereby granted   *
 * without fee, provided that the above copyright notice appears in all   *
 * copies and that both the copyright notice and this permission notice   *
 * appear in the supporting documentation. The authors make no claims     *
 * about the suitability of this software for any purpose. It is          *
 * provided "as is" without express or implied warranty.                  *
 **************************************************************************/
#include "AliHLTCaloTriggerRawDigitDataStruct.h"
#include "AliCaloRawStreamV3.h"
#include "AliEMCALTriggerData.h"
#include "AliEMCALTriggerSTURawStream.h"
#include "AliHLTEMCALDefinitions.h"
#include "AliHLTEMCALGeometry.h"
#include "AliHLTEMCALMapper.h"
#include "AliHLTEMCALRawAnalyzerComponentSTU.h"
#include "AliHLTEMCALSTURawDigitMaker.h"
#include "AliHLTEMCALSTUHeaderStruct.h"
#include "AliRawReaderMemory.h"
#include "AliDAQ.h"

ClassImp(AliHLTEMCALRawAnalyzerComponentSTU)

AliHLTEMCALRawAnalyzerComponentSTU::AliHLTEMCALRawAnalyzerComponentSTU():
AliHLTCaloProcessor(),
AliHLTCaloConstantsHandler("EMCAL"),
fSTURawDigitMaker(NULL),
fGeometry(NULL)
{
}

AliHLTEMCALRawAnalyzerComponentSTU::~AliHLTEMCALRawAnalyzerComponentSTU() {
  if(fGeometry) delete fGeometry;
}

int AliHLTEMCALRawAnalyzerComponentSTU::DoInit(int argc, const char **argv){
  fGeometry = new AliHLTEMCALGeometry(GetRunNo());

  fSTURawDigitMaker = new AliHLTEMCALSTURawDigitMaker;
  fSTURawDigitMaker->SetGeometry(fGeometry);

  return 0;
}

int AliHLTEMCALRawAnalyzerComponentSTU::DoDeinit(){
  if(fGeometry) delete fGeometry;
  fGeometry = NULL;
  return 0;
}

const char* AliHLTEMCALRawAnalyzerComponentSTU::GetComponentID(){
  return "EmcalStuAnalyzer";
}

void AliHLTEMCALRawAnalyzerComponentSTU::GetInputDataTypes( std::vector <AliHLTComponentDataType>& list){
  list.clear();
  list.push_back( AliHLTEMCALDefinitions::fgkDDLRawDataType   | kAliHLTDataOriginEMCAL );
}

AliHLTComponentDataType AliHLTEMCALRawAnalyzerComponentSTU::GetOutputDataType(){
  return AliHLTEMCALDefinitions::fgkTriggerSTUDataType;
}

void AliHLTEMCALRawAnalyzerComponentSTU::GetOutputDataSize(unsigned long& constBase, double& inputMultiplier){
  //
  constBase = sizeof(AliHLTEMCALSTUHeaderStruct) + 10000 * sizeof(AliHLTCaloTriggerRawDigitDataStruct);
  //inputMultiplier = 5.;
}

AliHLTComponent* AliHLTEMCALRawAnalyzerComponentSTU::Spawn(){
  return new AliHLTEMCALRawAnalyzerComponentSTU;
}

bool AliHLTEMCALRawAnalyzerComponentSTU::CheckInputDataType(const AliHLTComponentDataType &datatype) {
  //comment
  vector <AliHLTComponentDataType> validTypes;
  GetInputDataTypes(validTypes);

  for(UInt_t i=0; i < validTypes.size(); i++) {
    if ( datatype  ==  validTypes.at(i) ) {
      return true;
    }
  }

  HLTDebug("Invalid Datatype");
  return false;
}

int AliHLTEMCALRawAnalyzerComponentSTU::DoEvent( const AliHLTComponentEventData& evtData, const AliHLTComponentBlockData* blocks,
         AliHLTComponentTriggerData& trigData, AliHLTUInt8_t* outputPtr,
         AliHLTUInt32_t& size, std::vector<AliHLTComponentBlockData>& outputBlocks ){

  if(!IsDataEvent()) {
    size = 0;
    return 0;
  }

  UInt_t totSize = 0, ndigitddl = 0;
  const AliHLTComponentBlockData* iter = NULL;
  unsigned long ndx;


  // Get pointers to output buffer
  AliHLTEMCALSTUHeaderStruct *headerPtr = reinterpret_cast<AliHLTEMCALSTUHeaderStruct *>(outputPtr);
  AliHLTCaloTriggerRawDigitDataStruct *dataIter = reinterpret_cast<AliHLTCaloTriggerRawDigitDataStruct *>(outputPtr + sizeof(AliHLTEMCALSTUHeaderStruct)),
      *nextFastor(NULL);
  totSize += sizeof(AliHLTEMCALSTUHeaderStruct);

  AliHLTUInt32_t availableSize = size;
  if(availableSize < sizeof(AliHLTEMCALSTUHeaderStruct)){
	  HLTWarning("Not enough size in writeout buffer");
	  return 1;
  }
  availableSize -= sizeof(AliHLTEMCALSTUHeaderStruct);

  bool headerInitialized = false;
  int ndigittotal = 0;
  fSTURawDigitMaker->Reset();

  for( ndx = 0; ndx < evtData.fBlockCnt; ndx++ ) {
    iter = blocks+ndx;
    if(  ! CheckInputDataType(iter->fDataType) ) {
      continue;
    }
    if(iter->fSpecification < AliDAQ::GetFirstSTUDDL() || iter->fSpecification > AliDAQ::GetLastSTUDDL()) // check for STU DDLs
      continue;

    // Initialize raw reader from input data
    AliRawReaderMemory rawReaderMemoryPtr;
    rawReaderMemoryPtr.SetMemory(reinterpret_cast<UChar_t*>( iter->fPtr ), static_cast<ULong_t>(iter->fSize));
    rawReaderMemoryPtr.SetEquipmentID(iter->fSpecification + fCaloConstants->GetDDLOFFSET());
    rawReaderMemoryPtr.Reset();
    rawReaderMemoryPtr.NextEvent();

    fSTURawDigitMaker->Reset();

    AliEMCALTriggerSTURawStream stustream(&rawReaderMemoryPtr);
    fSTURawDigitMaker->ProcessSTUStream(&stustream, iter->fSpecification == AliDAQ::GetFirstSTUDDL() ? 0 : 1);
    const AliEMCALTriggerData *triggerData = fSTURawDigitMaker->GetTriggerData();

    if(!headerInitialized){
      // Set Header
      for (int i = 0; i < 2; i++) {
        headerPtr->fL1Threshold[2*i] = triggerData->GetL1JetThreshold(i);
        headerPtr->fL1Threshold[2*i+1] = triggerData->GetL1GammaThreshold(i);
      }
      headerPtr->fL1FrameMask = triggerData->GetL1FrameMask();
      headerInitialized = true;
    }
    int digitoutputsize = fSTURawDigitMaker->WriteRawDigitsBuffer(dataIter, availableSize);
    ndigitddl = digitoutputsize/(sizeof(AliHLTCaloTriggerRawDigitDataStruct));
    dataIter += ndigitddl; // fSTURawDigitMaker->GetNumberOfRawDigits();
    ndigittotal += ndigitddl;
    totSize += digitoutputsize;
  }

  headerPtr->fNRawDigits = ndigittotal;
  HLTDebug("Successfully decoded %d digits.", headerPtr->fNRawDigits);

  if (ndigittotal == 0) return 0; //Do not push an empty block

  AliHLTComponentBlockData bdChannelData;
  FillBlockData( bdChannelData );
  bdChannelData.fOffset = 0; //FIXME
  bdChannelData.fSize = totSize;
  bdChannelData.fDataType = GetOutputDataType();
  bdChannelData.fSpecification = 0;
  outputBlocks.push_back(bdChannelData);
  outputPtr += totSize; //Updating position of the output buffer
  size = totSize;
  return 0;
}
