/* This is a collection of subprograms for mwap that utilize the 
datascope db library.  Primary use is to write out special tables
defined by mwap.  This requires a schema addendum to css3.0 
Currently this is called MWaveletX.X where X.X is a version number.
I won't attempt to keep these comments consistent with an evolution 
of a special schema definition. */

#include <stdlib.h>
#include "multiwavelet.h"

#define AMPCOMP "MAJ"
/* This function creates the mwslow table output of mwap.
This table hold slowness vector estimates.

arguments:
	phase - name of seismic phase
	u - estimated slowness vector
	timeref - reference time at reference station
	w - time window structure that defines the analysis window
		for this slowness estimate
	array - array name to use in table for station key field
	evid - css3.0 evid of parent data
	bankid - defines unique multiwavelet bank (could be 
		extracted from traces of gather, but it is
		so deep in indirection it gets ridiculous)
	fc - center frequency (in Hz) of this wavelet bank.	
	C - 3x3 covariance matrix estimate for this slowness vector
		(ux,uy, t order assumed )
	db - output database
Returns 0 if dbaddv was successful, -1 if dbaddv fails.

Author:  G Pavlis
Written:  March 2000
*/
int MWdb_save_slowness_vector(char *phase, 
	MWSlowness_vector *u,
	double timeref,
	Time_Window *w,
	char *array,
	int evid,
	int bankid,
	double fc,
	double *C,
	int nsta,
	int ncomp,
	Dbptr db)
{
	double time, twin, slo, azimuth, cxx, cyy, cxy;

	slo = hypot(u->ux,u->uy);
	azimuth = atan2(u->ux,u->uy);
	if(azimuth<0.0) azimuth += 2.0*M_PI;
	azimuth = deg(azimuth);
	time = timeref + (w->si)*((double)(w->tstart));
	twin = ((double)((w->tend)-(w->tstart)))*(w->si);
	db = dblookup(db,0,"mwslow",0,0);

	if( dbaddv(db,0,"sta",array,
		"evid",evid,
		"bankid",bankid,
		"phase", phase,
		"fc",fc,
		"time",time,
		"twin",twin,
		"slo",slo,
		"azimuth",azimuth,
		"cxx",C[0],
		"cyy",C[4],
		"cxy",C[1],
		"nsta",nsta,
		"ncomp",ncomp,
		"algorithm","mwap",0) < 0) 
	{
		elog_notify(0,
			"dbaddv error for mwslow table on evid %d fc=%lf\n",
				evid,fc);
		return(-1);
	}
	else
		return(0);
}
/* This routine saves array average amplitude estimates in a table
called mwavgamp.  This routine always adds one and only one record.

arguments:

array - array name assigned to the group of stations used to produce
	this set of averages
evid - css3.0 event id
bankid - multiwavelet group id 
phase - phase name as in css3.0
fc - center frequency (in Hz) of the wavelets used
timeref - reference start time.  This time needs to be lag corrected
	so the window defintion works correctly.  
w - time window structure used in mwap.  relation of timeref and
	this structure are shown clearly below.  
mwamp - computed array average amplitude
erramp - estimate of error in mwamp 
	Note: both mwap and erramp are log10 values derived from nm/s
	converted trace data (assumed).
ndgf - degrees of freedom in estimate of mwamp
db - pointer to output database 

Author:  G Pavlis
Written:  March 2000
*/
int MWdb_save_avgamp(char *array,
	int evid, 
	int bankid,
	char *phase,
	double fc,
	double timeref,
	Time_Window *w,
	double mwamp,
	double erramp,
	int ndgf,
	Dbptr db)
{
	double time,twin;

	time = timeref + (w->si)*((double)(w->tstart));
	twin = ((double)((w->tend)-(w->tstart)))*(w->si);
	db = dblookup(db,0,"mwavgamp",0,0);

	/* Note algorithm and ampcomp are frozen here.   */

	if( dbaddv(db,0,"sta",array,
		"evid",evid,
		"bankid",bankid,
		"phase", phase,
		"fc",fc,
		"ampcomp",AMPCOMP,
		"time",time,
		"twin",twin,
		"mwamp",mwamp,
		"erramp",erramp,
		"ndgf",ndgf,
		"algorithm","mwap",0) < 0) 
	{
		elog_notify(0,
			"dbaddv error for mwavgamp table on evid %d with  fc=%lf\n",
				evid,fc);
		return(-1);
	}
	else
		return(0);
}
/* This function saves three results from mwap in three output extension
tables.  Quantities here are all related to static estimates, although
the one is done more out of convenience than logic.  That is, we produce
two static tables:  one for time statics and a second for relative 
amplitudes.  Note the amplitude statics are all relative amplitudes
passed in.  These are converted to db here before writing them out
using a standard formula. We also write a table holding signal to 
noise ratio estimates.  The computed time windows for these estimates
generally are different from those for the static tables.  Here is
the rather long argument list:

evid - css3.0 event id
bankid - multiwavelet group id 
phase - phase name as in css3.0
fc - center frequency (in Hz) of the wavelets used
timeref - reference start time.  This time needs to be lag corrected
	so the window defintion works correctly.  
w - time window structure used in mwap. Note timeref, w, and moveout
	(see below) all interact to define exact sliding windows 
	that vary from station to station.  The window start time 
	stored here could, in fact, be stored as arrival times although
	they would generally have a dc bias.  
refelev - reference elevation in km used for elevation static estimates
g - MWgather structure from which a lot of base information is extracted.
	In fact, the gather is parsed so every nonnull entry in the gather
	structure should yield a row in the output database for each call
	to this function.
moveout - relative moveout time vector (parallel to g entries) for
	each station
statics - result array of static estimates indexed by station
stations - array of MW station objects
snrarr - array of Signal_to_Noise objects indexed by sta (Warning
	mwap has a stack of these for each frequency band being analyzed.)
db - output database 

Author:  G Pavlis
Written:  March 2000
*/
 	
	
int MWdb_save_statics(
	int evid, 
	int bankid,
	char *phase,
	double fc,
	double timeref,
	Time_Window *w,
	double refelev,
	MWgather *g,
	double *moveout,
	Arr *statics,
	Arr *stations,
	Arr *snrarr,
	Dbptr db)
{
	char *sta;
	int i;
        MWstatic *mws;
	MWstation *s;
	Signal_to_Noise *snr;
	int errcount=0;
	double time, twin;
	int nsta;
	Dbptr dbt, dba, dbsnr;
	double ampdb, aerrdb;  

	dbt = dblookup(db,0,"mwtstatic",0,0);
	dba = dblookup(db,0,"mwastatic",0,0);
	dbsnr = dblookup(db,0,"mwsnr",0,0);

	nsta = g->nsta;
	twin = ((double)((w->tend)-(w->tstart)))*(w->si);

	for(i=0;i<nsta;++i)
	{
		/* We keep all snr estimates that can be found in the snarr
		array.  Station with low signal to noise ratio are automatically
		deleted in processing so it is useful to record this fact in
		the database in the mwsnr table.  Note we silently skip stations
		not found in the array for snr */
		sta = g->sta[i]->sta;
 		snr = (Signal_to_Noise *)getarr(snrarr,sta);
		if(snr != NULL)
		{
		    if( dbaddv(dbsnr,0,
			"sta",sta,
			"fc",fc,
			"bankid",bankid,
			"phase",phase,
			"evid",evid,
			"nstime",snr->nstime,
			"netime",snr->netime,
			"sstime",snr->sstime,
			"setime",snr->setime,
			"snrz",snr->ratio_z,
			"snrn",snr->ratio_n,
			"snre",snr->ratio_e,
			"snr3c",snr->ratio_3c,
			"algorithm","mwap",0) < 0) 
		    {
			elog_notify(0,"mwtstatic dbaddv error for station %s\n",sta);

			++errcount;
		    }
		}
		mws = (MWstatic *)getarr(statics,sta);

   		/* Again we silently skip stations without an entry in
		this array because they can auto-edited in processing
		so this is not an error. */
		if(mws != NULL)
                {
			s = (MWstation *)getarr(stations,sta);
			if(s == NULL)
			{
				elog_complain(0,"MWdb_save_time_statics cannot find entry for station %s\nThis should NOT happen--program may have ovewritten itself!\n",
					sta);
				continue;
			}
			/* We have to correct the start time for moveout.
			This asssumes the moveout vector has the current 
			best estimate */
			time = timeref 
				+ moveout[i] + ((double)(w->tstart))*(w->si);
		
			if( dbaddv(dbt,0,
				"sta",sta,
				"fc",fc,
				"bankid",bankid,
				"phase",phase,
				"evid",evid,
				"time",time,
				"twin",twin,
				"wgt",s->current_weight_base,
				"estatic",s->elevation_static,
				"pwstatic",s->plane_wave_static,
				"rstatic",s->residual_static,
				"errstatic",mws->sigma_t,
				"ndgf",mws->ndgf,
				"datum",refelev,
				"algorithm","mwap",0) < 0) 
			{
				elog_notify(0,"mwtstatic dbaddv error for station %s\n",sta);

				++errcount;
			}
			/* zero weight stations will have a static 
			time shift computed, but the amplitude will
			be meaningless.  Thus, we skip stations with 
			a zero weight */
			if((s->current_weight_base)>0.0)
			{
			/* output amplitude statics are converted to db */
			    ampdb = 20.0*(mws->log10amp);
			    aerrdb = 20.0*(mws->sigma_log10amp);
			    if( dbaddv(dba,0,
				"sta",sta,
				"ampcomp",AMPCOMP,
				"fc",fc,
				"bankid",bankid,
				"phase",phase,
				"evid",evid,
				"time",time,
				"twin",twin,
				"wgt",s->current_weight_base,
				"ndgf",mws->ndgf,
				"ampstatic",ampdb,
				"erramp",aerrdb,
				"algorithm","mwap",0) < 0) 
			    {
				elog_notify(0,"mwtstatic dbaddv error for station %s\n",sta);

				++errcount;
			    }
			}
			
		}
	}
	return(errcount);
}
/* This function saves particle motion parameter estimates to a extension
table called mwpm.  Individual station estimates and an array average
estimate are all saved in the same table.  they can be sorted out through
the key field pmtype set to "ss" for single station and "aa" for array average.

Arguments:

array - array name used as tag on the array average row
evid - css3.0 event id
bankid - multiwavelet bank id tag
phase - seismic phase name as in css3.0
fc - center frequency of band in hz.
timeref - window start time reference at reference station
w - time window structure for the band
g - MWgather structure for this band.  The routine loops through
	the list defined by this complicated structure.
moveout - moveout vector.  Elements of moveout are a parallel array
	to g->sta and related quanties in the gather structure.
pmarr - particle motion structures array indexed by station name
pmerrarr - particle motion error structure array indexed by station name
pmavg - particle motion ellipse parameters for array average
pmaerr - error parameters associated with pmavg
db - output database

Author:  G Pavlis
Written:  march 2000
*/
int MWdb_save_pm(
	char *array,
	int evid, 
	int bankid,
	char *phase,
	double fc,
	double timeref,
	Time_Window *w,
	MWgather *g,
	double *moveout,
	Arr *pmarr,
	Arr *pmerrarr,
	Particle_Motion_Ellipse *pmavg,
	Particle_Motion_Error *pmaerr,
	Dbptr db)
{

	char *sta;
	int i;
	Particle_Motion_Ellipse *pm;
	Particle_Motion_Error *pmerr;
	int errcount=0;
	double time, twin;
	int nsta;
	Spherical_Coordinate scoor;
	double majaz, majema, minaz, minema;

	db = dblookup(db,0,"mwpm",0,0);

	nsta = g->nsta;
	twin = ((double)((w->tend)-(w->tstart)))*(w->si);

	/* We look through the whole gather quietly skipping entries
	flagged bad with a null pointer */
	for(i=0;i<nsta;++i)
	{
                pm = (Particle_Motion_Ellipse *)getarr(pmarr,g->sta[i]->sta);
		pmerr = (Particle_Motion_Error *)getarr(pmerrarr,g->sta[i]->sta);
		/* Silently skip null entries because autoediting makes
		this happen often.  We could trap the condition where
		one of these pointers is null and the other is not, but
		this should not happen so I skip it.*/
		if( (pm != NULL) && (pmerr != NULL) )
                {
			/* We have to correct the start time for moveout.
			This asssumes the moveout vector has the current 
			best estimate */
			time = timeref 
				+ moveout[i] + ((double)(w->tstart))*(w->si);
			/* The pm structure stores the major and minor axes
			as unit vectors.  It is more compact and more 
			intuitive to store these quantities in spherical coord
			form (az and ema) in the database so we have to convert
			them.  NOte also all angles are stored internally
			in radians and need to be converted to degrees with
			the deg for external consumption. */
			scoor = unit_vector_to_spherical(pm->major);
			majaz = deg(scoor.phi);
			majema = deg(scoor.theta);
			scoor = unit_vector_to_spherical(pm->minor);
			minaz = deg(scoor.phi);
			minema = deg(scoor.theta);
			if( dbaddv(db,0,
				"sta",g->sta[i]->sta,
				"bankid",bankid,
				"fc",fc,
				"phase",phase,
				"evid",evid,
				"time",time,
				"twin",twin,
				"pmtype","ss",
				"majoraz",majaz,
				"majorema",majema,
				"minoraz",minaz,
				"minorema",minema,
				"rect",pm->rectilinearity,
				"errmajaz",deg(pmerr->dphi_major),
				"errmajema",deg(pmerr->dtheta_major),
				"errminaz",deg(pmerr->dphi_minor),
				"errminema",deg(pmerr->dtheta_minor),
				"errrect",pmerr->delta_rect,
				"majndgf",pmerr->ndgf_major,
				"minndgf",pmerr->ndgf_minor,
				"rectndgf",pmerr->ndgf_rect,
				"algorithm","mwap",0) < 0) 
			{
				elog_notify(0,"dbaddv error in mwpm table for station %s\n",sta);

				++errcount;
			}
		}
	}
	/* now we add a row for the array average.  This is flagged only
	by the pmtype field.  */
	scoor = unit_vector_to_spherical(pmavg->major);
	majaz = deg(scoor.phi);
	majema = deg(scoor.theta);
	scoor = unit_vector_to_spherical(pmavg->minor);
	minaz = deg(scoor.phi);
	minema = deg(scoor.theta);
	if( dbaddv(db,0,
		"sta",array,
		"bankid",bankid,
		"fc",fc,
		"phase",phase,
		"evid",evid,
		"time",time,
		"twin",twin,
		"pmtype","aa",
		"majoraz",majaz,
		"majorema",majema,
		"minoraz",minaz,
		"minorema",minema,
		"rect",pmavg->rectilinearity,
		"errmajaz",deg(pmaerr->dphi_major),
		"errmajema",deg(pmaerr->dtheta_major),
		"errminaz",deg(pmaerr->dphi_minor),
		"errminema",deg(pmaerr->dtheta_minor),
		"errrect",pmaerr->delta_rect,
		"majndgf",pmaerr->ndgf_major,
		"minndgf",pmaerr->ndgf_minor,
		"rectndgf",pmaerr->ndgf_rect,
	   "algorithm","mwap",0) < 0) 
	{
		elog_notify(0,"dbaddv error saving array average particle motion parameters in mwpm table for evid %d\n",
			evid);
		++errcount;
	}
	return(errcount);
}

/* This function loads a set of MWbasis objects from an input
database.  The name of the database is passed through the parameter
space.  It is functionally similar to load_multiwavelets_pf.

Arguments:

pf - parameter object
nwavelets - returned as the number of wavelets found in the bank requested
	through definitions in pf
bankid - an id tag on this bank of multiwavelet functions  (returned).

*/

MWbasis *load_multiwavelets_db(Pf *pf,int *nwavelets, int *bankid)
{
	MWbasis *mwb;
	die(0,"db input for multiwavelets not yet implemented--use parameter file form\n");
	return(mwb);
}
