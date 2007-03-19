/////////////////////////////////////////////////////////////////////////////
// Name:        Stage.cpp
// Purpose:     stores information about a virtual rep. of a microscope stage
// Author:      Ryan Schubert
// Created:     3/18/07
// Modified by:
/////////////////////////////////////////////////////////////////////////////


#include "Stage.h"

Stage::Stage()
{
	m_x = m_y = m_z = 0;
	m_lastX = m_lastY = m_lastZ = 0;
	m_targetX = m_targetY = m_targetZ = 0;
}

Stage::~Stage()
{
}


void Stage::Update(float t)
{
	// TODO: add rate-controlled stage movement
	m_lastX = m_x;
	m_lastY = m_y;
	m_lastZ = m_z;
}

bool Stage::MoveTo(float x, float y, float z)
{
	// TODO: do some sort of bounds checking...

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

void Stage::GetTargetPosition(float &x, float &y, float &z)
{
	x = m_targetX;
	y = m_targetY;
	z = m_targetZ;
}