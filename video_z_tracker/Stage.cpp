/////////////////////////////////////////////////////////////////////////////
// Name:        Stage.cpp
// Purpose:     stores information about a virtual rep. of a microscope stage
// Author:      Ryan Schubert
// Created:     3/18/07
// Modified by:
/////////////////////////////////////////////////////////////////////////////


#include "Stage.h"


unsigned long	duration(struct timeval t1, struct timeval t2)
{
	return (t1.tv_usec - t2.tv_usec) +
	       1000000L * (t1.tv_sec - t2.tv_sec);
}


double clamp(double val, double min, double max)
{
	if (val < min)
		val = min;
	else if (val > max)
		val = max;

	return val;
}


Stage::Stage(char* Stage_name)
{
	m_x = m_y = m_z = 0;
	m_lastX = m_lastY = m_lastZ = 0;
	m_targetX = m_targetY = m_targetZ = 0;

	vrpn_gettimeofday(&m_lastTime, NULL);

	m_zOffset = 0;
	m_xOffset = 0;
	m_yOffset = 0;

#ifndef	FAKE_STAGE

//	char  *config_file_name = "mcl.cfg";
//	char	*Stage_name = "Focus@mercury-cs.cs.unc.edu";
	bool	use_local_server = false;
	con = NULL;
	svr = NULL;
	read_z = NULL;
	set_z = NULL;

	if (use_local_server) {
/* *** NOT SUPPORTED YET ***
		// Open a connection on which to talk between the server and client
		// portions of the code.  Then, start a VRPN generic server object
		// whose job it is to set up a Tracker to report stage position (Z) in
		// meters and a Poser whose job it is to move the stage in Z in
		// meters.  Both servers should have the name "Focus".  Other
		// auxilliary servers may be needed as well.
		con = new vrpn_Connection();

		svr = new vrpn_Generic_Server_Object(con, config_file_name);
		Stage_name = "Focus";
*/
	}

	if (Stage_name != "")
	{
		printf("Connecting to stage control server: %s...\n", Stage_name);

		read_z = new vrpn_Tracker_Remote(Stage_name, con);
		set_z = new vrpn_Poser_Remote(Stage_name, con);
		read_z->register_change_handler(this, handle_vrpn_focus_change);

		printf("Done.\n");
	}

#endif

}

Stage::~Stage()
{
}


void Stage::Update()
{

#ifdef FAKE_STAGE
	struct timeval currentTime;
    vrpn_gettimeofday(&currentTime, NULL);

	unsigned long usecElapsed = duration(currentTime, m_lastTime);
//	printf("seconds elapsed: %f\n", usecElapsed / 1000000.0f);

	double secElapsed = usecElapsed / 1000000.0f;


	m_lastTime = currentTime;

	m_lastX = m_x;
	m_lastY = m_y;
	m_lastZ = m_z;


	double delta;

	bool moved = false;


	// handle x stage movement
	if (m_x != m_targetX)
	{
		delta = secElapsed * MAX_X_RATE;
		if (abs(m_x - m_targetX) < delta)
		{
			m_x = m_targetX;
		}
		else if (m_targetX < m_x)
		{
			m_x -= delta;
		}
		else
		{
			m_x += delta;
		}
		moved = true;
	}

	// handle y stage movement
	if (m_y != m_targetY)
	{
		delta = secElapsed * MAX_Y_RATE;
		if (abs(m_y - m_targetY) < delta)
		{
			m_y = m_targetY;
		}
		else if (m_targetY < m_y)
		{
			m_y -= delta;
		}
		else
		{
			m_y += delta;
		}
		moved = true;
	}

	// handle z stage movement
	if (m_z != m_targetZ)
	{
		delta = secElapsed * MAX_Z_RATE;
		if (abs(m_z - m_targetZ) < delta)
		{
			m_z = m_targetZ;
		}
		else if (m_targetZ < m_z)
		{
			m_z -= delta;
		}
		else
		{
			m_z += delta;
		}
		moved = true;
	}

	/*
	if (moved)
		printf("stage moved to: (%f, %f, %f)\n", m_x, m_y, m_z);
	//*/
#else

	// update vrpn stuff
	if (svr) { svr->mainloop(); }
	if (con) { con->mainloop(); }
	if (read_z) { read_z->mainloop(); }

#endif
}

bool Stage::MoveTo(double x, double y, double z)
{
	//if (x < MIN_X || x > MAX_X || y < MIN_Y || y > MAX_Y || z < MIN_Z || z > MAX_Z)
	//	return false; // illegal stage position
	
	x = clamp(x, MIN_X, MAX_X);
	y = clamp(y, MIN_Y, MAX_Y);
	z = clamp(z, MIN_Z, MAX_Z);

	m_targetX = x;
	m_targetY = y;
	m_targetZ = z;

#ifndef FAKE_STAGE
	// Send the request for a new position in meters to the Poser.
	vrpn_float64  pos[3] = { (x - m_xOffset) * METERS_PER_MICRON,
							 (y - m_yOffset) * METERS_PER_MICRON,
							 (z - m_zOffset) * METERS_PER_MICRON };
	vrpn_float64  quat[4] = { 0, 0, 0, 1 };
	struct timeval now;
	gettimeofday(&now, NULL);
	if (set_z)
	{
		set_z->request_pose(now, pos, quat);
	}
#endif

	return true;
}

void Stage::GetPosition(double &x, double &y, double &z)
{
	x = m_x;
	y = m_y;
	z = m_z;
}

// SetPosition does nothing for a real MCL stage--must use MoveTo
void Stage::SetPosition(double x, double y, double z)
{
#ifdef FAKE_STAGE
	m_x = x;
	m_y = y;
	m_z = z;

	m_targetX = x;
	m_targetY = y;
	m_targetZ = z;
#endif
}

void Stage::GetTargetPosition(double &x, double &y, double &z)
{
	x = m_targetX;
	y = m_targetY;
	z = m_targetZ;
}


void Stage::CalculateOffset(double z_meters, double x_meters = 0, double y_meters = 0) 
{
#ifndef	FAKE_STAGE
	if (set_z)
	{
		// Request that the camera focus go where we want it to
		vrpn_float64  pos[3] = { x_meters, y_meters, z_meters };
		vrpn_float64  quat[4] = { 0, 0, 0, 1 };
		struct timeval now;
		gettimeofday(&now, NULL);
		set_z->request_pose(now, pos, quat);

		// Wait at least a quarter of a second and then get a new focus report.
		// Subtract it from the requested focus each time when we ask
		// for one later.  This is to deal with an offset between
		// the requested position and the found position because the
		// Mad City Labs stage (at least) has an offset between them.
		set_z->mainloop();
		if (svr) 
		{ 
			svr->mainloop(); 
		}
		vrpn_SleepMsecs(250);

		// Wait until we get at least one response for focus.
		m_focus_changed = false;
		do {
			if (svr) { svr->mainloop(); }
			if (con) { con->mainloop(); }
			set_z->mainloop();
			read_z->mainloop();
			vrpn_SleepMsecs(1);

			m_zOffset = m_z - (z_meters / METERS_PER_MICRON);

			m_xOffset = m_x - (x_meters / METERS_PER_MICRON);
			m_yOffset = m_y - (y_meters / METERS_PER_MICRON);

		} while (!m_focus_changed);

		//printf("Estimated focus offset at %f\n", m_zOffset);
		printf("Estimated stage offsets: (%f, %f, %f)\n", m_xOffset, m_yOffset, m_zOffset);
	}
#endif
}


//--------------------------------------------------------------------------
// Handles updates for the Stage from the microscope by setting the
// focus to that location (in microns) and telling that the focus has changed.

void VRPN_CALLBACK Stage::handle_vrpn_focus_change(void *stg, const vrpn_TRACKERCB info)
{
	if (info.sensor == 0) {
		double sensedZ = info.pos[2] / METERS_PER_MICRON;
		double sensedX = info.pos[0] / METERS_PER_MICRON;
		double sensedY = info.pos[1] / METERS_PER_MICRON;

//		printf("sensedZ = %f\n", sensedZ);
		if (sensedZ < 65535)
		{
			((Stage*)stg)->SetZ( sensedZ );
			((Stage*)stg)->SetFocusChanged(true);
		}

		if (sensedX < 65535)
			((Stage*)stg)->SetX(sensedX);

		if (sensedY < 65535)
			((Stage*)stg)->SetY(sensedY);
	}
}