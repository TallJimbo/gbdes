// Code for fitting parameters of defaulted pixel maps to the staring WCS solution.
#include "Std.h"
#include "PixelMapCollection.h"
#include "Instrument.h"
#include "Match.h"
#include "WcsSubs.h"
#include "FitSubroutines.h"

using namespace astrometry;

void
fitDefaulted(PixelMapCollection& pmc,
	     set<Extension*> extensions,
	     const vector<Instrument*>& instruments,
	     const vector<Exposure*>& exposures) {

  // Make a new pixel map collection that will hold only the maps
  // involved in this fit.
  PixelMapCollection pmcFit;

  // Take all wcs's used by these extensions and copy them into
  // pmcFit
  for (auto extnptr : extensions) {
    auto pm = pmc.cloneMap(extnptr->basemapName);
    pmcFit.learnMap(*pm);
    delete pm;
  }

  // Find all the atomic map components that are defaulted.
  // Fix the parameters of all the others
  set<string> defaultedAtoms;
  set<string> fixAtoms;
  for (auto mapname : pmcFit.allMapNames()) {
    if (!pmc.isAtomic(mapname))
      continue;
    if (pmc.getDefaulted(mapname))
      defaultedAtoms.insert(mapname);
    else
      fixAtoms.insert(mapname);
  }
  // If the exposure has any defaulted maps, initialize them
  /**/if (true) {
    cerr << "Initializing maps " ;
    for (auto name : defaultedAtoms) cerr << name << " ";
    cerr << endl;
  }
  pmcFit.learnMap(IdentityMap());  // Make sure we know this one
  pmcFit.setFixed(fixAtoms);
  pmcFit.rebuildParameterVector();

  auto identityMap = pmcFit.issueMap(IdentityMap().getName());
      
  // Make Matches at a grid of points on each extension's device,
  // matching pix coords to the coordinates derived from startWCS.
  list<Match*> matches;
  for (auto extnptr : extensions) {
    const Exposure& expo = *exposures[extnptr->exposure];
    // Get projection used for this extension, and set up
    // startWCS to reproject into this system.
    extnptr->startWcs->reprojectTo(*expo.projection);

    // Get a realization of the extension's map
    auto map = pmcFit.issueMap(extnptr->basemapName);
    
    // Get the boundaries of the device it uses
    Bounds<double> b=instruments[expo.instrument]->domains[extnptr->device];

    // Generate a grid of matched Detections
    const int nGridPoints=512;	// Number of test points for map initialization

    // "errors" on world coords of test points 
    const double testPointSigma = 0.01*ARCSEC/DEGREE;
    const double fitWeight = pow(testPointSigma,-2.);
    // Put smaller errors on the "reference" points.  Doesn't really matter.
    const double refWeight = 10. * fitWeight;
    
    // Distribute points equally in x and y, but shuffle the y coords
    // so that the points fill the rectangle
    vector<int> vx(nGridPoints);
    for (int i=0; i<vx.size(); i++) vx[i]=i;
    vector<int> vy = vx;
    std::random_shuffle(vy.begin(), vy.end());
    double xstep = (b.getXMax()-b.getXMin())/nGridPoints;
    double ystep = (b.getYMax()-b.getYMin())/nGridPoints;
    for (int i=0; i<vx.size(); i++) {
      double xpix = b.getXMin() + (vx[i]+0.5)*xstep;
      double ypix = b.getXMin() + (vy[i]+0.5)*ystep;
      double xw, yw;
      extnptr->startWcs->toWorld(xpix, ypix, xw, yw);
      Detection* dfit = new Detection;
      Detection* dref = new Detection;
      dfit->xpix = xpix;
      dfit->ypix = ypix;
      dref->xpix = xw;
      dref->ypix = yw;
      dref->xw = xw;
      dref->yw = yw;
      
      map->toWorld(xpix,ypix,xw,yw);
      dfit->xw = xw;
      dfit->yw = yw;

      // Set up errors and maps for these matched "detections"
      dfit->wtx = fitWeight;
      dfit->wty = fitWeight;
      dref->wtx = refWeight;
      dref->wty = refWeight;
      dref->map = identityMap;
      dfit->map = map;
      
      matches.push_back(new Match(dfit));
      matches.back()->add(dref);
    }

  }

  // Build CoordAlign object and solve for defaulted parameters
  CoordAlign ca(pmcFit, matches);
  ca.setRelTolerance(0.01);  // weaker tolerance for fit convergence
  ca.fitOnce(false);
  
  // Copy defaulted parameters back into the parent pmc.
  for (auto mapname : defaultedAtoms) {
    auto pm = pmcFit.cloneMap(mapname);
    pmc.copyParamsFrom(*pm);
    delete pm;
  }

  // Delete the Matches and Detections
  for (auto m : matches) {
    m->clear(true);  // Flush the detections
    // And get rid of match itself.
    delete m;
  } 

  return;
}

// Methods of the statistics-accumulator class

Accum::Accum(): sumxw(0.), sumyw(0.), 
		sumx(0.), sumy(0.), sumwx(0.), sumwy(0.), 
		sumxx(0.), sumyy(0.), sumxxw(0.), sumyyw(0.),
		sumdof(0.), xpix(0.), ypix(0.), xw(0.), yw(0.),
		n(0) {}
void 
Accum::add(const Detection* d, double xoff, double yoff, double dof) {
  sumx += (d->xw-xoff);
  sumy += (d->yw-yoff);
  sumxw += (d->xw-xoff)*d->wtx;
  sumyw += (d->yw-yoff)*d->wty;
  sumxxw += (d->xw-xoff)*(d->xw-xoff)*d->wtx;
  sumyyw += (d->yw-yoff)*(d->yw-yoff)*d->wty;
  sumxx += (d->xw-xoff)*(d->xw-xoff);
  sumyy += (d->yw-yoff)*(d->yw-yoff);
  sumwx += d->wtx;
  sumwy += d->wty;
  sumdof += dof;
  ++n;
}
double 
Accum::rms() const {
  return n > 0 ? sqrt( (sumxx+sumyy)/(2.*n)) : 0.;
}
double
Accum::reducedChisq() const {
  return sumdof>0 ? (sumxxw+sumyyw)/(2.*sumdof) : 0.;
}
string 
Accum::summary() const {
  ostringstream oss;
  double dx = 0., dy = 0., sigx=0., sigy=0.;
  if (n>0) {
    dx = sumxw / sumwx;
    sigx = 1./sqrt(sumwx);
    dy = sumyw / sumwy;
    sigy = 1./sqrt(sumwy);
  }
  oss << setw(4) << n 
      << fixed << setprecision(1)
      << " " << setw(6) << sumdof
      << " " << setw(5) << dx*1000.*DEGREE/ARCSEC 
      << " " << setw(5) << sigx*1000.*DEGREE/ARCSEC
      << " " << setw(5) << dy*1000.*DEGREE/ARCSEC 
      << " " << setw(5) << sigy*1000.*DEGREE/ARCSEC
      << " " << setw(5) << rms()*1000.*DEGREE/ARCSEC
      << setprecision(2) 
      << " " << setw(5) << reducedChisq()
      << setprecision(0) << noshowpoint
      << " " << setw(5) << xpix 
      << " " << setw(5) << ypix
      << setprecision(5) << showpoint << showpos
      << " " << setw(9) << xw 
      << " " << setw(9) << yw ;
  return oss.str();
}

void
readFields(string inputTables,
	   string outCatalog,
	   NameIndex& fieldNames,
	   vector<astrometry::SphericalCoords*>& fieldProjections) {
  FITS::FitsTable in(inputTables, FITS::ReadOnly, "Fields");
  FITS::FitsTable out(outCatalog,
		      FITS::ReadWrite + FITS::OverwriteFile + FITS::Create,
		      "Fields");
  img::FTable ft = in.extract();
  out.adopt(ft);
  vector<double> ra;
  vector<double> dec;
  vector<string> name;
  ft.readCells(name, "Name");
  ft.readCells(ra, "RA");
  ft.readCells(dec, "Dec");
  for (int i=0; i<name.size(); i++) {
    spaceReplace(name[i]);
    fieldNames.append(name[i]);
    Orientation orient(SphericalICRS(ra[i]*DEGREE, dec[i]*DEGREE));
    fieldProjections.push_back( new Gnomonic(orient));
  }
}
