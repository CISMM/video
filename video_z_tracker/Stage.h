/////////////////////////////////////////////////////////////////////////////
// Name:        Stage.h
// Purpose:     stores information about a virtual rep. of a microscope stage
// Author:      Ryan Schubert
// Created:     3/18/07
// Modified by:
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "vrpn_Analog.h"
#include "vrpn_Analog_Output.h"

#include "vrpn_Connection.h"
#include "vrpn_Tracker.h"
#include "vrpn_Poser.h"
#include "vrpn_Generic_server_object.h"

//#define FAKE_STAGE

// units in microns(?)
static const double MIN_X = 0;
static const double MIN_Y = 0;
static const double MIN_Z = 0;

static const double MAX_X = 100;
static const double MAX_Y = 100;
static const double MAX_Z = 100;

// units in microns/sec (?)
static const double MAX_X_RATE = 1;
static const double MAX_Y_RATE = 1;
static const double MAX_Z_RATE = 0.5;


// The number of meters per micron.
static const double  METERS_PER_MICRON = 1e-6;


class Stage
{
public:
	Stage(char* Stage_name);
	~Stage();

	void Update();

	bool MoveTo(double x, double y, double z);
	void GetPosition(double &x, double &y, double &z);
	void SetPosition(double x, double y, double z);
	void GetTargetPosition(double &x, double &y, double &z);

	void SetX(double x) { m_x = x; }
	void SetY(double y) { m_y = y; }
	void SetZ(double z) { m_z = z; }
	void SetFocusChanged(bool changed) { m_focus_changed = changed; }

	void CalculateOffset(double z_meters, double x_meters, double y_meters);

protected:

	static void VRPN_CALLBACK handle_vrpn_focus_change(void *, const vrpn_TRACKERCB info);


	double m_x, m_y, m_z;
	double m_lastX, m_lastY, m_lastZ;
	double m_targetX, m_targetY, m_targetZ;

	struct timeval m_lastTime;

	bool m_focus_changed;

	double m_zOffset, m_xOffset, m_yOffset;

#ifndef FAKE_STAGE
	// vrpn stuff for controlling the MCL stage
	vrpn_Connection	*con;
	vrpn_Generic_Server_Object *svr;

	vrpn_Tracker_Remote *read_z;
	vrpn_Poser_Remote *set_z;
#endif

};