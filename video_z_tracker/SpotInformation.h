/////////////////////////////////////////////////////////////////////////////
// Name:        SpotInformation.h
// Purpose:     stores information for SpotTracker
// Author:      Ryan Schubert
// Created:     3/28/07
// Modified by:
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "spot_tracker.h"

class Spot_Information {
public:
	Spot_Information(spot_tracker_XY *xytracker, spot_tracker_Z *ztracker) {
		d_tracker_XY = xytracker;
		d_tracker_Z = ztracker;
		d_index = d_static_index++;
		d_velocity[0] = d_acceleration[0] = d_velocity[1] = d_acceleration[1] = 0;
	}

	spot_tracker_XY *xytracker(void) const { return d_tracker_XY; }
	spot_tracker_Z *ztracker(void) const { return d_tracker_Z; }
	unsigned index(void) const { return d_index; }

	void set_xytracker(spot_tracker_XY *tracker) { d_tracker_XY = tracker; }
	void set_ztracker(spot_tracker_Z *tracker) { d_tracker_Z = tracker; }

	void get_velocity(double velocity[2]) const { velocity[0] = d_velocity[0]; velocity[1] = d_velocity[1]; }
	void set_velocity(const double velocity[2]) { d_velocity[0] = velocity[0]; d_velocity[1] = velocity[1]; }
	void get_acceleration(double acceleration[2]) const { acceleration[0] = d_acceleration[0]; acceleration[1] = d_acceleration[1]; }
	void set_acceleration(const double acceleration[2]) { d_acceleration[0] = acceleration[0]; d_acceleration[1] = acceleration[1]; }

protected:
	spot_tracker_XY	*d_tracker_XY;	    //< The tracker we're keeping information for in XY
	spot_tracker_Z	*d_tracker_Z;	    //< The tracker we're keeping information for in Z
	unsigned		d_index;	    //< The index for this instance
	double		d_velocity[2];	    //< The velocity of the particle
	double		d_acceleration[2];  //< The acceleration of the particle
	static unsigned	d_static_index;     //< The index to use for the next one (never to be re-used).
};


