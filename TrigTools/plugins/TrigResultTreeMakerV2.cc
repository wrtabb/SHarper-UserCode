

#include "SHarper/TrigTools/interface/TrigToolsFuncs.h"

#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "DataFormats/HLTReco/interface/TriggerEvent.h"
#include "DataFormats/Common/interface/TriggerResults.h"


#include "HLTrigger/HLTcore/interface/HLTFilter.h"
#include "HLTrigger/HLTcore/interface/HLTConfigProvider.h"
#include "HLTrigger/HLTcore/interface/HLTPrescaleProvider.h"

#include "FWCore/Common/interface/TriggerNames.h"
#include "DataFormats/Provenance/interface/ParameterSetID.h"

#include "FWCore/Framework/interface/EDAnalyzer.h"
#include "FWCore/Utilities/interface/InputTag.h"
#include "FWCore/Utilities/interface/EDGetToken.h"

#include "SHarper/TrigTools/interface/TrigToolsStructs.h"

#include "CommonTools/UtilAlgos/interface/TFileService.h"
#include "FWCore/ServiceRegistry/interface/Service.h"

#include "TTree.h"
#include "TFile.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

namespace {

  std::pair<size_t,size_t> getWordAndBitNr(size_t index,size_t kWordSize=32){return {index/kWordSize,index%kWordSize};}
  
}

class TrigResultTreeMakerV2 : public edm::EDAnalyzer { 

private:
   
  edm::InputTag trigResultsTag_;
  edm::InputTag trigResultsP5Tag_;
  edm::EDGetTokenT<edm::TriggerResults> trigResultsToken_;
  edm::EDGetTokenT<edm::TriggerResults> trigResultsP5Token_;
  std::vector<std::string> pathNames_;
  std::vector<std::string> pathNamesP5_;
 
  std::unique_ptr<HLTPrescaleProvider> hltConfig_;
  std::unique_ptr<HLTPrescaleProvider> hltConfigP5_;
 
  trigtools::EvtInfoStruct evtInfo_;
  std::vector<unsigned int> trigBits_;
  std::vector<unsigned int> trigBitsP5_;
  int psColumn_;
  int hltPhysicsPS_;
  std::string refTrigger_;
  TTree* tree_; //not owned by us

  static constexpr unsigned int kWordSize=32;

public:
  explicit TrigResultTreeMakerV2(const edm::ParameterSet& iPara);
  ~TrigResultTreeMakerV2(){}

  TrigResultTreeMakerV2(const TrigResultTreeMakerV2& rhs)=delete;
  TrigResultTreeMakerV2& operator=(const TrigResultTreeMakerV2& rhs)=delete;

 private:
  virtual void beginJob(){}
  virtual void beginRun(const edm::Run& run,const edm::EventSetup& iSetup);
  virtual void analyze(const edm::Event& iEvent, const edm::EventSetup& iSetup);
  virtual void endJob();
  
  

};


TrigResultTreeMakerV2::TrigResultTreeMakerV2(const edm::ParameterSet& iPara):tree_(nullptr)
{
  trigResultsTag_ = iPara.getParameter<edm::InputTag>("trigResultsTag");
  trigResultsToken_ = consumes<edm::TriggerResults>(trigResultsTag_);
  trigResultsP5Tag_ = iPara.getParameter<edm::InputTag>("trigResultsP5Tag");
  trigResultsP5Token_ = consumes<edm::TriggerResults>(trigResultsP5Tag_);

  hltConfig_ = std::make_unique<HLTPrescaleProvider>(iPara,consumesCollector(),*this);
  hltConfigP5_ = std::make_unique<HLTPrescaleProvider>(iPara,consumesCollector(),*this);

  refTrigger_ = iPara.getParameter<std::string>("refTrigger");

}

void TrigResultTreeMakerV2::beginRun(const edm::Run& run,const edm::EventSetup& setup)
{
  std::cout <<"begining run "<<std::endl;
  bool changed=false;
  hltConfig_->init(run,setup,trigResultsTag_.process(),changed); //as we need the orginal HLT config...
  std::cout <<"new table name "<<hltConfig_->hltConfigProvider().tableName()<<std::endl;

  bool changedP5=false;
  hltConfigP5_->init(run,setup,trigResultsP5Tag_.process(),changed); //as we need the orginal HLT config...
  std::cout <<"new table name "<<hltConfigP5_->hltConfigProvider().tableName()<<std::endl;
  
  
  if((changed || changedP5) && !pathNames_.empty()){ 
    std::cout <<"TrigResultTreeMakerV2::beginRun warning HLT menu has changed new: "<<changed<<" p5 "<<changedP5<<std::endl;
  }
  if(pathNames_.empty()){ //initialising hlt paths names
    pathNames_ = hltConfig_->hltConfigProvider().triggerNames();
    pathNamesP5_ = hltConfigP5_->hltConfigProvider().triggerNames();
    
    edm::Service<TFileService> fs;
    TFile* file = &fs->file();
    file->cd();
    
    size_t nrInts = pathNames_.size()/kWordSize;
    trigBits_.resize(nrInts,0);

    size_t nrIntsP5 = pathNamesP5_.size()/kWordSize;
    trigBitsP5_.resize(nrIntsP5,0);
    
    tree_=new TTree("trigResultTree","tree");
    tree_->Branch("evtInfo",&evtInfo_,trigtools::EvtInfoStruct::contents().c_str());
    tree_->Branch("trigBits",&trigBits_[0],("trigBits["+std::to_string(trigBits_.size())+"]/i").c_str());
    tree_->Branch("trigBitsP5",&trigBitsP5_[0],("trigBitsP5["+std::to_string(trigBitsP5_.size())+"]/i").c_str());
    tree_->Branch("psColumn",&psColumn_,"psColumn/I");
    tree_->Branch("hltPhysicsPS",&hltPhysicsPS_,"hltPhysicsPS/I");
    
    file->WriteObject(&pathNames_,"trigPathNames");
    file->WriteObject(&pathNames_,"trigPathNamesP5");
  }
  
}
namespace {
  void fillTrigInfo(const std::vector<std::string>& pathNames,const edm::TriggerNames& trigNames,const edm::TriggerResults& trigResults,std::vector<unsigned int>& trigBits,const unsigned int wordSize)
  {
    std::fill(trigBits.begin(),trigBits.end(),0);
    for(size_t pathNr=0;pathNr<pathNames.size();pathNr++){
      size_t pathIndex = trigNames.triggerIndex(pathNames[pathNr]);
      if(pathIndex<trigResults.size() &&  trigResults.accept(pathIndex)){
	auto wordAndBitNr = getWordAndBitNr(pathNr,wordSize);
	if(wordAndBitNr.first<trigBits.size()){
	  unsigned int bit = 0x1 << wordAndBitNr.second;
	  trigBits[wordAndBitNr.first] |= bit;
	}else{
	  std::cout <<"Warning word number is "<<wordAndBitNr.first<<" while trigBits has "<<trigBits.size()<<" words "<<std::endl;
	}
      }
    }
  }
}

void TrigResultTreeMakerV2::analyze(const edm::Event& iEvent, const edm::EventSetup& iSetup)
{
  
  edm::Handle<edm::TriggerResults> trigResultsHandle;
  iEvent.getByToken(trigResultsToken_,trigResultsHandle);
  edm::Handle<edm::TriggerResults> trigResultsP5Handle;
  iEvent.getByToken(trigResultsP5Token_,trigResultsP5Handle);
  
  const edm::TriggerResults& trigResults = *trigResultsHandle;
  const edm::TriggerNames& trigNames = iEvent.triggerNames(trigResults);  
  const edm::TriggerResults& trigResultsP5 = *trigResultsP5Handle;
  const edm::TriggerNames& trigNamesP5 = iEvent.triggerNames(trigResultsP5);  
  
  evtInfo_.fill(iEvent);

  psColumn_= hltConfigP5_->prescaleSet(iEvent,iSetup);
  hltPhysicsPS_ = hltConfigP5_->prescaleValues(iEvent,iSetup,refTrigger_).second;
  fillTrigInfo(pathNames_,trigNames,trigResults,trigBits_,kWordSize);
  fillTrigInfo(pathNamesP5_,trigNamesP5,trigResultsP5,trigBitsP5_,kWordSize);

  tree_->Fill();
 
}

void TrigResultTreeMakerV2::endJob()
{

}



//define this as a plug-in
DEFINE_FWK_MODULE(TrigResultTreeMakerV2);