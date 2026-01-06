#pragma once
#include "Vector.h"
#include "includes.h"
#include <vector>

// Forward declaration
namespace SDK {
    class GameObject;
}

namespace Render
{
	// Matrix globals
	extern float viewMatrix[16];
	extern float projMatrix[16];
	extern float viewProjMatrix[16];

	// Screen dimensions
	extern int g_screenWidth;
	extern int g_screenHeight;

	// Circle drawing tables
	extern size_t circlePoints;
	extern std::vector<float> vSinTable;
	extern std::vector<float> vCosTable;

	// Matrix operations
	void MultiplyMatrices(float* out, float* a, int row1, int col1, float* b, int row2, int col2);

	// World to screen
	Vector2 WorldToScreen(const Vector3& pos);

	// Drawing functions
	void InitCircle();
	void DrawCircle3D(ImDrawList* draw, Vector3 vPos, float flPoints, float flRadius, ImU32 clrColor, float flThickness = 1.f);
	void DrawCirclee(ImDrawList* canvas, const Vector3& worldPos, float radius, bool filled, int numPoints, ImColor color, float thickness);
	void DrawLine(ImDrawList* drawList, float x1, float y1, float x2, float y2, float thickness, float r, float g, float b, float a);
	void DrawCircleAt(ImDrawList* draw, const Vector3 worldPos, float radius, bool filled, int numPoints, float thickness, ImColor Col);
	
	// Skillshot drawing (Evade)
	void DrawRectangle3D(ImDrawList* draw, const Vector3& startPos, const Vector3& endPos, float width, ImU32 fillColor, ImU32 borderColor, float thickness = 2.0f);
	
	// Debug drawing
	void DrawAiManagerDebug(ImDrawList* draw, SDK::GameObject* obj);
}
