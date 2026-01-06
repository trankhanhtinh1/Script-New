#pragma once
#include <cmath>
#include <algorithm>

// Forward declaration - ImVec2 will be included from imgui
struct ImVec2;

struct Vector2
{
	float x, y;

	Vector2() : x(0.0f), y(0.0f) {}
	Vector2(float _x, float _y) : x(_x), y(_y) {}

	Vector2 operator+(const Vector2& rhs) const
	{
		return Vector2(x + rhs.x, y + rhs.y);
	}

	Vector2 operator-(const Vector2& rhs) const
	{
		return Vector2(x - rhs.x, y - rhs.y);
	}

	Vector2 operator*(float scalar) const
	{
		return Vector2(x * scalar, y * scalar);
	}

	Vector2 operator/(float scalar) const
	{
		return Vector2(x / scalar, y / scalar);
	}

	bool operator==(const Vector2& rhs) const
	{
		return x == rhs.x && y == rhs.y;
	}
    
    float Length() const {
        return sqrtf(x * x + y * y);
    }
    
    Vector2 Normalized() const {
        float len = Length();
        if (len > 0.0001f) return Vector2(x / len, y / len);
        return *this;
    }
    
    float Distance(const Vector2& to) const {
        return (*this - to).Length();
    }
    
    // Rotate vector by angle (radians)
    Vector2 Rotated(float angle) const {
        float c = cosf(angle);
        float s = sinf(angle);
        return Vector2(x * c - y * s, y * c + x * s);
    }
    
    // Perpendicular vector
    Vector2 Perpendicular() const {
        return Vector2(-y, x);
    }
};

struct Vector3
{
	float x, y, z;

	Vector3() : x(0.0f), y(0.0f), z(0.0f) {}
	Vector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

	bool IsValid() const
	{
		return !(this->x == 0.0f && this->y == 0.0f && this->z == 0.0f) && (this->x > (-1000000)) && (this->y > (-1000000)) && (this->z > (-1000000)) && (this->x < (1000000)) && (this->y < (1000000)) && (this->z < (1000000));
	}

	Vector3 operator+(const Vector3& rhs) const
	{
		return Vector3(x + rhs.x, y + rhs.y, z + rhs.z);
	}

	Vector3 operator-(const Vector3& rhs) const
	{
		return Vector3(x - rhs.x, y - rhs.y, z - rhs.z);
	}

	Vector3 operator*(float scalar) const
	{
		return Vector3(x * scalar, y * scalar, z * scalar);
	}

	Vector3 operator/(float scalar) const
	{
		return Vector3(x / scalar, y / scalar, z / scalar);
	}

	bool operator==(const Vector3& rhs) const
	{
		return x == rhs.x && y == rhs.y && z == rhs.z;
	}

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
    
    // ========================================================================
    // EXTENDED VECTOR METHODS (ported from EnsoulSharp.SDK)
    // ========================================================================
    
    // Length in XZ plane (2D for game calculations)
    float Length2D() const {
        return sqrtf(x * x + z * z);
    }
    
    // Distance in XZ plane (ignores Y/height)
    float Distance2D(const Vector3& to) const {
        float dx = to.x - x;
        float dz = to.z - z;
        return sqrtf(dx * dx + dz * dz);
    }
    
    // Squared distance (faster, for comparisons)
    float DistanceSquared(const Vector3& to) const {
        float dx = to.x - x;
        float dy = to.y - y;
        float dz = to.z - z;
        return dx * dx + dy * dy + dz * dz;
    }
    
    float DistanceSquared2D(const Vector3& to) const {
        float dx = to.x - x;
        float dz = to.z - z;
        return dx * dx + dz * dz;
    }
    
    // Rotate around Y axis (XZ plane) by angle in radians
    Vector3 Rotated(float angle) const {
        float c = cosf(angle);
        float s = sinf(angle);
        return Vector3(x * c - z * s, y, z * c + x * s);
    }
    
    // Perpendicular vector in XZ plane
    Vector3 Perpendicular() const {
        return Vector3(-z, y, x);
    }
    
    // Angle between two vectors (in degrees)
    float AngleBetween(const Vector3& other) const {
        float len1 = Length2D();
        float len2 = other.Length2D();
        if (len1 < 0.0001f || len2 < 0.0001f) return 0.0f;
        
        float dot = (x * other.x + z * other.z) / (len1 * len2);
        dot = std::max(-1.0f, std::min(1.0f, dot));
        return acosf(dot) * 180.0f / 3.14159265f;
    }
    
    // Cross product (2D, returns scalar - useful for determining side)
    float CrossProduct2D(const Vector3& other) const {
        return x * other.z - z * other.x;
    }
    
    // Convert to Vector2 (XZ plane)
    Vector2 ToVector2() const {
        return Vector2(x, z);
    }
    
    // Create from Vector2 with Y height
    static Vector3 FromVector2(const Vector2& v2, float height = 0.0f) {
        return Vector3(v2.x, height, v2.y);
    }
    
    // Normalized in 2D (XZ plane, preserves Y)
    Vector3 Normalized2D() const {
        float len = Length2D();
        if (len > 0.0001f) {
            return Vector3(x / len, y, z / len);
        }
        return *this;
    }
    
    // Project onto line segment, returns closest point on segment
    Vector3 ProjectOn(const Vector3& lineStart, const Vector3& lineEnd) const {
        Vector3 line = lineEnd - lineStart;
        float lineLenSq = line.x * line.x + line.z * line.z;
        
        if (lineLenSq < 0.0001f) return lineStart;
        
        float t = ((x - lineStart.x) * line.x + (z - lineStart.z) * line.z) / lineLenSq;
        t = std::max(0.0f, std::min(1.0f, t));
        
        return lineStart + line * t;
    }
    
    // Distance to line segment
    float DistanceToLineSegment(const Vector3& lineStart, const Vector3& lineEnd) const {
        Vector3 closest = ProjectOn(lineStart, lineEnd);
        return Distance2D(closest);
    }
    
    // Check if point is on segment
    bool IsOnSegment(const Vector3& lineStart, const Vector3& lineEnd) const {
        float segmentLen = lineStart.Distance2D(lineEnd);
        float d1 = Distance2D(lineStart);
        float d2 = Distance2D(lineEnd);
        return fabsf((d1 + d2) - segmentLen) < 1.0f;  // 1 unit tolerance
    }
};

