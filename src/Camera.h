#pragma once

#include "Vector.h"


struct Camera
{
	Camera() {};
	~Camera() {};

	float speed = 1;
	Vec4 pos = Vec4(0, 0,0,1);
	Vec4 front = Vec4(0,0,1,0);
	Vec4 right = Vec4(1,0,0,0);
	Vec4 up = Vec4(0,1,0,0);

	void move_forward(float dt)
	{
		Vec4 new_p = pos * cos(dt) + front * sin(dt);
		Vec4 new_front = front * cos(dt)- pos * sin(dt);

		pos = new_p.normalized();
		front = new_front.normalized();
	}

	void move_right(float dt)
	{
		Vec4 new_p = pos * cos(dt) + right * sin(dt);
		Vec4 new_right = right * cos(dt) - pos * sin(dt);

		pos = new_p.normalized();
		right = new_right.normalized();
	}

	void move_up(float dt)
	{
		Vec4  new_p = pos * cos(dt) + up * sin(dt);
		Vec4 new_up = up * cos(dt) - pos * sin(dt);

		pos = new_p.normalized();
		up = new_up.normalized();
	}

	void yaw(float dt)
	{
		Vec4 new_front = front * cos(dt) + right * sin(dt);
		Vec4 new_right = right * cos(dt) - front * sin(dt);

		front = new_front.normalized();
		right = new_right.normalized();
	}

	void pitch(float dt)
	{
		Vec4 new_up = up * cos(dt) + front  * sin(dt);
		Vec4 new_front = front  * cos(dt) - up * sin(dt);

		front = new_front.normalized();
		up = new_up.normalized();
	}


};