#pragma once

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <cstdlib>  
#include <ctime>    


struct Vec3
{
	float x = 0.0;
	float y = 0.0;
	float z = 0.0;

	Vec3() {}
	Vec3(float s) : x(s), y(s), z(s) {}
	Vec3(float x, float y) : x(x), y(y), z(0.0) {}
	Vec3(float x, float y, float z) : x(x), y(y), z(z) {}


	static Vec3 from_spherical(float theta = 0.0, float phi = 0.0)
	{
		float ct = std::cosf(theta);
		float st = std::sinf(theta);
		float cp = std::cosf(phi);
		float sp = std::sinf(phi);



		float x_ = -st * cp;
		float y_ = sp;
		float z_ = ct * cp;

		return Vec3(x_, y_, z_).normalized();
	}

	static Vec3 random1()
	{
		while (true)
		{
			float x = ((float)rand() / (RAND_MAX));
			float y = ((float)rand() / (RAND_MAX));
			float z = ((float)rand() / (RAND_MAX));
			x -= 0.5;
			x *= 2.0;
			y -= 0.5;
			y *= 2.0;
			z -= 0.5;
			z *= 2.0;

			if (Vec3(x, y, z).length() <= 1.0)
			{
				return Vec3(x, y, z).normalized();
				break;
			}
		}
	}

	Vec3& ApplyRotateTransformes(Vec3 orientation)
	{
		float cy = std::cosf(orientation.y); float sy = std::sinf(orientation.y);
		float new_x = x * cy - z * sy;
		float new_z = x * sy + z * cy;

		float cx = std::cosf(orientation.x); float sx = std::sinf(orientation.x);
		float new_y = y * cx - new_z * sx;
		new_z = y * sx + new_z * cx;

		float cz = std::cosf(orientation.z); float sz = std::sinf(orientation.z);
		float new_new_x = new_x * cz - new_y * sz;
		new_y = new_x * sz + new_y * cz;

		x = new_new_x;
		y = new_y;
		z = new_z;


		return *this;
	}

	Vec3 cross(const Vec3& other) const
	{
		Vec3 ans;
		ans.x = this->y * other.z - this->z * other.y;
		ans.y = this->z * other.x - this->x * other.z;
		ans.z = this->x * other.y - this->y * other.x;

		return ans;
	}

	float dot(const Vec3& other) const
	{
		return this->x * other.x + this->y * other.y + this->z * other.z;
	}

	float length2() const
	{
		return this->dot(*this);
	}

	float length() const
	{
		return std::sqrtf(this->length2());
	}

	Vec3 normalized() const
	{
		if (this->length() == 0) { return *this; }
		return *this / this->length();
	}

	Vec3& normalize()
	{
		*this = this->normalized();
		return *this;
	}

	Vec3 operator+(const Vec3& other) const
	{
		return Vec3(this->x + other.x, this->y + other.y, this->z + other.z);
	}

	Vec3& operator+=(const Vec3& other)
	{
		*this = *this + other;
		return *this;
	}

	Vec3 operator+(float other) const
	{
		return Vec3(this->x + other, this->y + other, this->z + other);
	}

	Vec3& operator+=(float other)
	{
		*this = *this + other;
		return *this;
	}

	Vec3 operator-(const Vec3& other) const
	{
		return Vec3(this->x - other.x, this->y - other.y, this->z - other.z);
	}

	Vec3& operator-=(const Vec3& other)
	{
		*this = *this - other;
		return *this;
	}

	Vec3 operator*(const Vec3& other) const
	{
		return Vec3(this->x * other.x, this->y * other.y, this->z * other.z);
	}

	Vec3& operator*=(const Vec3& other)
	{
		*this = *this * other;
		return *this;
	}

	Vec3 operator*(const float& other) const
	{
		return Vec3(this->x * other, this->y * other, this->z * other);
	}

	Vec3& operator*=(const float& other)
	{
		*this = *this * other;
		return *this;
	}

	Vec3 operator/(float other) const
	{
		if (other == 0.0f)
		{
			throw std::runtime_error("cant divide by zero!!! Vec3 class");
		}

		return *this * (1.0f / other);
	}

	Vec3& operator/=(const float& other)
	{
		*this = *this / other;
		return *this;
	}

	bool operator==(const Vec3& other)
	{
		return (x == other.x) && (y = other.y) && (z == other.z);
	}
};

struct Vec4
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float w = 0.0f;

	Vec4() {}
	Vec4(float s) : x(s), y(s), z(s), w(s) {}
	Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}



	float dot(const Vec4& other) const
	{
		return x * other.x + y * other.y + z * other.z + w * other.w;
	}

	float length2() const { return dot(*this); }
	float length()  const { return std::sqrt(length2()); }

	Vec4 normalized() const
	{
		float len = length();
		if (len == 0.0f) return *this;
		return *this / len;
	}

	Vec4& normalize()
	{
		*this = normalized();
		return *this;
	}


	Vec4 project_tangent(const Vec4& p) const
	{
		return *this - p * dot(p);
	}


	void rotate_with(Vec4& b, float angle)
	{
		float c = std::cos(angle);
		float s = std::sin(angle);

		Vec4 a_new = (*this) * c + b * s;
		Vec4 b_new = b * c - (*this) * s;

		*this = a_new;
		b = b_new;
	}


	Vec4 operator+(const Vec4& o) const { return Vec4(x + o.x, y + o.y, z + o.z, w + o.w); }
	Vec4 operator-(const Vec4& o) const { return Vec4(x - o.x, y - o.y, z - o.z, w - o.w); }
	Vec4 operator*(const Vec4& o) const { return Vec4(x * o.x, y * o.y, z * o.z, w * o.w); }

	Vec4 operator*(float s) const { return Vec4(x * s, y * s, z * s, w * s); }

	Vec4 operator/(float s) const
	{
		if (s == 0.0f) throw std::runtime_error("Vec4 divide by zero");
		return (*this) * (1.0f / s);
	}

	Vec4& operator+=(const Vec4& o) { *this = *this + o; return *this; }
	Vec4& operator-=(const Vec4& o) { *this = *this - o; return *this; }
	Vec4& operator*=(float s) { *this = *this * s; return *this; }
	Vec4& operator/=(float s) { *this = *this / s; return *this; }

};