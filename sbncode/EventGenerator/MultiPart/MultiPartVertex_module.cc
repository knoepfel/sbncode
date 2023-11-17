////////////////////////////////////////////////////////////////////////
// Class:       MultiPartVertex
// Module Type: producer
// File:        MultiPartVertex_module.cc
//
// Generated at Tue Dec 13 15:48:59 2016 by Kazuhiro Terao using artmod
// from cetpkgsupport v1_11_00.
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Principal/SubRun.h"
//#include "art/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include <memory>
#include <vector>
#include "cetlib/pow.h"

#include "CLHEP/Random/RandFlat.h"
#include "CLHEP/Random/RandGauss.h"
#include "TRandom.h"
#include "nurandom/RandomUtils/NuRandomService.h"
#include "larcore/Geometry/Geometry.h"
#include "larcore/CoreUtils/ServiceUtil.h"
#include "larcoreobj/SummaryData/RunData.h"
#include "larcoreobj/SimpleTypesAndConstants/PhysicalConstants.h"

#include "nusimdata/SimulationBase/MCTruth.h"
#include "nusimdata/SimulationBase/MCParticle.h"

#include "TLorentzVector.h"
#include "TDatabasePDG.h"

struct PartGenParam {
  std::vector<int       > pdg;
  std::vector<double    > mass;
  std::array <size_t, 2 > multi;
  std::array <double, 2 > kerange;
  bool use_mom;
  double weight;
};

class MultiPartVertex;

class MultiPartVertex : public art::EDProducer {
public:
  explicit MultiPartVertex(fhicl::ParameterSet const & p);
  // The destructor generated by the compiler is fine for classes
  // without bare pointers or other resource use.
  ~MultiPartVertex();

  // Plugins should not be copied or assigned.
  MultiPartVertex(MultiPartVertex const &) = delete;
  MultiPartVertex(MultiPartVertex &&) = delete;
  MultiPartVertex & operator = (MultiPartVertex const &) = delete;
  MultiPartVertex & operator = (MultiPartVertex &&) = delete;

  // Required functions.
  void produce(art::Event & e) override;

  void beginRun(art::Run& run) override;

  void GenPosition(double& x, double& y, double& z);
  void GenBoost(double& bx, double& by, double& bz);

  std::array<double, 3U> extractDirection() const;
  void GenMomentum(const PartGenParam& param, const double& mass, double& px, double& py, double& pz);
  void GenMomentum(const PartGenParam& param, const double& mass, double& px, double& py, double& pz, bool& same_range);

  void GenMomentumSF(const double& sf, const double& mass, const double& p, double &p_sf);
  std::vector<size_t> GenParticles() const;

private:

  CLHEP::HepRandomEngine& fFlatEngine;
  std::unique_ptr<CLHEP::RandFlat> fFlatRandom;
  std::unique_ptr<CLHEP::RandGauss> fNormalRandom;

  // exception thrower
  void abort(const std::string msg) const;

  // array of particle info for generation
  std::vector<PartGenParam> _param_v;

  // g4 time of generation
  double _t0;
  double _t0_sigma;

  // g4 position
  std::array<double,2> _xrange;
  std::array<double,2> _yrange;
  std::array<double,2> _zrange;

  // TPC range
  std::vector<std::vector<unsigned short> > _tpc_v;

  // multiplicity constraint
  size_t _multi_min;
  size_t _multi_max;

  // Range of boosts
  std::array<double,2> _gamma_beta_range;

  // verbosity flag
  unsigned short _debug;

  unsigned short _use_boost;

  unsigned short _revert;
};

void MultiPartVertex::abort(const std::string msg) const
{
  std::cerr << "\033[93m" << msg.c_str() << "\033[00m" << std::endl;
  throw std::exception();
}

MultiPartVertex::~MultiPartVertex()
{ }

MultiPartVertex::MultiPartVertex(fhicl::ParameterSet const & p)
  : EDProducer(p)
  , fFlatEngine(art::ServiceHandle<rndm::NuRandomService>()->registerAndSeedEngine(
										   createEngine(0, "HepJamesRandom", "GenVertex"), "HepJamesRandom", "GenVertex"))
    //, fFlatEngine(art::ServiceHandle<rndm::NuRandomService>()->createEngine(*this, "HepJamesRandom", "Gen", p, "Seed"))
    // Initialize member data here.
{

  //
  // Random engine initialization
  //
  fFlatRandom = std::make_unique<CLHEP::RandFlat>(fFlatEngine,0,1);
  fNormalRandom = std::make_unique<CLHEP::RandGauss>(fFlatEngine);

  produces< std::vector<simb::MCTruth>   >();
  produces< sumdata::RunData, art::InRun >();

  _debug = p.get<unsigned short>("DebugMode",0);
  _revert = p.get<unsigned short>("Revert",0);
  _use_boost = p.get<unsigned short>("UseBoost",0);
  _t0 = p.get<double>("G4Time");
  _t0_sigma = p.get<double>("G4TimeJitter");
  if(_t0_sigma < 0) this->abort("Cannot have a negative value for G4 time jitter");

  _multi_min = p.get<size_t>("MultiMin");
  _multi_max = p.get<size_t>("MultiMax");

  _tpc_v  = p.get<std::vector<std::vector<unsigned short> > >("TPCRange");
  auto const xrange = p.get<std::vector<double> > ("XRange");
  auto const yrange = p.get<std::vector<double> > ("YRange");
  auto const zrange = p.get<std::vector<double> > ("ZRange");

  auto const part_cfg = p.get<fhicl::ParameterSet>("ParticleParameter");
  _gamma_beta_range = p.get<std::array<double,2> >("GammaBetaRange", {0.0,0.0});

  _param_v.clear();
  auto const pdg_v      = part_cfg.get<std::vector<std::vector<int>    > > ("PDGCode");
  auto const minmult_v  = part_cfg.get<std::vector<unsigned short> > ("MinMulti");
  auto const maxmult_v  = part_cfg.get<std::vector<unsigned short> > ("MaxMulti");
  auto const weight_v   = part_cfg.get<std::vector<double> > ("ProbWeight");

  auto kerange_v  = part_cfg.get<std::vector<std::vector<double> > > ("KERange");
  auto momrange_v = part_cfg.get<std::vector<std::vector<double> > > ("MomRange");

  if( (kerange_v.empty() && momrange_v.empty()) ||
      (!kerange_v.empty() && !momrange_v.empty()) ) {
    this->abort("Only one of KERange or MomRange must be empty!");
  }

  bool use_mom = false;
  if(kerange_v.empty()){
    kerange_v = momrange_v;
    use_mom = true;
  }
  // sanity check
  if( pdg_v.size() != kerange_v.size() ||
      pdg_v.size() != minmult_v.size() ||
      pdg_v.size() != maxmult_v.size() ||
      pdg_v.size() != weight_v.size() )
    this->abort("configuration parameters have incompatible lengths!");

  // further sanity check (1 more depth for some double-array)
  for(auto const& r : pdg_v    ) { if(              r.empty()  ) this->abort("PDG code not given!");                        }
  for(auto const& r : kerange_v) { if(              r.size()!=2) this->abort("Incompatible legnth @ KE vector!");           }
  if(_gamma_beta_range[0] > _gamma_beta_range[1]) this->abort("Incompatible boost range!");

  size_t multi_min = 0;
  for(size_t idx=0; idx<minmult_v.size(); ++idx) {
    if(minmult_v[idx] > maxmult_v[idx]) this->abort("Particle MinMulti > Particle MaxMulti!");
    if(minmult_v[idx] > _multi_max) this->abort("Particle MinMulti > overall MultiMax!");
    multi_min += minmult_v[idx];
  }
  _multi_min = std::max(_multi_min, multi_min);
  if(_multi_max < _multi_min) this->abort("Overall MultiMax <= overall MultiMin!");

  /*
    if(!xrange.empty() && xrange.size() >2) this->abort("Incompatible legnth @ X vector!" );
    if(!yrange.empty() && yrange.size() >2) this->abort("Incompatible legnth @ Y vector!" );
    if(!zrange.empty() && zrange.size() >2) this->abort("Incompatible legnth @ Z vector!" );

    // range register
    if(xrange.size()==1) { _xrange[0] = _xrange[1] = xrange[0]; }
    if(xrange.size()==2) { _xrange[0] = xrange[0]; _xrange[1] = xrange[1]; }
    if(yrange.size()==1) { _yrange[0] = _yrange[1] = yrange[0]; }
    if(yrange.size()==2) { _yrange[0] = yrange[0]; _yrange[1] = yrange[1]; }
    if(zrange.size()==1) { _zrange[0] = _zrange[1] = zrange[0]; }
    if(zrange.size()==2) { _zrange[0] = zrange[0]; _zrange[1] = zrange[1]; }
  */

  if(!xrange.empty() && xrange.size() >2) this->abort("Incompatible legnth @ X vector!" );
  if(!yrange.empty() && yrange.size() >2) this->abort("Incompatible legnth @ Y vector!" );
  if(!zrange.empty() && zrange.size() >2) this->abort("Incompatible legnth @ Z vector!" );

  // slight modification from mpv: define the overall volume across specified TPC IDs + range options
  double xmin,xmax,ymin,ymax,zmin,zmax;
  xmin = ymin = zmin =  1.e20;
  xmax = ymax = zmax = -1.e20;
  // Implementation of required member function here.
  auto geop = lar::providerFrom<geo::Geometry>();
  for(auto const& tpc_id : _tpc_v) {
    assert(tpc_id.size() == 2);
    size_t cid = tpc_id[0];
    size_t tid = tpc_id[1];
    auto const& cryostat = geop->Cryostat(geo::CryostatID(cid));
    assert(cryostat.HasTPC(tid));

    auto const& tpc = cryostat.TPC(tid);
    auto const& tpcabox = tpc.ActiveBoundingBox();
    xmin = std::min(tpcabox.MinX(), xmin);
    ymin = std::min(tpcabox.MinY(), ymin);
    zmin = std::min(tpcabox.MinZ(), zmin);
    xmax = std::max(tpcabox.MaxX(), xmax);
    ymax = std::max(tpcabox.MaxY(), ymax);
    zmax = std::max(tpcabox.MaxZ(), zmax);

    if(_debug) {
      std::cout << "Using Cryostat " << tpc_id[0] << " TPC " << tpc_id[1]
		<< " ... X " << xmin << " => " << xmax
		<< " ... Y " << ymin << " => " << ymax
		<< " ... Z " << zmin << " => " << zmax
		<< std::endl;
    }
  }

  // range register
  if(xrange.size()==1) { _xrange[0] = _xrange[1] = xrange[0]; }
  if(yrange.size()==1) { _yrange[0] = _yrange[1] = yrange[0]; }
  if(zrange.size()==1) { _zrange[0] = _zrange[1] = zrange[0]; }
  if(xrange.size()==2) { _xrange[0] = xrange[0]; _xrange[1] = xrange[1]; }
  if(yrange.size()==2) { _yrange[0] = yrange[0]; _yrange[1] = yrange[1]; }
  if(zrange.size()==2) { _zrange[0] = zrange[0]; _zrange[1] = zrange[1]; }

  _xrange[0] = xmin + _xrange[0];
  _xrange[1] = xmax - _xrange[1];
  _yrange[0] = ymin + _yrange[0];
  _yrange[1] = ymax - _yrange[1];
  _zrange[0] = zmin + _zrange[0];
  _zrange[1] = zmax - _zrange[1];

  // check
  assert(_xrange[0] <= _xrange[1]);
  assert(_yrange[0] <= _yrange[1]);
  assert(_zrange[0] <= _zrange[1]);

  if(_debug>0) {
    std::cout<<"Particle generation world boundaries..."<<std::endl
	     <<"X " << _xrange[0] << " => " << _xrange[1] << std::endl
	     <<"Y " << _yrange[0] << " => " << _yrange[1] << std::endl
	     <<"Z " << _zrange[0] << " => " << _zrange[1] << std::endl;
  }


  // register
  //auto db = new TDatabasePDG;
  auto db = TDatabasePDG::Instance();
  for(size_t idx=0; idx<pdg_v.size(); ++idx) {
    auto const& pdg     = pdg_v[idx];
    auto const& kerange = kerange_v[idx];
    PartGenParam param;
    param.use_mom    = use_mom;
    param.pdg        = pdg;
    param.kerange[0] = kerange[0];
    param.kerange[1] = kerange[1];
    param.mass.resize(pdg.size());
    param.multi[0]   = minmult_v[idx];
    param.multi[1]   = maxmult_v[idx];
    param.weight     = weight_v[idx];
    for(size_t i=0; i<pdg.size(); ++i)
      param.mass[i] = db->GetParticle(param.pdg[i])->Mass();

    // sanity check
    if(kerange[0]<0 || kerange[1]<0)
      this->abort("You provided negative energy? Fuck off Mr. Trump.");

    // overall range check
    if(param.kerange[0] > param.kerange[1]) this->abort("KE range has no phase space...");

    if(_debug>0) {
      std::cout << "Generating particle (PDG";
      for(auto const& pdg : param.pdg) std::cout << " " << pdg;
      std::cout << ")" << std::endl
		<< (param.use_mom ? "    KE range ....... " : "    Mom range ....... ")
		<< param.kerange[0] << " => " << param.kerange[1] << " MeV" << std::endl
		<< std::endl;
    }

    _param_v.push_back(param);
  }
}

void MultiPartVertex::beginRun(art::Run& run)
{
  // grab the geometry object to see what geometry we are using
  art::ServiceHandle<geo::Geometry> geo;

  std::unique_ptr<sumdata::RunData> runData(new sumdata::RunData(geo->DetectorName()));

  run.put(std::move(runData), art::fullRun());

  return;
}

std::vector<size_t> MultiPartVertex::GenParticles() const {

  std::vector<size_t> result;
  std::vector<size_t> gen_count_v(_param_v.size(),0);

  int num_part = (int)(fFlatRandom->fire(_multi_min,_multi_max+1-1.e-10));

  // generate min multiplicity first
  std::vector<double> weight_v(_param_v.size(),0);
  for(size_t idx=0; idx<_param_v.size(); ++idx) {
    weight_v[idx] = _param_v[idx].weight;
    for(size_t ctr=0; ctr<_param_v[idx].multi[0]; ++ctr) {
      result.push_back(idx);
      gen_count_v[idx] += 1;
      num_part -= 1;
    }
    if(gen_count_v[idx] >= _param_v[idx].multi[1])
      weight_v[idx] = 0.;
  }

  assert(num_part >= 0);

  while(num_part) {

    double total_weight = 0;
    for(auto const& v : weight_v) total_weight += v;

    double rval = 0;
    rval = fFlatRandom->fire(0,total_weight);

    size_t idx = 0;
    for(idx = 0; idx < weight_v.size(); ++idx) {
      rval -= weight_v[idx];
      if(rval <=0.) break;
    }

    // register to the output
    result.push_back(idx);

    // if generation count exceeds max, set probability weight to be 0
    gen_count_v[idx] += 1;
    if(gen_count_v[idx] >= _param_v[idx].multi[1])
      weight_v[idx] = 0.;

    --num_part;
  }
  return result;
}


void MultiPartVertex::GenPosition(double& x, double& y, double& z) {

  x = fFlatRandom->fire(_xrange[0],_xrange[1]);
  y = fFlatRandom->fire(_yrange[0],_yrange[1]);
  z = fFlatRandom->fire(_zrange[0],_zrange[1]);

  if(_debug>0) {
    std::cout << "Generating a rain particle at ("
	      << x << "," << y << "," << z << ")" << std::endl;
  }
}

void MultiPartVertex::GenBoost(double& bx, double& by, double& bz) {

  double gbmag = 0.;
  if ( _gamma_beta_range[1] > _gamma_beta_range[0]) {
    gbmag = fFlatRandom->fire(_gamma_beta_range[0],_gamma_beta_range[1]);
  }
  else {
    gbmag = _gamma_beta_range[0];
  }
  double ct = fFlatRandom->fire(-1.0,1.0);
  double st = TMath::Sqrt(1.0 - ct*ct);
  double phi = fFlatRandom->fire(0.0,2.0 * M_PI);
  double bmag = gbmag / TMath::Sqrt(1.0 + gbmag*gbmag);
    
  bx = bmag * st * TMath::Cos(phi);
  by = bmag * st * TMath::Sin(phi);
  bz = bmag * ct;

  if(_debug>0) {
    std::cout << "Generated boost parameter gamma beta = " << gbmag << ", cos(theta) = " << ct 
	      << ", phi = " << phi << std::endl;
    std::cout << "Generating a boost ("
	      << bx << "," << by << "," << bz << ")" << std::endl;
  }
}

/*
void MultiPartVertex::GenPosition(double& x, double& y, double& z) {

  auto const& tpc_id = _tpc_v.at((size_t)(fFlatRandom->fire(0,_tpc_v.size())));
  // Implementation of required member function here.
  auto geop = lar::providerFrom<geo::Geometry>();
  size_t cid = tpc_id[0];
  size_t tid = tpc_id[1];
  auto const& cryostat = geop->Cryostat(cid);
  if(!cryostat.HasTPC(tid)) {
  std::cerr<< "\033[93mTPC " << tid << " not found... in cryostat " << cid << "\033[00m" << std::endl;
    throw std::exception();
  }

  auto const& tpc = cryostat.TPC(tid);
  auto const& tpcabox = tpc.ActiveBoundingBox();
  double xmin = tpcabox.MinX() + _xrange[0];
  double xmax = tpcabox.MaxX() - _xrange[1];
  double ymin = tpcabox.MinY() + _yrange[0];
  double ymax = tpcabox.MaxY() - _yrange[1];
  double zmin = tpcabox.MinZ() + _zrange[0];
double zmax = tpcabox.MaxZ() - _zrange[1];
x = fFlatRandom->fire(xmin,xmax);
y = fFlatRandom->fire(ymin,ymax);
z = fFlatRandom->fire(zmin,zmax);

}
*/

std::array<double, 3U> MultiPartVertex::extractDirection() const {
  double ct = fFlatRandom->fire(-1.0,1.0);
  double st = TMath::Sqrt(1.0 - ct*ct);
  double phi = fFlatRandom->fire(0.0,2.0*M_PI);
  double px = st * TMath::Cos(phi);
  double py = st * TMath::Sin(phi);
  double pz = ct;
  std::array<double, 3U> result = { px, py, pz };
  return result;
}
/*
std::array<double, 3U> MultiPartVertex::extractDirection() const {
    double px, py, pz;
    px = fNormalRandom->fire(0, 1);
    py = fNormalRandom->fire(0, 1);
    pz = fNormalRandom->fire(0, 1);
    double p = std::hypot(px, py, pz);
    px = px / p;
    py = py / p;
    pz = pz / p;
    std::array<double, 3U> result = { px, py, pz };
    return result;
}
*/

void MultiPartVertex::GenMomentum(const PartGenParam& param, const double& mass, double& px, double& py, double& pz) {

  double tot_energy = 0;
  if(param.use_mom)
    tot_energy = sqrt(cet::square(fFlatRandom->fire(param.kerange[0],param.kerange[1])) + cet::square(mass));
  else
    tot_energy = fFlatRandom->fire(param.kerange[0],param.kerange[1]) + mass;

  double mom_mag = sqrt(cet::square(tot_energy) - cet::square(mass));

  std::array<double, 3U> p = extractDirection();
  px = p[0]; py = p[1]; pz = p[2];

  if(_debug>1)
    std::cout << "    Direction : (" << px << "," << py << "," << pz << ")" << std::endl
	      << "    Momentum  : " << mom_mag << " [MeV/c]" << std::endl
	      << "    Energy    : " << tot_energy << " [MeV/c^2]" << std::endl;
  px *= mom_mag;
  py *= mom_mag;
  pz *= mom_mag;

}

void MultiPartVertex::GenMomentum(const PartGenParam& param, const double& mass, double& px, double& py, double& pz, bool& same_range) {

  if (!same_range){
    GenMomentum(param, mass, px, py, pz);
  }
  else{
    double tot_energy = 0;
    if(param.use_mom)
      tot_energy = sqrt(cet::square(fFlatRandom->fire(param.kerange[0],param.kerange[1])) + cet::square(mass));
    else
      tot_energy = fFlatRandom->fire(param.kerange[0],1) + mass;

    double mom_mag = sqrt(cet::square(tot_energy) - cet::square(mass));

    std::array<double, 3U> p = extractDirection();
    px = p[0]; py = p[1]; pz = p[2];

    if(_debug>1)
      std::cout << "    Direction : (" << px << "," << py << "," << pz << ")" << std::endl
		<< "    Momentum  : " << mom_mag << " [MeV/c]" << std::endl
		<< "    Energy    : " << tot_energy << " [MeV/c^2]" << std::endl;
    px *= mom_mag;
    py *= mom_mag;
    pz *= mom_mag;
  }
}


void MultiPartVertex::GenMomentumSF(const double& sf, const double& m, const double& p, double& p_sf)
{
  p_sf = sqrt( sf*( 2*(sf-1)*cet::square(m)-2*(sf-1)*m*sqrt(cet::square(m)+cet::square(p))+sf*cet::square(p) ) )/p;
}

void MultiPartVertex::produce(art::Event & e)
{
  if(_debug>0) std::cout << "Processing a new event..." << std::endl;

  std::unique_ptr< std::vector<simb::MCTruth> > mctArray(new std::vector<simb::MCTruth>);

  double g4_time = fFlatRandom->fire(_t0 - _t0_sigma/2., _t0 + _t0_sigma/2.);

  double x, y, z;
  GenPosition(x,y,z);
  TLorentzVector pos(x,y,z,g4_time);

  double bx,by,bz;
  GenBoost(bx,by,bz);

  simb::MCTruth mct;

  mct.SetOrigin(simb::kBeamNeutrino);

  std::vector<simb::MCParticle> part_v;

  std::vector<double> E_vec;
  std::vector<double> mass_vec;
  std::vector<int> pdg_vec;
  std::vector<double> px_vec;
  std::vector<double> py_vec;
  std::vector<double> pz_vec;

  auto const param_idx_v = GenParticles();
  if(_debug)
    std::cout << "Event Vertex @ (" << x << "," << y << "," << z << ") ... " << param_idx_v.size() << " particles..." << std::endl;

  for(size_t idx=0; idx<param_idx_v.size(); ++idx) {
    auto const& param = _param_v[param_idx_v[idx]];
    double px,py,pz;
    bool same_range = true;
    // decide which particle
    size_t pdg_index = (size_t)(fFlatRandom->fire(0,param.pdg.size()-1.e-10));
    auto const& pdg  = param.pdg[pdg_index];
    auto const& mass = param.mass[pdg_index];
    if(_debug) std::cout << "  " << idx << "th instance PDG " << pdg << std::endl;

    if (_revert) {
      GenMomentum(param,mass,px,py,pz);
    }
    else {
      GenMomentum(param,mass,px,py,pz,same_range);
    }
    // moving boost
    //
    //TLorentzVector mom(px,py,pz,sqrt(cet::square(px)+cet::square(py)+cet::square(pz)+cet::square(mass)));
    //
    /*
       if(_use_boost){
       mom.Boost(bx,by,bz);
         px=mom.X();
	   py=mom.Y();
	     pz=mom.Z();
	     }*/

    E_vec.push_back(sqrt(cet::square(px)+cet::square(py)+cet::square(pz)+cet::square(mass)));
    mass_vec.push_back(mass);
    pdg_vec.push_back(pdg);
    px_vec.push_back(px);
    py_vec.push_back(py);
    pz_vec.push_back(pz); 
        
  }

  if(_revert){
    for(size_t idx=0; idx<param_idx_v.size(); ++idx) {
      TLorentzVector mom(px_vec[idx],py_vec[idx],pz_vec[idx],E_vec[idx]);
      if (_use_boost) mom.Boost(bx, by, bz);
      simb::MCParticle part(part_v.size(), pdg_vec[idx], "primary", 0, mass_vec[idx], 1);
      part.AddTrajectoryPoint(pos,mom);
      part_v.emplace_back(std::move(part));
    }
  }
  else{

    double total_KE = 0;
    for (size_t idx=0; idx<E_vec.size(); ++idx){
      total_KE += (E_vec[idx]-mass_vec[idx]);
      if(_debug) std::cout << "E: "<< E_vec[idx] <<" , m: "<< mass_vec[idx]<< " , KE: "<< E_vec[idx]-mass_vec[idx]<< std::endl;
    }
    if(_debug) std::cout << "total KE: "<< total_KE << std::endl;
    
    if (total_KE>1.){
      
      for(size_t idx=0; idx<param_idx_v.size(); ++idx) {
	auto const& param = _param_v[param_idx_v[idx]];

	if(param.kerange[1]!=1){
	  // lepton scale here.
	  double mom_sf = 1;
	  double temp_p = sqrt(cet::square(px_vec[idx])+cet::square(py_vec[idx])+cet::square(pz_vec[idx]));
	  if(_debug) std::cout << "KE range: "<< param.kerange[1] <<  " , pdg :  " << pdg_vec[idx] << std::endl;
	  GenMomentumSF(param.kerange[1], mass_vec[idx], temp_p, mom_sf);
	  if(_debug) std::cout << "particle KE sf: "<< param.kerange[1] << ", p : "<< temp_p << " , mass :  " << mass_vec[idx]<< " , mom_sf: "<< mom_sf << std::endl;
	  double E_scaled = mass_vec[idx]+param.kerange[1]*(E_vec[idx]-mass_vec[idx]);
	  if(_debug) std::cout << "particle E : "<< E_vec[idx]<< " , scaled E : " << E_scaled << std::endl;
	  TLorentzVector mom(mom_sf*px_vec[idx],mom_sf*py_vec[idx],mom_sf*pz_vec[idx],E_scaled);
	  if (_use_boost) mom.Boost(bx, by, bz);
	  simb::MCParticle part(part_v.size(), pdg_vec[idx], "primary", 0, mass_vec[idx], 1);
	  part.AddTrajectoryPoint(pos,mom);
	  part_v.emplace_back(std::move(part));
	}
	else{
	  TLorentzVector mom(px_vec[idx],py_vec[idx],pz_vec[idx],E_vec[idx]);
	  if (_use_boost) mom.Boost(bx, by, bz);
	  simb::MCParticle part(part_v.size(), pdg_vec[idx], "primary", 0, mass_vec[idx], 1);
	  part.AddTrajectoryPoint(pos,mom);
	  part_v.emplace_back(std::move(part));
	}
	//simb::MCParticle part(part_v.size(), pdg_vec[idx], "primary", 0, mass_vec[idx], 1);
	//part.AddTrajectoryPoint(pos,mom);
	//part_v.emplace_back(std::move(part));
      }

    }

    else{

      double gen_total_KE = fFlatRandom->fire(0,1);      
      double total_KE_sf = gen_total_KE/total_KE; 
      if(_debug) std::cout << "gen_total_KE: "<< gen_total_KE <<  " , KE scale factor :  " << total_KE_sf << std::endl;

      for(size_t idx=0; idx<param_idx_v.size(); ++idx) {
	auto const& param = _param_v[param_idx_v[idx]];
	double mom_sf = 1;
	double temp_p = sqrt(cet::square(px_vec[idx])+cet::square(py_vec[idx])+cet::square(pz_vec[idx]));
	if(_debug) std::cout << "KE range: "<< param.kerange[1] <<  " , pdg :  " << pdg_vec[idx] << std::endl;
	GenMomentumSF(total_KE_sf*param.kerange[1], mass_vec[idx], temp_p, mom_sf);
	if(_debug) std::cout << "particle KE sf: "<< total_KE_sf*param.kerange[1] << ", p : "<< temp_p << " , mass :  " << mass_vec[idx]<< " , mom_sf: "<< mom_sf << std::endl;
	double E_scaled = mass_vec[idx]+total_KE_sf*param.kerange[1]*(E_vec[idx]-mass_vec[idx]);
	if(_debug) std::cout << "particle E : "<< E_vec[idx]<< " , scaled E : " << E_scaled << std::endl;
	TLorentzVector mom(mom_sf*px_vec[idx],mom_sf*py_vec[idx],mom_sf*pz_vec[idx],E_scaled);
	if (_use_boost) mom.Boost(bx, by, bz);
	simb::MCParticle part(part_v.size(), pdg_vec[idx], "primary", 0, mass_vec[idx], 1);
	part.AddTrajectoryPoint(pos,mom);
	part_v.emplace_back(std::move(part));
      }

    }
  }
  if(_debug) std::cout << "Total number particles: " << mct.NParticles() << std::endl;

  simb::MCParticle nu(mct.NParticles(), 16, "primary", mct.NParticles(), 0, 0);
  double px=0;
  double py=0;
  double pz=0;
  double en=0;
  for(auto const& part : part_v) {
    px += part.Momentum().Px();
    py += part.Momentum().Py();
    pz += part.Momentum().Pz();
    en += part.Momentum().E();
  }
  TLorentzVector mom(px,py,pz,en);
  nu.AddTrajectoryPoint(pos,mom);

  mct.Add(nu);
  for(auto& part : part_v)
    mct.Add(part);

  // Set the neutrino for the MCTruth object:
  // NOTE: currently these parameters are all pretty much a guess...
  mct.SetNeutrino(simb::kCC,
		  simb::kQE, // not sure what the mode should be here, assumed that these are all QE???
		  simb::kCCQE, // not sure what the int_type should be either...
		  0, // target is AR40? not sure how to specify that...
		  0, // nucleon pdg ???
		  0, // quark pdg ???
		  -1.0,  // W ??? - not sure how to calculate this from the above
		  -1.0,  // X ??? - not sure how to calculate this from the above
		  -1.0,  // Y ??? - not sure how to calculate this from the above
		  -1.0); // Qsqr ??? - not sure how to calculate this from the above

  mctArray->push_back(mct);

  e.put(std::move(mctArray));
}

DEFINE_ART_MODULE(MultiPartVertex)
