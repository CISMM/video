/////////////////////////////////////////////////////////////////////////////
// Name:        Stage.h
// Purpose:     stores information about a virtual rep. of a microscope stage
// Author:      Ryan Schubert
// Created:     3/18/07
// Modified by:
/////////////////////////////////////////////////////////////////////////////

#pragma once


static const float MIN_X = 0;
static const float MIN_Y = 0;
static const float MIN_Z = 0;

static const float MAX_X = 1000;
static const float MAX_Y = 1000;
static const float MAX_Z = 100;

static const float MAX_X_STEP = 1;
static const float MAX_Y_STEP = 1;
static const float MAX_Z_STEP = 0.1;


class Stage
{
public:
	Stage();
	~Stage();

	void Update(float t);

	bool MoveTo(float x, float y, float z);
	void GetPosition(float &x, float &y, float &z);
	void GetTargetPosition(float &x, float &y, float &z);

protected:
	float m_x, m_y, m_z;
	float m_lastX, m_lastY, m_lastZ;
	float m_targetX, m_targetY, m_targetZ;

private:


};