// -----------------------------------------
// PrtLutReco.cpp
//
// Created on: 13.07.2013
// Author: R.Dzhygadlo at gsi.de
// -----------------------------------------

#include "PrtLutReco.h"

#include "PrtManager.h"

#include "PrtLutNode.h"
#include "PrtTrackInfo.h"
#include "PrtPhotonInfo.h"
#include "PrtAmbiguityInfo.h"

#include "TROOT.h"
#include "TStyle.h"
#include "TCanvas.h"
#include "TMath.h"
#include "TRotation.h"
#include "TGraph.h"
#include <TVirtualFitter.h>
#include <TArc.h>
#include <TLegend.h>
#include "../../prttools/prttools.C"

using std::cout;
using std::endl;

TH1F*  fHist0 = new TH1F("timediff",";t_{calc}-t_{measured} [ns];entries [#]", 500,-10,10);
TH1F*  fHist1 = new TH1F("time1",";measured time [ns];entries [#]",   1000,0,100);
TH1F*  fHist2 = new TH1F("time2",";calculated time [ns];entries [#]", 1000,0,100);
TH2F*  fHist3 = new TH2F("time3",";calculated time [ns];measured time [ns]", 500,0,80, 500,0,40);
TH2F*  fHist4 = new TH2F("time4",";#theta_{c}sin(#varphi_{c});#theta_{c}cos(#varphi_{c}", 200,-1,1, 200,-1,1);
TH2F*  fHist5 = new TH2F("time5",";#theta_{c}sin(#varphi_{c});#theta_{c}cos(#varphi_{c}", 200,-1,1, 200,-1,1);
Int_t gg_i(0);
TGraph gg_gr;

// -----   Default constructor   -------------------------------------------
PrtLutReco::PrtLutReco(TString infile, TString lutfile, Int_t verbose){
  fVerbose = verbose;
  fChain = new TChain("data");
  fChain->Add(infile);
  fChain->SetBranchAddress("PrtEvent", &fEvent);

  fFile = new TFile(lutfile);
  fTree=(TTree *) fFile->Get("prtlut") ;
  fLut = new TClonesArray("PrtLutNode");
  fTree->SetBranchAddress("LUT",&fLut); 
  fTree->GetEntry(0);

  fHist = new TH1F("chrenkov_angle_hist","chrenkov angle;#theta_{C} [rad];entries [#]", 200,0.6,1); //200
  fFit = new TF1("fgaus","[0]*exp(-0.5*((x-[1])/[2])*(x-[1])/[2]) +[3]",0.35,0.9);
  fSpect = new TSpectrum(10);

  if(infile.Contains("beam_")){
    TString fileid(infile);
    fileid.Remove(0,fileid.Last('/')+1);
    fileid.Remove(fileid.Last('.')-1);
    prt_data_info = getDataInfo(fileid);
    
    TString opath(infile);
    opath.Remove(opath.Last('/'));
    if(infile.Contains("C.root")){
      fSavePath = opath+Form("/%dr/%d",prt_data_info.getStudyId(),prt_data_info.getFileId());
    }else{
      fSavePath = opath+Form("/%ds/%d",prt_data_info.getStudyId(),prt_data_info.getFileId());
    }
    
    std::cout<<"fSavePath  "<< fSavePath <<std::endl;    
  }
  cout << "-I- PrtLutReco: Intialization successfull" << endl;
}

// -----   Destructor   ----------------------------------------------------
PrtLutReco::~PrtLutReco(){

}
 
//-------------- Loop over tracks ------------------------------------------
void PrtLutReco::Run(Int_t start, Int_t end){
  TVector3 dird, dir, momInBar(0,0,1),posInBar;
  Double_t cangle,spr,tangle,boxPhi,evtime, bartime, lenz,dirz,luttheta, barHitTime, hitTime;
  Int_t pdgcode, evpointcount=0;
  Bool_t reflected = kFALSE;
  gStyle->SetOptFit(111);

  TVector3 fnX1 = TVector3 (1,0,0);   
  TVector3 fnY1 = TVector3( 0,1,0);
  bool testTrRes = false;
  Double_t angdiv,dtheta,dtphi,fAngle;;

  TString outFile = PrtManager::Instance()->GetOutName()+"_spr.root";
  Double_t theta(0),phi(0), trr(0),  nph(0),
    par1(0), par2(0), par3(0), par4(0), par5(0), par6(0), test1(0), test2(0);
  
  TFile f(outFile,"recreate");
  TTree tree("dirc","SPR");
  tree.Branch("spr", &spr,"spr/D");
  tree.Branch("trr", &trr,"trr/D");
  tree.Branch("nph",&nph,"nph/D");
  tree.Branch("cangle",&cangle,"cangle/D");
  tree.Branch("par3",&par3,"par3/D");
  tree.Branch("par4",&par4,"par4/D");
  tree.Branch("par5",&par5,"par5/D");
  tree.Branch("par6",&par6,"par6/D");
  tree.Branch("test1",&test1,"test1/D");
  tree.Branch("test2",&test2,"test2/D");
  tree.Branch("theta",&theta,"theta/D");
  tree.Branch("phi",&phi,"phi/D");

  test1 = PrtManager::Instance()->GetTest1();
  
  Int_t nEvents = fChain->GetEntries();
  if(end==0) end = nEvents;
  
  std::cout<<"Run started for ["<<start<<","<<end <<"]"<<std::endl;
  Int_t ntotal=0;
  
  for (Int_t ievent=0; ievent<nEvents && ievent<end; ievent++){
    fChain->GetEntry(ievent);
    Int_t nHits = fEvent->GetHitSize();
    ntotal+=nHits;
    std::cout<<"Event # "<< ievent << " has "<< nHits <<" hits"<<std::endl;
    tree.SetTitle(fEvent->PrintInfo());
    Double_t minChangle = -3.35;
    Double_t maxChangle = 3.9;

    PrtTrackInfo trackinfo;
    if(fVerbose>3){
      trackinfo.AddInfo(fEvent->PrintInfo()+"\n Basic reco informaion: \n");
      trackinfo.AddInfo(Form("Cerenkov angle selection: (%f,%f) \n",minChangle,maxChangle));
    }

    //    TVector3 rotatedmom = fEvent->GetMomentum().Unit();
    if(fEvent->GetParticle()!=2212) continue;
    fAngle = fEvent->GetAngle();

    Double_t rad = TMath::Pi()/180.;
    TVector3 rotatedmom = momInBar;
    rotatedmom.RotateY(fAngle*rad);
    TVector3 cz = rotatedmom.Unit();
    cz = TVector3(-cz.X(),cz.Y(),cz.Z());    
    for(Int_t h=0; h<nHits; h++) {
      fHit = fEvent->GetHit(h);
      hitTime = fHit.GetLeadTime();
      PrtPhotonInfo photoninfo;
      
      Double_t radiatorL = 1250; //bar


      Double_t z =  fEvent->GetBeamZ();
      if( fEvent->GetType()==1){
	lenz = radiatorL/2.-fHit.GetPosition().Z();
	//std::cout<<"lenz 0  "<<lenz;
      }else{
	lenz = z-1/tan(fAngle*rad)*(122+(z-96)/tan((135-0.5*fAngle)*rad));
	// Double_t b = 122*tan(0.5*((fAngle-90)*rad)); 
	// Double_t lenz = (z-96+b)/cos((fAngle-90)*rad)+b+96;     
      }
      // std::cout<<"z "<<fEvent->GetBeamZ()  << "  a "<< fAngle  <<std::endl;
      // std::cout<<" lenz 1  "<< z-1/tan(fAngle*rad)*(122+(z-96)/tan((135-0.5*fAngle)*rad)) <<std::endl;
      
      
      TVector3 vv = fHit.GetMomentum();
      vv.RotateY(fAngle*rad);
      dirz = vv.Z();

      TVector3 cd = fHit.GetMomentum();
      Double_t phi0 =  cd.Phi();        
      Double_t theta0 = cd.Theta();      
      fHist5->Fill(theta0*TMath::Sin(phi0),theta0*TMath::Cos(phi0));
      
      if(dirz<0) reflected = kTRUE;
      else reflected = kFALSE;

      if(hitTime>12) {
      	reflected = kTRUE;
	lenz = 2*radiatorL - lenz;
      }else{
	reflected = kFALSE;
      }
      
      Int_t sensorId = 100*fHit.GetMcpId()+fHit.GetPixelId();
      if(sensorId==1) continue;
   
      PrtLutNode *node = (PrtLutNode*) fLut->At(sensorId);
      Int_t size = node->Entries();
    
      for(int i=0; i<size; i++){
	dird = node->GetEntry(i);
	evtime = node->GetTime(i);

	for(int u=0; u<4; u++){
	  
	  if(u == 0) dir = dird;
	  if(u == 1) dir.SetXYZ( dird.X(),-dird.Y(),  dird.Z());
	  if(u == 2) dir.SetXYZ( -dird.X(), dird.Y(), dird.Z());
	  if(u == 3) dir.SetXYZ( -dird.X(),-dird.Y(), dird.Z());
	  if(reflected) dir.SetXYZ( dir.X(), dir.Y(), -dir.Z());
	  
	  double criticalAngle = asin(1.00028/1.47125); // n_quarzt = 1.47125; //(1.47125 <==> 390nm)
	  if(dir.Angle(fnX1) < criticalAngle || dir.Angle(fnY1) < criticalAngle) continue;
	  
	  luttheta = dir.Theta();  
	  if(luttheta > TMath::PiOver2()) luttheta = TMath::Pi()-luttheta;
	  // if(luttheta <0 ) luttheta = TMath::Pi()+luttheta;

	  bartime = fabs(lenz/cos(luttheta)/198.); 
	 
	  fHist0->Fill((bartime+evtime)-hitTime);
	  fHist1->Fill(hitTime);
	  fHist2->Fill(bartime+evtime);
	  
	  if(fabs((bartime+evtime)-hitTime)>test1) continue;
	  fHist3->Fill(fabs((bartime+evtime)),hitTime);
	  tangle = rotatedmom.Angle(dir);
	  if(tangle>TMath::PiOver2()) tangle = TMath::Pi()-tangle;
	  
	  if(fVerbose>3){
	    PrtAmbiguityInfo ambinfo;
	    ambinfo.SetBarTime(bartime);
	    ambinfo.SetEvTime(evtime);
	    ambinfo.SetCherenkov(tangle);
	    photoninfo.AddAmbiguity(ambinfo);
	  }
	  
	  if(tangle > minChangle && tangle < maxChangle){
	    fHist->Fill(tangle);
	   
	    // TVector3 rdir = TVector3(-dir.X(),dir.Y(),dir.Z());
	    // rdir.RotateUz(cz);
	 
	    // Double_t phi = rdir.Phi();
	    // Double_t tt =  rdir.Theta();
	    // fHist4->Fill(tt*TMath::Sin(phi),tt*TMath::Cos(phi));

	    // //for cherenckov circle fit
	    // gg_gr.SetPoint(gg_i,tangle*TMath::Sin(phi),tangle*TMath::Cos(phi));
	    // gg_i++;
	  }
	}
      }
      if(fVerbose>3){
	photoninfo.SetHitTime(hitTime);
	photoninfo.SetReflected(reflected);
	photoninfo.SetEvReflections(evpointcount);
	photoninfo.SetSensorId(sensorId);
	photoninfo.SetMcCherenkovInBar(fHit.GetCherenkovMC());

	trackinfo.AddPhoton(photoninfo);
      }
    }
    if(fVerbose>4){
      FindPeak(cangle,spr, fAngle); // fEvent->GetAngle()+0.01
      std::cout<<"RES   "<<spr*1000 << "   N "<<nHits << "  "<<spr/sqrt(nHits)*1000<<std::endl;
    }
    //Int_t pdgreco = FindPdg(fEvent->GetMomentum().Mag(), cherenkovreco);

    if(fVerbose>3){
      //if(testTrRes) trackinfo.SetMomentum(TVector3(dtheta,dtphi,0)); //track deviation
      trackinfo.SetMcMomentum(fEvent->GetMomentum());
      trackinfo.SetMcMomentumInBar(momInBar);
      trackinfo.SetMcPdg(0);
      trackinfo.SetPdg(0);
      trackinfo.SetAngle(fEvent->GetAngle());
      trackinfo.SetMcCherenkov(cangle);
      
      //trackinfo.SetCherenkov(cherenkovreco);
      trackinfo.SetMcTimeInBar(barHitTime);
      
      PrtManager::Instance()->AddTrackInfo(trackinfo);
      PrtManager::Instance()->Fill();
    }
  }
  
  FindPeak(cangle,spr,fAngle);
  Double_t aEvents = ntotal/(Double_t)nEvents;

  nph = ntotal/(Double_t)nEvents;
  spr = spr*1000;
  trr = spr/sqrt(aEvents);
  theta = fEvent->GetAngle();
  par3 = fEvent->GetTest1();
  
  std::cout<<"RES   "<<spr << "   N "<< nph << " trr  "<<trr<<std::endl;
    
  tree.Fill();
  tree.Write();

  PrtManager::Instance()->Save();
}

Int_t g_num =0;
Bool_t PrtLutReco::FindPeak(Double_t& cherenkovreco, Double_t& spr, Double_t a){
  cherenkovreco=0;
  spr=0;
  //  gStyle->SetCanvasPreferGL(kTRUE);
  
  if(fHist->GetEntries()>20 ){
    gROOT->SetBatch(1);
    Int_t nfound = fSpect->Search(fHist,1,"",0.9); //0.6
    Float_t *xpeaks = fSpect->GetPositionX();
    if(nfound>0) cherenkovreco = xpeaks[0];
    else cherenkovreco =  fHist->GetXaxis()->GetBinCenter(fHist->GetMaximumBin());

    fFit->SetParameters(100,cherenkovreco,0.005,10);   // peak
    // fFit->SetParameter(1,cherenkovreco);   // peak
    // fFit->SetParameter(2,0.005); // width
    fFit->SetParameter(0,1000); 
    fFit->FixParameter(2,0.014); // width
    fHist->Fit("fgaus","","",cherenkovreco-0.15,cherenkovreco+0.15);
    fFit->ReleaseParameter(2); // width
    fHist->Fit("fgaus","M","",cherenkovreco-0.07,cherenkovreco+0.07);
    cherenkovreco = fFit->GetParameter(1);
    spr = fFit->GetParameter(2); 
    if(fVerbose>1) gROOT->SetBatch(0);
    
    Bool_t storePics(true);
    if(storePics){
      canvasAdd("r_tangle",800,400);
      fHist->SetTitle(Form("theta %3.1f", a));
      fHist->SetMinimum(0);
      fHist->Draw();
      
      canvasAdd("r_time",800,400);
      fHist1->SetTitle(Form("theta %3.1f", a));
      fHist1->SetLineColor(2);
      fHist1->Draw();
      fHist2->Draw("same");

      canvasAdd("r_diff",800,400);
      fHist0->SetTitle(Form("theta %3.1f", a));
      fHist0->Draw();
    
      canvasAdd("r_cm",800,400);
      fHist3->SetTitle(Form("theta %3.1f", a));
      fHist3->Draw("colz");

      waitPrimitive("r_cm");
      canvasSave(1,0);
      
      // TCanvas* c2 = new TCanvas("c2","c2",0,0,800,400);
      // c2->Divide(2,1);
      // c2->cd(1);
     
      // fHist4->SetStats(0);
      // fHist4->SetTitle(Form("Calculated from LUT, #theta = %d#circ", a));
      // fHist4->Draw("colz");
      // Double_t x0(0), y0(0), theta(cherenkovreco);
      // FitRing(x0,y0,theta);
      // TVector3 corr(x0,y0,1-TMath::Sqrt(x0*x0+y0*y0));
      // std::cout<<"Tcorr "<< corr.Theta()*1000<< "  Pcorr "<< corr.Phi() <<std::endl;

      // TLegend *leg = new TLegend(0.5,0.7,0.85,0.87);
      // //      leg->SetFillColor(0);
      // //leg->SetFillColorAlpha(0,0.8);
      // leg->SetFillStyle(0);
      // //leg->SetFillStyle(4000); 
      // leg->SetBorderSize(0);
      // leg->AddEntry((TObject*)0,Form("Entries %0.0f",fHist4->GetEntries()),"");
      // leg->AddEntry((TObject*)0,Form("#Delta#theta_{c} %f [mrad]",corr.Theta()*1000),"");
      // leg->AddEntry((TObject*)0,Form("#Delta#varphi_{c} %f [rad]",corr.Phi()),"");
      // leg->Draw();

      // TArc *arc = new TArc(x0,y0,theta);
      // arc->SetLineColor(kRed);
      // arc->SetLineWidth(1);
      // arc->SetFillStyle(0);
      // arc->Draw();
      // gg_i=0;
      // gg_gr.Set(0);

      // c2->cd(2);
      // gStyle->SetOptStat(1110); 
      // fHist5->SetTitle(Form("True from MC, #theta = %d#circ", a));
      // fHist5->Draw("colz");

      // c2->Print(Form("spr/tcorr_%d.png", a));
      // c2->Modified();
      // c2->Update();
      // c2->WaitPrimitive("s");
    
    }
  }

  if(fVerbose<2) gROOT->SetBatch(0);
  fHist->Reset();
  fHist0->Reset();
  fHist1->Reset();
  fHist2->Reset();
  fHist3->Reset();
  fHist4->Reset();

  return (cherenkovreco>0 && cherenkovreco<1);
}

void circleFcn(Int_t &, Double_t *, Double_t &f, Double_t *par, Int_t) {
  Int_t np = gg_gr.GetN();
  f = 0;
  Double_t *x = gg_gr.GetX();
  Double_t *y = gg_gr.GetY();
  for (Int_t i=0;i<np;i++) {
    Double_t u = x[i] + par[0];
    Double_t v = y[i] + par[1];
    Double_t dr = par[2] - TMath::Sqrt(u*u+v*v);
    f += dr*dr;  
  }
  std::cout<<"fcn  "<< f<<std::endl;
  
}

void circleFcn2(Int_t &, Double_t *, Double_t &f, Double_t *par, Int_t) {
  Int_t np = gg_gr.GetN();
  f = 0;
  Double_t *x = gg_gr.GetX();
  Double_t *y = gg_gr.GetY();
  for (Int_t i=0;i<np;i++) {
    Double_t u = x[i] + par[0];
    Double_t v = y[i] + par[1];
    Double_t dr = par[2] - TMath::Sqrt(u*u+v*v);
    if(dr>0.07) f += dr*dr; 
    else f += fabs(dr);
  }
}

void PrtLutReco::FitRing(Double_t& x0, Double_t& y0, Double_t& theta){
  TGraph ff_gr;
  Int_t ff_i(0);
  Int_t np = gg_gr.GetN();
  Double_t *x = gg_gr.GetX();
  Double_t *y = gg_gr.GetY();
  for (Int_t i=0;i<np;i++) {
    if( fabs(theta - TMath::Sqrt(x[i]*x[i]+y[i]*y[i]))<0.05) {
      ff_gr.SetPoint(ff_i,x[i],y[i]);
      ff_i++;
    }
  }
  gg_gr = ff_gr;

  //Fit a circle to the graph points
  TVirtualFitter::SetDefaultFitter("Minuit");  //default is Minuit
  TVirtualFitter *fitter = TVirtualFitter::Fitter(0, 3);
  fitter->SetPrecision(0.00000001);
  fitter->SetMaxIterations(1000);

  fitter->SetFCN(circleFcn);
  fitter->SetParameter(0, "x0",   0.03, 0.01, -0.05,0.05);
  fitter->SetParameter(1, "y0",   0, 0.01, -0.05,0.05);
  fitter->SetParameter(2, "R",    theta, 0.01, theta-0.05,theta+0.05);

  //fitter->FixParameter(0);
  //fitter->FixParameter(1);
  fitter->FixParameter(2);
  Double_t arglist[1] = {0};
  fitter->ExecuteCommand("MINIMIZE", arglist, 0);

  // fitter->SetFCN(circleFcn2);
  // fitter->ExecuteCommand("MINIMIZE", arglist, 0);

  x0 = fitter->GetParameter(0);
  y0 = fitter->GetParameter(1);
  theta = fitter->GetParameter(2);
}

Int_t PrtLutReco::FindPdg(Double_t mom, Double_t cangle){
  Int_t pdg[]={11,13,211,321,2212};
  Double_t mass[] = {0.000511,0.1056584,0.139570,0.49368,0.9382723};
  // Int_t pdg[]={211,321,2212};
  // Double_t mass[] = {0.139570,0.49368,0.9382723};
  Double_t tdiff, diff=100;
  Int_t minid=0;
  for(Int_t i=0; i<5; i++){
    tdiff = fabs(cangle - acos(sqrt(mom*mom + mass[i]*mass[i])/mom/1.46907)); //1.46907 - fused silica
    if(tdiff<diff){
      diff = tdiff;
      minid = i;
    }
  }
  return pdg[minid]; 
}


