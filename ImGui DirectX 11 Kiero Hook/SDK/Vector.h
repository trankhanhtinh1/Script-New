#pragma once
#include <cmath>
#include <limits>

struct Vector2
{
	float x, y;

	Vector2() : x(0.0f), y(0.0f) {}
	Vector2(float _x, float _y) : x(_x), y(_y) {}

	Vector2 operator+(const Vector2& rhs) const { return Vector2(x + rhs.x, y + rhs.y); }
	Vector2 operator-(const Vector2& rhs) const { return Vector2(x - rhs.x, y - rhs.y); }
	Vector2 operator*(float scalar) const { return Vector2(x * scalar, y * scalar); }
	Vector2 operator/(float scalar) const { return Vector2(x / scalar, y / scalar); }
	bool operator==(const Vector2& rhs) const { return x == rhs.x && y == rhs.y; }
};

struct Vector3
{
	float x, y, z;

	Vector3() : x(0.0f), y(0.0f), z(0.0f) {}
	Vector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

	bool IsValid() const
	{
		return !(this->x == 0.0f && this->y == 0.0f && this->z == 0.0f) && 
            (this->x > (-1000000)) && (this->y > (-1000000)) && (this->z > (-1000000)) && 
            (this->x < (1000000)) && (this->y < (1000000)) && (this->z < (1000000));
	}

	Vector3 operator+(const Vector3& rhs) const { return Vector3(x + rhs.x, y + rhs.y, z + rhs.z); }
	Vector3 operator-(const Vector3& rhs) const { return Vector3(x - rhs.x, y - rhs.y, z - rhs.z); }
	Vector3 operator*(float scalar) const { return Vector3(x * scalar, y * scalar, z * scalar); }
	Vector3 operator/(float scalar) const { return Vector3(x / scalar, y / scalar, z / scalar); }
	bool operator==(const Vector3& rhs) const { return x == rhs.x && y == rhs.y && z == rhs.z; }

	Vector3 Extend(const Vector3& to, float distance) const
	{
		const auto from = *this;
		const auto result = from + (to - from).Normalized() * distance;
		return result;
	}

	float Distance(const Vector3& to) const
	{
		return sqrtf(powf(to.x - x, 2) + powf(to.z - z, 2) + powf(to.y - y, 2));
	}

	float Length() const
	{
		return sqrtf(this->x * this->x + this->y * this->y + this->z * this->z);
	}

	float DotProduct(const Vector3& other) const
	{
		return this->x * other.x + this->y * other.y + this->z * other.z;
	}

	Vector3 Normalized() const
	{
		auto const length = this->Length();
		if (length != 0)
		{
			auto const inv = 1.0f / length;
			return { this->x * inv, this->y * inv, this->z * inv };
		}
		return *this;
	}
};
