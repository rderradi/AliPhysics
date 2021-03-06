////////////////////////////////////////////////////////////
//
// Macro for determining which raw V0M value corresponds
// to a certain high-multiplicity-percentile of a min-bias
// data sample for anchoring high-multiplicity-triggered
// data
//
////////////////////////////////////////////////////////////

// global pointer to calib histogram
TH1D *hCalib = 0x0;

void DetermineAnchorsPP( TString lPeriodName = "LHC18f", TString lPass = "pass1" ) {

    Bool_t lUseDefaultAnchorPercentile = kFALSE;
    Double_t  lDefaultAnchorPercentile = 0.10;
    Double_t  lMinimumAnchorPercentile = 0.05;

    // open minimum bias OADB file
    TString lOADBfile = Form("OADB-%s-MB.root", lPeriodName.Data());

    cout << "Opening minimum bias info ... " << endl;
    TFile *foadb = new TFile( lOADBfile.Data(), "READ" );
    AliOADBContainer *lOADBcontainer = (AliOADBContainer*)foadb->Get("MultSel");


    // set percentile boundaries for the estimator histos
    // (based on what is implemented in the calibration)
    Double_t lDesiredBoundaries[1000];
    Long_t   lNDesiredBoundaries=0;
    lDesiredBoundaries[0] = 0.0;
    //From High To Low Multiplicity
    for( Int_t ib = 1; ib < 101; ib++) { // 100 bins  ] 0.0 , 0.1 ]
    //    lNDesiredBoundaries++;
    //    lDesiredBoundaries[lNDesiredBoundaries] = lDesiredBoundaries[lNDesiredBoundaries-1] + 0.001;
    //    // cout << "loop 1: " << lDesiredBoundaries[lNDesiredBoundaries] << endl;
    //}
    //for( Int_t ib = 1; ib < 91; ib++) { // 90 bins  ] 0.1 , 1.0 ]
        lNDesiredBoundaries++;
        lDesiredBoundaries[lNDesiredBoundaries] = lDesiredBoundaries[lNDesiredBoundaries-1] + 0.01;
        // cout << "loop 2: " << lDesiredBoundaries[lNDesiredBoundaries] << endl;
    }
    for( Int_t ib = 1; ib < 91; ib++) { // 90 bins ] 1.0 , 10. ]
        lNDesiredBoundaries++;
        lDesiredBoundaries[lNDesiredBoundaries] = lDesiredBoundaries[lNDesiredBoundaries-1] + 0.1;
        // cout << "loop 3: " << lDesiredBoundaries[lNDesiredBoundaries] << endl;
    }
    for( Int_t ib = 1; ib < 91; ib++) { // 90 bins ] 10.0 , 100.0 ]
        lNDesiredBoundaries++;
        lDesiredBoundaries[lNDesiredBoundaries] = lDesiredBoundaries[lNDesiredBoundaries-1] + 1.0;
        // cout << "loop 4: " << lDesiredBoundaries[lNDesiredBoundaries] << endl;
    }
    // for(int i=0; i<lNDesiredBoundaries; ++i)
    // cout << i << "\t[ " << lDesiredBoundaries[i] << " , " << lDesiredBoundaries[i+1] << " ]\t" << lDesiredBoundaries[i+1]-lDesiredBoundaries[i] << endl;
    

    // open file hashed.txt to get the run numbers
    TString lRunlistFile = "../hashed.txt"; 
    FILE *flist = fopen(lRunlistFile.Data(), "r");
    if(flist==NULL) { Error(0, "Could not open list file %s", lRunlistFile.Data()); return; }

    // save output
    TFile *fout = new TFile(Form("Anchors_%s_VHM.root", lPeriodName.Data()), "RECREATE");
    FILE *fap = fopen(Form("Anchors_%s_VHM.txt", lPeriodName.Data()), "w");

    // auxiliary objects
    TLegend *legEstimator = 0x0;
    //
    TLine *anchorLine = new TLine();
    anchorLine->SetLineStyle(2);
    //
    TLatex *latex = new TLatex();
    latex->SetTextFont(42);
    latex->SetTextSize(0.025);

    // constant function for the scaling factor determination
    TF1 *fpol0 = new TF1("fpol0", "[0]", 0.005, lMinimumAnchorPercentile);
    fpol0->SetLineStyle(3);
    fpol0->SetLineWidth(1);
    fpol0->SetLineColor(kBlack);
    TF1 *fpol0_hi = (TF1*)fpol0->Clone("fpol0_hi");
    TF1 *fpol0_lo = (TF1*)fpol0->Clone("fpol0_lo");
    //
    Int_t npar = 3;
    TF1 *fturnon = new TF1("fturnon", func_turnon, 0., 1., npar);
    fturnon->SetParameters(1., 0.1, -1.);
    fturnon->SetParLimits(1, lMinimumAnchorPercentile, 1.0);
    fturnon->SetParLimits(2, -1.e15, 0.);
    fturnon->SetLineColor(1);

    // open input AnalysisResults.root file for the VHM sample
    TFile *fin = TFile::Open("../VHM/AnalysisResults.root", "READ");
    TTree *treeEvent = (TTree*)fin->Get("MultSelection/fTreeEvent");

    // loop over the files and determine anchors
    Char_t buffer[500];
    Int_t nfiles = 0;
    Int_t nfailed = 0;
    while(fgets(buffer, 500, flist)) {

        cout << "-------------------------------------------------" << endl;

        TString bufferStr = buffer;
        bufferStr.ReplaceAll("\n", "");
        Int_t runnumber = (((TObjString*)bufferStr.Tokenize("/")->At(4))->String()).Atoi();
        cout << "   - run number....................: " << runnumber << endl;

        // define estimator histo for this run
        TH1D* hEstimator = new TH1D(Form("hEstimator_%d", runnumber), "", lNDesiredBoundaries, lDesiredBoundaries);
        hEstimator->Sumw2();
        hEstimator->GetXaxis()->SetTitle("V0M Percentile");
        hEstimator->GetYaxis()->SetTitle("Counts");
        hEstimator->SetStats(0);
        hEstimator->SetLineColor(kRed);

        // get corresponding calibration histogram from OADB
        AliOADBMultSelection* lOADB = (AliOADBMultSelection*)lOADBcontainer->GetObject( runnumber, "Default" );
        if( (Int_t)lOADBcontainer->GetIndexForRun( runnumber )<0 ) {
            cout << "   ---> Warning: no calibration histo found for this run - skipping..." << endl;
            nfiles++;
            // write anchors for this run to a txt file
            fprintf(fap, "%d %d %.2lf %lf\n", runnumber, runnumber, -1., -1.);
            continue;
        }
        //TH1D *hCalib = (TH1D*)lOADB->GetCalibHisto( "hCalib_V0M" );;

        // set the pointer to the calib histo for this run
        hCalib = (TH1D*)lOADB->GetCalibHisto( "hCalib_V0M" );;

        //
        Double_t nall = treeEvent->Draw(Form("get_percentile(fAmplitude_V0A+fAmplitude_V0C)>>hEstimator_%d", runnumber), 
                                        Form("fRunNumber==%d && fEvSel_Triggered && fEvSel_IsNotPileupInMultBins && fEvSel_PassesTrackletVsCluster && fEvSel_INELgtZERO && fEvSel_HasNoInconsistentVertices && TMath::Abs(fEvSel_VtxZ)<=10.0", runnumber),
                                        "goff"); 

        hEstimator->Scale(1., "width");
        Double_t nevents = (Double_t)hEstimator->GetEntries();
        cout << "   - number of events (selected)...: " << nevents << endl;
        //if(nevents<1.e5) {
        //    cout << "   ---> too few events -- skipping..." << endl;
        //    nfiles++;
        //    continue;
        //}

        // draw histogram
        TCanvas *cEstimator = new TCanvas(Form("cEstimator_%d", runnumber), "Estimator Distribution", 10, 10, 1000, 750);
        cEstimator->SetRightMargin(0.05);
        cEstimator->SetTopMargin(0.11);
        //cEstimator->SetLogx();
        //cEstimator->SetLogy();
        //hEstimator->GetXaxis()->SetRangeUser(0., 2.);
        hEstimator->GetXaxis()->SetRangeUser(0., 0.2);
        hEstimator->Draw("hist e0");
        latex->SetNDC();
        latex->SetTextSize(0.06);
        latex->DrawLatex(0.1, 0.93, Form("Run: %d", runnumber));

        // first, fit a pol0 in the flat region (usually up to 0.05)
        hEstimator->Fit(fpol0, "RQ0");
        Double_t flat_top = fpol0->GetParameter(0);

        // get standard deviantion of bin contents in the flat region
        Double_t flat_top_stdev = 0.;
        for(Int_t ibin=1; ibin<=hEstimator->FindBin(lMinimumAnchorPercentile); ++ibin) {
            Double_t content = hEstimator->GetBinContent(ibin);
            Double_t   width = hEstimator->GetBinWidth(ibin);
            flat_top_stdev += TMath::Power((content-flat_top), 2.)*width;
        }
        flat_top_stdev = TMath::Sqrt(flat_top_stdev/lMinimumAnchorPercentile) / 2.;
        fpol0_hi->SetParameter(0, flat_top+flat_top_stdev);
        fpol0_lo->SetParameter(0, flat_top-flat_top_stdev);
        
        // now, fix the constant parameter in the turnon function
        fturnon->SetParameters(1., 0.1, -1.);
        fturnon->FixParameter(0, flat_top);
        
        // get the maximum range to perform the fit
        Double_t range_max = (hEstimator->GetBinLowEdge(hEstimator->FindLastBinAbove())) / 1.8;
        fturnon->SetRange(0.005, (range_max>0.1) ? range_max : 0.1);

        // get anchor percentile
        Double_t anchor_percentile = -1.;
        TFitResultPtr fitr = hEstimator->Fit(fturnon, "RQM");
        fturnon->Draw("lsame");
        TString fitstatus = gMinuit->fCstatu;
        cEstimator->Flush();
        cEstimator->Update();
        cout << "   - fit status....................: " << fitstatus << endl;
        if( !fitstatus.Contains("OK") ) {
            nfailed++;
            if(gROOT->IsBatch()) {
                cout << "   ---> Warning: fit failed! -- skipping this run..." << endl;
                nfiles++;
                // write anchors for this run to a txt file
                fprintf(fap, "%d %d %.2lf %lf\n", runnumber, runnumber, -1., -1.);
                continue;
            }
            cout << "   - Please, provide an anchor percentile to continue: " << endl;
            cout << "     (entering a negative value will skip this run)" << endl;
            cout << "     >>>> anchor percentile: "; 
            cin >> anchor_percentile;
            if(anchor_percentile<0.) {
                cout << "   ---> Warning: percentile provided is negative -- skipping this run..." << endl;;
                nfiles++;
                // write anchors for this run to a txt file
                fprintf(fap, "%d %d %.2lf %lf\n", runnumber, runnumber, -1., -1.);
                continue;
            }
        }
        else {
            if(lUseDefaultAnchorPercentile) anchor_percentile = lDefaultAnchorPercentile;
            else {
                anchor_percentile = fturnon->GetParameter(1);
                if( anchor_percentile >= lDefaultAnchorPercentile ) anchor_percentile = lDefaultAnchorPercentile;
                else {
                    Double_t this_percentile = lDefaultAnchorPercentile;
                    while(this_percentile>=lMinimumAnchorPercentile) {
                        Double_t diff = flat_top - fturnon->Eval(this_percentile);
                        if( diff < (flat_top_stdev) ) { 
                            anchor_percentile = this_percentile; 
                            break; 
                        }
                        else this_percentile -= 0.01; 
                    }
                }
            }
        }
        //fturnon->DrawCopy("lsame");
        fpol0_hi->SetRange(0.005, range_max);
        fpol0_hi->DrawCopy("lsame");
        fpol0_lo->SetRange(0.005, range_max);
        fpol0_lo->DrawCopy("lsame");
        cout << "   - anchor percentile found.......: " << anchor_percentile << endl;
        Double_t y_max = fturnon->Eval(anchor_percentile);
        anchorLine->DrawLine(anchor_percentile, 0., anchor_percentile, y_max);

        // find corresponding anchor point
        Double_t anchor_point = -1.; 
        for(Int_t i=0; i<hCalib->GetNbinsX(); ++i) {
            if(hCalib->GetBinContent( i+1 ) < anchor_percentile ) {
                anchor_point = hCalib->GetBinLowEdge( i+1 );
                break;
            }
        }
        cout << "   - corresponding anchor point....: " << anchor_point << endl;

        // please, confirm
        cEstimator->Update();
        Bool_t isOk = kFALSE;
        cout << "   ---> Is this Ok? (0->no; 1->yes): ";
        cin >> isOk;
        if(!isOk) {
            cout << "   - Please, provide an anchor percentile to continue: " << endl;
            cout << "     (entering a negative value will skip this run)" << endl;
            cout << "     >>>> anchor percentile: "; 
            cin >> anchor_percentile;
            anchorLine->SetLineStyle(1);
            anchorLine->DrawLine(anchor_percentile, 0., anchor_percentile, y_max);
            cEstimator->Update();
        }

        if(anchor_percentile<lMinimumAnchorPercentile) {
            cout << "   ---> Warning: anchor percentile too low -- skipping this run..." << endl;
            nfiles++;
            // write anchors for this run to a txt file
            fprintf(fap, "%d %d %.2lf %lf\n", runnumber, runnumber, -1., -1.);
            continue; 
        }
        
        // find corresponding anchor point
        Double_t anchor_point = -1.; 
        for(Int_t i=0; i<hCalib->GetNbinsX(); ++i) {
            if(hCalib->GetBinContent( i+1 ) < anchor_percentile ) {
                anchor_point = hCalib->GetBinLowEdge( i+1 );
                break;
            }
        }
        cout << "   - confirmed anchor point........: " << anchor_point << endl;


        latex->SetTextAlign(31);
        latex->SetTextSize(0.04);
        latex->DrawLatex(0.88, 0.85, Form("%.3g events", nevents));
        latex->SetTextSize(0.03);
        latex->DrawLatex(0.88, 0.96, "anchor percentile found: ");
        latex->DrawLatex(0.88, 0.92, "corresponding anchor point: ");
        latex->SetTextAlign(11);
        latex->DrawLatex(0.88, 0.96,  Form("%.2lf%%", anchor_percentile));
        latex->DrawLatex(0.88, 0.92, Form("%.1lf", anchor_point));

        // save canvas with histogram and decision
        fout->cd();
        cEstimator->Update();
        cEstimator->Write();

        // write anchors for this run to a txt file
        fprintf(fap, "%d %d %.2lf %lf\n", runnumber, runnumber, anchor_percentile, anchor_point);

        nfiles++;
    }

    fclose(fap);
    fout->Close();

    if(nfailed>0 && gROOT->IsBatch()) {
        cout << endl;
        cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! WARNING !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << endl;
        cout << nfailed << " RUN(S) FAILED DURING DETERMINATION OF THE ANCHOR PERCENTILE " << endl;
        cout << "IT IS STRONGLY RECOMMENDED TO RERUN THE CODE WITHOUT BEING IN BATCH MODE" << endl;
        cout << "IN ORDER TO CHECK THE FITS AND DEFINE THE ANCHORS FOR THE FAILED RUNS.  " << endl;
        cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << endl;
        cout << endl;
    }

}

// function to get V0M percentile
Double_t get_percentile(Double_t amplitude) 
{
    
    Double_t percentile = hCalib->GetBinContent( hCalib->FindBin( amplitude ) );

    return percentile;

}


// function to fit high-mult. trigger turn-on curve
Double_t func_turnon(Double_t *x, Double_t *par)
{

    Double_t f = par[0];
    if(x[0]>par[1]) f = par[0] + par[2]*TMath::Power(x[0]-par[1], 2.);

    return f;

}

