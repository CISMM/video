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


Stage::Stage()
{
	m_x = m_y = m_z = 0;
	m_lastX = m_lastY = m_lastZ = 0;
	m_targetX = m_targetY = m_targetZ = 0;

	vrpn_gettimeofday(&m_lastTime, NULL);
}

Stage::~Stage()
{
}


void Stage::Update()
{
	// TODO: add rate-controlled stage movement

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
}

bool Stage::MoveTo(float x, float y, float z)
{
	if (x < MIN_X || x > MAX_X || y < MIN_Y || y > MAX_Y || z < MIN_Z || z > MAX_Z)
		return false; // illegal stage position

	m_targetX = x;
	m_targetY = y;
	m_targetZ = z;

	return true;
}

void Stage::GetPosition(float &x, float &y, float &z)
{
	x = m_x;
	y = m_y;
	z = m_z;
}

void Stage::SetPosition(float x, float y, float z)
{
	m_x = x;
	m_y = y;
	m_z = z;

	m_targetX = x;
	m_targetY = y;
	m_targetZ = z;
}

void Stage::GetTargetPosition(float &x, float &y, float &z)
{
	x = m_targetX;
	y = m_targetY;
	z = m_targetZ;
}